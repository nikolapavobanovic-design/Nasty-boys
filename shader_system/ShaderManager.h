#pragma once

#include "ShaderTypes.h"
#include "ShaderCompiler.h"

#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

struct ShaderManagerStats {
    uint64_t compilationCount  = 0;
    uint64_t cacheHits         = 0;
    uint64_t cacheMisses       = 0;
    double   averageCompileTime = 0.0; // milliseconds
};

// ---------------------------------------------------------------------------
// ShaderManager
// ---------------------------------------------------------------------------

class ShaderManager {
public:
    using ReloadCallback = std::function<void(const std::string& cacheKey)>;

    explicit ShaderManager(GraphicsAPI api);
    ~ShaderManager();

    // -----------------------------------------------------------------------
    // Compilation (file-based)
    // -----------------------------------------------------------------------

    // Compile (or retrieve from cache) a shader program.
    // Returns nullptr on failure.
    std::shared_ptr<ShaderProgram> compile(const ShaderProgramCPU& desc);

    // Asynchronously compile a shader program.
    // The returned future resolves to nullptr on failure.
    std::future<std::shared_ptr<ShaderProgram>> compileAsync(const ShaderProgramCPU& desc);

    // Compile multiple variant permutations of a base descriptor.
    // Each inner vector is a set of define names that are set to "1".
    std::vector<std::shared_ptr<ShaderProgram>> compileVariants(
        const ShaderProgramCPU&                       base,
        const std::vector<std::vector<std::string>>&  variantDefines);

    // -----------------------------------------------------------------------
    // Compilation (in-memory source strings)
    // -----------------------------------------------------------------------

    // Compile (or retrieve from cache) a shader program from source strings.
    // Returns nullptr on failure.
    std::shared_ptr<ShaderProgram> compileFromSource(const ShaderProgramSource& src);

    // Asynchronously compile a shader program from source strings.
    // The returned future resolves to nullptr on failure.
    std::future<std::shared_ptr<ShaderProgram>> compileFromSourceAsync(const ShaderProgramSource& src);

    // -----------------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------------

    void clearMemoryCache();

    // Evict least-recently-used cache entries until at most maxEntries remain.
    // Hot-reload tracking and descriptors for evicted entries are also removed.
    void evictLRU(size_t maxEntries);

    // Set a hard limit on the number of in-memory cache entries.
    // After each successful compilation the LRU is evicted to this limit.
    // Pass 0 to disable the limit (default).
    void setCacheCapacity(size_t maxEntries);

    // Persist the in-memory cache to disk (directory path).
    void saveDiskCache(const std::string& directory) const;

    // Load a previously saved disk cache.
    void loadDiskCache(const std::string& directory);

    // -----------------------------------------------------------------------
    // Hot-reload
    // -----------------------------------------------------------------------

    // Register a callback invoked whenever a shader is reloaded.
    void setReloadCallback(ReloadCallback cb);

    // Poll source files for changes and recompile if needed.
    // Call once per frame from the main/render thread.
    void update();

    // -----------------------------------------------------------------------
    // Stats
    // -----------------------------------------------------------------------

    const ShaderManagerStats& getStatistics() const;
    void resetStatistics();

    // Returns a JSON string representation of the current statistics.
    std::string statisticsToJson() const;

private:
    // Derive a stable cache key from a file-based descriptor.
    static std::string cacheKey(const ShaderProgramCPU& desc);

    // Derive a stable cache key from a source-string descriptor.
    static std::string cacheKey(const ShaderProgramSource& src);

    // Compile all active stages of desc and return a filled ShaderProgram.
    std::shared_ptr<ShaderProgram> compileUncached(const ShaderProgramCPU& desc);

    // Compile all active stages from source strings and return a filled ShaderProgram.
    std::shared_ptr<ShaderProgram> compileUncachedFromSource(const ShaderProgramSource& src);

    // Update the running average compilation time and increment compilation count.
    void updateCompileStats(double ms);

    // Check whether any source file tracked for a cache entry has changed.
    bool hasSourceChanged(const std::string& key) const;

    // Move key to the front of the LRU list (most recently used).
    // Must be called with cacheMutex_ held.
    void touchLRU(const std::string& key);

    // Evict down to cacheCapacity_ if a capacity limit has been set.
    // Must be called with cacheMutex_ held.
    void enforceCapacity();

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    GraphicsAPI                    api_;
    std::unique_ptr<IShaderCompiler> compiler_;

    // Memory cache:  cacheKey -> compiled program
    mutable std::mutex                                            cacheMutex_;
    std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> cache_;

    // Hot-reload tracking:  cacheKey -> { source-path -> last-write-time }
    std::unordered_map<std::string,
        std::unordered_map<std::string, int64_t>>                 watchedFiles_;

    // Original descriptors kept for recompilation
    std::unordered_map<std::string, ShaderProgramCPU>             descriptors_;

    // LRU eviction tracking: front = most recently used, back = least recently used
    std::list<std::string>                                         lruOrder_;
    std::unordered_map<std::string,
        std::list<std::string>::iterator>                          lruPos_;

    size_t           cacheCapacity_ = 0; // 0 = unlimited
    ReloadCallback   reloadCallback_;
    ShaderManagerStats stats_;
};
