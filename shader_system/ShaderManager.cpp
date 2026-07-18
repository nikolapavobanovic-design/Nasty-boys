#include "ShaderManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
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

ShaderManager::~ShaderManager() {
    // Wait for all in-flight async compilations so that lambdas that captured
    // `this` finish before the object is destroyed.
    std::vector<std::future<void>> toWait;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        toWait = std::move(pendingFutures_);
    }
    for (auto& f : toWait)
        if (f.valid()) f.wait();
}

// ---- cache keys ----

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

std::string ShaderManager::cacheKey(const ShaderProgramSource& src) {
    // Prefix "src:" to avoid collisions with file-based keys.
    std::string raw = "src:";
    raw += src.vsSource + src.vsEntry;
    raw += src.psSource + src.psEntry;
    raw += src.csSource + src.csEntry;
    raw += src.gsSource + src.gsEntry;
    raw += src.tcsSource + src.tcsEntry;
    raw += src.tesSource + src.tesEntry;
    for (const auto& [k, v] : src.defines)
        raw += k + "=" + v + ";";
    return std::to_string(fnv1a(raw));
}

// ---- shared stats helper ----

void ShaderManager::updateCompileStats(double ms) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    ++stats_.compilationCount;
    if (stats_.compilationCount == 1)
        stats_.averageCompileTime = ms;
    else
        stats_.averageCompileTime +=
            (ms - stats_.averageCompileTime) /
            static_cast<double>(stats_.compilationCount);
}

// ---- compile (public, file-based) ----

std::shared_ptr<ShaderProgram> ShaderManager::compile(const ShaderProgramCPU& desc) {
    const std::string key = cacheKey(desc);

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            {
                std::lock_guard<std::mutex> slock(statsMutex_);
                ++stats_.cacheHits;
            }
            touchLRU(key);
            return it->second;
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.cacheMisses;
    }
    auto program = compileUncached(desc);
    if (!program)
        return nullptr;

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[key]       = program;
        descriptors_[key] = desc;
        touchLRU(key);
        enforceCapacity();

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

// ---- compileAsync (public, file-based) ----

