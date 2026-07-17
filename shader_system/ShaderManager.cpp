#include "ShaderManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int64_t fileWriteTime(const std::string& path) {
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    if (ec) return -1;
    return t.time_since_epoch().count();
}

// Very small non-crypto hash used for cache keys.
static uint64_t fnv1a(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// ---------------------------------------------------------------------------
// ShaderManager
// ---------------------------------------------------------------------------

ShaderManager::ShaderManager(GraphicsAPI api)
    : api_(api), compiler_(ShaderCompilerFactory::create(api)) {}

ShaderManager::~ShaderManager() = default;

// ---- cache key ----

std::string ShaderManager::cacheKey(const ShaderProgramCPU& desc) {
    std::string raw;
    raw += desc.vsPath + desc.vsEntry;
    raw += desc.psPath + desc.psEntry;
    raw += desc.csPath + desc.csEntry;
    raw += desc.gsPath + desc.gsEntry;
    raw += desc.tcsPath + desc.tcsEntry;
    raw += desc.tesPath + desc.tesEntry;
    for (const auto& [k, v] : desc.defines)
        raw += k + "=" + v + ";";
    return std::to_string(fnv1a(raw));
}

// ---- compile (public) ----

std::shared_ptr<ShaderProgram> ShaderManager::compile(const ShaderProgramCPU& desc) {
    const std::string key = cacheKey(desc);

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            ++stats_.cacheHits;
            return it->second;
        }
    }

    ++stats_.cacheMisses;
    auto program = compileUncached(desc);
    if (!program || !program->isValid)
        return nullptr;

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[key]       = program;
        descriptors_[key] = desc;

        if (desc.enableHotReload) {
            auto& watched = watchedFiles_[key];
            auto track = [&](const std::string& path) {
                if (!path.empty())
                    watched[path] = fileWriteTime(path);
            };
            track(desc.vsPath);
            track(desc.psPath);
            track(desc.csPath);
            track(desc.gsPath);
            track(desc.tcsPath);
            track(desc.tesPath);
        }
    }

    return program;
}

// ---- compile variants ----

std::vector<std::shared_ptr<ShaderProgram>> ShaderManager::compileVariants(
    const ShaderProgramCPU&                      base,
    const std::vector<std::vector<std::string>>& variantDefines)
{
    std::vector<std::shared_ptr<ShaderProgram>> results;
    results.reserve(variantDefines.size());

    for (const auto& defs : variantDefines) {
        ShaderProgramCPU desc = base;
        for (const auto& name : defs)
            desc.defines.emplace_back(name, "1");
        results.push_back(compile(desc));
    }
    return results;
}

// ---- compileUncached (private) ----

std::shared_ptr<ShaderProgram> ShaderManager::compileUncached(const ShaderProgramCPU& desc) {
    auto program = std::make_shared<ShaderProgram>();

    auto t0 = std::chrono::high_resolution_clock::now();

    struct StageInfo {
        ShaderStage  stage;
        std::string  path;
        std::string  entry;
    };

    const StageInfo stages[] = {
        { ShaderStage::Vertex,   desc.vsPath,  desc.vsEntry  },
        { ShaderStage::Pixel,    desc.psPath,  desc.psEntry  },
        { ShaderStage::Compute,  desc.csPath,  desc.csEntry  },
        { ShaderStage::Geometry, desc.gsPath,  desc.gsEntry  },
        { ShaderStage::Hull,     desc.tcsPath, desc.tcsEntry },
        { ShaderStage::Domain,   desc.tesPath, desc.tesEntry },
    };

    for (const auto& s : stages) {
        if (s.path.empty()) continue;

        ShaderCompileResult res = compiler_->compile(s.path, s.entry,
                                                      s.stage, desc.defines);
        if (!res.success) {
            return nullptr;
        }
        program->stageBytecode[static_cast<uint8_t>(s.stage)] =
            std::move(res.bytecode);
    }

    program->isValid = true;
    ++stats_.compilationCount;

    auto t1  = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Running average
    if (stats_.compilationCount == 1)
        stats_.averageCompileTime = ms;
    else
        stats_.averageCompileTime +=
            (ms - stats_.averageCompileTime) / static_cast<double>(stats_.compilationCount);

    return program;
}

// ---- hot-reload ----

bool ShaderManager::hasSourceChanged(const std::string& key) const {
    auto wit = watchedFiles_.find(key);
    if (wit == watchedFiles_.end()) return false;
    for (const auto& [path, stamp] : wit->second)
        if (fileWriteTime(path) != stamp) return true;
    return false;
}

void ShaderManager::update() {
    std::vector<std::string> toReload;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        for (const auto& [key, _] : watchedFiles_)
            if (hasSourceChanged(key))
                toReload.push_back(key);
    }

    for (const auto& key : toReload) {
        ShaderProgramCPU desc;
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            auto dit = descriptors_.find(key);
            if (dit == descriptors_.end()) continue;
            desc = dit->second;
        }

        auto fresh = compileUncached(desc);
        if (!fresh || !fresh->isValid)
            continue;

        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[key] = fresh;
            // Refresh timestamps
            auto& watched = watchedFiles_[key];
            for (auto& [path, stamp] : watched)
                stamp = fileWriteTime(path);
        }

        if (reloadCallback_)
            reloadCallback_(key);
    }
}

void ShaderManager::setReloadCallback(ReloadCallback cb) {
    reloadCallback_ = std::move(cb);
}

// ---- cache management ----

void ShaderManager::clearMemoryCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
    watchedFiles_.clear();
    descriptors_.clear();
}

void ShaderManager::saveDiskCache(const std::string& directory) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    fs::create_directories(directory);

    for (const auto& [key, program] : cache_) {
        if (!program || !program->isValid) continue;
        for (const auto& [stageId, bytecode] : program->stageBytecode) {
            std::string filename = directory + "/" + key + "_" +
                                   std::to_string(stageId) + ".bin";
            std::ofstream f(filename, std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytecode.data()),
                    static_cast<std::streamsize>(bytecode.size()));
        }
    }
}

void ShaderManager::loadDiskCache(const std::string& directory) {
    std::error_code ec;
    if (!fs::exists(directory, ec)) return;

    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (entry.path().extension() != ".bin") continue;
        std::string stem = entry.path().stem().string();

        // Filename format: <key>_<stageId>.bin
        auto sep = stem.rfind('_');
        if (sep == std::string::npos) continue;
        std::string key     = stem.substr(0, sep);
        uint8_t     stageId = static_cast<uint8_t>(
            std::stoul(stem.substr(sep + 1)));

        std::ifstream f(entry.path(), std::ios::binary);
        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(f)),
            std::istreambuf_iterator<char>());

        auto& prog = cache_[key];
        if (!prog) {
            prog = std::make_shared<ShaderProgram>();
            prog->isValid = true;
        }
        prog->stageBytecode[stageId] = std::move(data);
    }
}

// ---- statistics ----

const ShaderManagerStats& ShaderManager::getStatistics() const {
    return stats_;
}

void ShaderManager::resetStatistics() {
    stats_ = {};
}