std::future<std::shared_ptr<ShaderProgram>>
ShaderManager::compileAsync(const ShaderProgramCPU& desc) {
    // Use a shared promise so the caller's future and an internal tracking
    // future can both be derived from the same async task.  The internal
    // future is stored in pendingFutures_ and awaited by the destructor,
    // guaranteeing that `this` is valid for the entire duration of the lambda.
    auto promise = std::make_shared<std::promise<std::shared_ptr<ShaderProgram>>>();
    auto userFuture = promise->get_future();

    auto taskFuture = std::async(std::launch::async,
        [this, desc, promise]() mutable {
            try {
                promise->set_value(compile(desc));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingFutures_.push_back(std::move(taskFuture));
    }
    return userFuture;
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

// ---- compileFromSource (public, source strings) ----

std::shared_ptr<ShaderProgram>
ShaderManager::compileFromSource(const ShaderProgramSource& src) {
    const std::string key = cacheKey(src);

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            {
                std::lock_guard<std::mutex> slock(statsMutex_);
                ++stats_.cacheHits;
            }
            touchLRU(key);
            return it->second;
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.cacheMisses;
    }
    auto program = compileUncachedFromSource(src);
    if (!program)
        return nullptr;

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[key] = program;
        touchLRU(key);
        enforceCapacity();
    }

    return program;
}

// ---- compileFromSourceAsync (public, source strings) ----

std::future<std::shared_ptr<ShaderProgram>>
ShaderManager::compileFromSourceAsync(const ShaderProgramSource& src) {
    auto promise = std::make_shared<std::promise<std::shared_ptr<ShaderProgram>>>();
    auto userFuture = promise->get_future();

    auto taskFuture = std::async(std::launch::async,
        [this, src, promise]() mutable {
            try {
                promise->set_value(compileFromSource(src));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingFutures_.push_back(std::move(taskFuture));
    }
    return userFuture;
}

// ---- compileUncached (private, file-based) ----

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

        ShaderCompileResult res;
        {
            std::lock_guard<std::mutex> lock(compilerMutex_);
            res = compiler_->compile(s.path, s.entry, s.stage, desc.defines);
        }
        if (!res.success) {
            std::cerr << "Shader compilation failed (" << s.path
                      << ", entry=" << s.entry << "): " << res.errorMsg << "\n";
            return nullptr;
        }
        program->stageBytecode[static_cast<uint8_t>(s.stage)] =
            std::move(res.bytecode);
    }

    program->isValid = true;

    auto t1  = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    updateCompileStats(ms);

    return program;
}

// ---- compileUncachedFromSource (private, source strings) ----

std::shared_ptr<ShaderProgram>
ShaderManager::compileUncachedFromSource(const ShaderProgramSource& src) {
    auto program = std::make_shared<ShaderProgram>();

    auto t0 = std::chrono::high_resolution_clock::now();

    struct StageInfo {
        ShaderStage  stage;
        std::string  source;
        std::string  entry;
    };

    const StageInfo stages[] = {
        { ShaderStage::Vertex,   src.vsSource,  src.vsEntry  },
        { ShaderStage::Pixel,    src.psSource,  src.psEntry  },
        { ShaderStage::Compute,  src.csSource,  src.csEntry  },
        { ShaderStage::Geometry, src.gsSource,  src.gsEntry  },
        { ShaderStage::Hull,     src.tcsSource, src.tcsEntry },
        { ShaderStage::Domain,   src.tesSource, src.tesEntry },
    };

    for (const auto& s : stages) {
        if (s.source.empty()) continue;

        ShaderCompileResult res;
        {
            std::lock_guard<std::mutex> lock(compilerMutex_);
            res = compiler_->compileSource(s.source, s.entry, s.stage, src.defines);
        }
        if (!res.success) {
            std::cerr << "Shader compilation failed (in-memory source"
                      << ", entry=" << s.entry << "): " << res.errorMsg << "\n";
            return nullptr;
        }
        program->stageBytecode[static_cast<uint8_t>(s.stage)] =
            std::move(res.bytecode);
    }

    program->isValid = true;

    auto t1  = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    updateCompileStats(ms);

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
        if (!fresh) {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            auto& watched = watchedFiles_[key];
            for (auto& [path, stamp] : watched)
                stamp = fileWriteTime(path);
            continue;
        }

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
    lruOrder_.clear();
    lruPos_.clear();
}

void ShaderManager::setCacheCapacity(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cacheCapacity_ = maxEntries;
    if (cacheCapacity_ > 0) {
        while (cache_.size() > cacheCapacity_ && !lruOrder_.empty()) {
            const std::string& lruKey = lruOrder_.back();
            cache_.erase(lruKey);
            descriptors_.erase(lruKey);
            watchedFiles_.erase(lruKey);
            lruPos_.erase(lruKey);
            lruOrder_.pop_back();
        }
    }
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
    // statsMutex_ is mutable; callers must treat the returned reference as a
    // snapshot – the values can change as soon as the lock is released.
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void ShaderManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = {};
}

std::string ShaderManager::statisticsToJson() const {
    ShaderManagerStats s;
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        s = stats_;
    }
    std::ostringstream oss;
    oss << "{\n"
        << "  \"compilationCount\": "   << s.compilationCount   << ",\n"
        << "  \"cacheHits\": "          << s.cacheHits          << ",\n"
        << "  \"cacheMisses\": "        << s.cacheMisses        << ",\n"
        << "  \"averageCompileTime\": " << s.averageCompileTime << "\n"
        << "}";
    return oss.str();
}

// ---- LRU helpers ----

void ShaderManager::touchLRU(const std::string& key) {
    // Remove existing position if present, then push to front.
    auto it = lruPos_.find(key);
    if (it != lruPos_.end())
        lruOrder_.erase(it->second);
    lruOrder_.push_front(key);
    lruPos_[key] = lruOrder_.begin();
}

void ShaderManager::enforceCapacity() {
    if (cacheCapacity_ == 0) return;
    while (cache_.size() > cacheCapacity_ && !lruOrder_.empty()) {
        const std::string& lruKey = lruOrder_.back();
        cache_.erase(lruKey);
        descriptors_.erase(lruKey);
        watchedFiles_.erase(lruKey);
        lruPos_.erase(lruKey);
        lruOrder_.pop_back();
    }
}

// ---- cache management (eviction) ----

void ShaderManager::evictLRU(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    while (cache_.size() > maxEntries && !lruOrder_.empty()) {
        const std::string& lruKey = lruOrder_.back();
        cache_.erase(lruKey);
        descriptors_.erase(lruKey);
        watchedFiles_.erase(lruKey);
        lruPos_.erase(lruKey);
        lruOrder_.pop_back();
    }
}
