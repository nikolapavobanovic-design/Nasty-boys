// Unit tests for ShaderCompilerFactory::create() and ShaderManager::statisticsToJson()
// and ShaderManager::evictLRU().
//
// Uses a minimal self-contained test harness (no external framework required).

#include "ShaderCompiler.h"
#include "ShaderManager.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int  g_tests   = 0;
static int  g_passed  = 0;
static int  g_failed  = 0;

#define EXPECT_TRUE(expr)                                                  \
    do {                                                                   \
        ++g_tests;                                                         \
        if (expr) {                                                        \
            ++g_passed;                                                    \
        } else {                                                           \
            ++g_failed;                                                    \
            std::cerr << "  FAIL [" #expr "] at " __FILE__                \
                      << ":" << __LINE__ << "\n";                         \
        }                                                                  \
    } while (false)

#define EXPECT_FALSE(expr)  EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a, b)     EXPECT_TRUE((a) == (b))
#define EXPECT_NE(a, b)     EXPECT_TRUE((a) != (b))

static void begin(const char* name) {
    std::cout << "[ RUN ] " << name << "\n";
}
static void end(const char* name, int failed_before) {
    if (g_failed == failed_before)
        std::cout << "[ OK  ] " << name << "\n";
    else
        std::cout << "[FAIL ] " << name << "\n";
}

// ---------------------------------------------------------------------------
// ShaderCompilerFactory::create()
// ---------------------------------------------------------------------------

static void test_factory_directx11_returns_directx_compiler() {
    begin("factory_directx11_returns_directx_compiler");
    int fb = g_failed;

    auto compiler = ShaderCompilerFactory::create(GraphicsAPI::DirectX11);
    EXPECT_TRUE(compiler != nullptr);
    EXPECT_TRUE(dynamic_cast<DirectXShaderCompiler*>(compiler.get()) != nullptr);

    end("factory_directx11_returns_directx_compiler", fb);
}

static void test_factory_directx12_returns_directx_compiler() {
    begin("factory_directx12_returns_directx_compiler");
    int fb = g_failed;

    auto compiler = ShaderCompilerFactory::create(GraphicsAPI::DirectX12);
    EXPECT_TRUE(compiler != nullptr);
    EXPECT_TRUE(dynamic_cast<DirectXShaderCompiler*>(compiler.get()) != nullptr);

    end("factory_directx12_returns_directx_compiler", fb);
}

static void test_factory_opengl_returns_glsl_compiler() {
    begin("factory_opengl_returns_glsl_compiler");
    int fb = g_failed;

    auto compiler = ShaderCompilerFactory::create(GraphicsAPI::OpenGL);
    EXPECT_TRUE(compiler != nullptr);
    EXPECT_TRUE(dynamic_cast<GLSLShaderCompiler*>(compiler.get()) != nullptr);

    end("factory_opengl_returns_glsl_compiler", fb);
}

static void test_factory_vulkan_returns_spirv_compiler() {
    begin("factory_vulkan_returns_spirv_compiler");
    int fb = g_failed;

    auto compiler = ShaderCompilerFactory::create(GraphicsAPI::Vulkan);
    EXPECT_TRUE(compiler != nullptr);
    EXPECT_TRUE(dynamic_cast<SPIRVShaderCompiler*>(compiler.get()) != nullptr);

    end("factory_vulkan_returns_spirv_compiler", fb);
}

static void test_factory_metal_throws() {
    begin("factory_metal_throws");
    int fb = g_failed;

    bool threw = false;
    try {
        ShaderCompilerFactory::create(GraphicsAPI::Metal);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    end("factory_metal_throws", fb);
}

static void test_factory_same_api_returns_distinct_instances() {
    begin("factory_same_api_returns_distinct_instances");
    int fb = g_failed;

    // Two calls with the same API must produce independent unique_ptr instances.
    auto c1 = ShaderCompilerFactory::create(GraphicsAPI::DirectX11);
    auto c2 = ShaderCompilerFactory::create(GraphicsAPI::DirectX11);
    EXPECT_NE(c1.get(), c2.get());
    EXPECT_TRUE(dynamic_cast<DirectXShaderCompiler*>(c1.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<DirectXShaderCompiler*>(c2.get()) != nullptr);

    end("factory_same_api_returns_distinct_instances", fb);
}

// ---------------------------------------------------------------------------
// ShaderManager::statisticsToJson()
// ---------------------------------------------------------------------------

static void test_statistics_to_json_contains_expected_keys() {
    begin("statistics_to_json_contains_expected_keys");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    const std::string json = mgr.statisticsToJson();

    EXPECT_TRUE(json.find("\"compilationCount\"")   != std::string::npos);
    EXPECT_TRUE(json.find("\"cacheHits\"")          != std::string::npos);
    EXPECT_TRUE(json.find("\"cacheMisses\"")        != std::string::npos);
    EXPECT_TRUE(json.find("\"averageCompileTime\"") != std::string::npos);

    end("statistics_to_json_contains_expected_keys", fb);
}

static void test_statistics_to_json_zero_values_after_reset() {
    begin("statistics_to_json_zero_values_after_reset");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::OpenGL);
    mgr.resetStatistics();
    const std::string json = mgr.statisticsToJson();

    // All counts must be 0 after reset
    EXPECT_TRUE(json.find("\"compilationCount\": 0")   != std::string::npos);
    EXPECT_TRUE(json.find("\"cacheHits\": 0")          != std::string::npos);
    EXPECT_TRUE(json.find("\"cacheMisses\": 0")        != std::string::npos);

    end("statistics_to_json_zero_values_after_reset", fb);
}

// ---------------------------------------------------------------------------
// ShaderManager::evictLRU()
// ---------------------------------------------------------------------------

static void test_evict_lru_does_nothing_when_under_limit() {
    begin("evict_lru_does_nothing_when_under_limit");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    // Cache is empty; evicting with a large limit must not crash.
    mgr.evictLRU(10);
    EXPECT_EQ(mgr.getStatistics().compilationCount, 0u);

    end("evict_lru_does_nothing_when_under_limit", fb);
}

static void test_evict_lru_on_empty_cache_is_safe() {
    // Verify that calling evictLRU(0) on an empty cache does not crash and
    // leaves the manager in a consistent, usable state.
    begin("evict_lru_on_empty_cache_is_safe");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::Vulkan);
    mgr.evictLRU(0);
    // Manager must still be usable after eviction of empty cache.
    mgr.resetStatistics();
    EXPECT_EQ(mgr.getStatistics().cacheHits, 0u);

    end("evict_lru_on_empty_cache_is_safe", fb);
}

static void test_evict_lru_noop_when_under_limit_after_clear() {
    // Verify that evictLRU does not corrupt the manager state when the cache
    // is empty after clearMemoryCache(), and that the LRU structures are also
    // cleared (i.e. subsequent eviction calls are safe and idempotent).
    begin("evict_lru_noop_when_under_limit_after_clear");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    mgr.clearMemoryCache();   // resets cache AND LRU structures
    mgr.evictLRU(0);          // no-op on empty cache; must not crash
    mgr.evictLRU(0);          // idempotent
    EXPECT_EQ(mgr.getStatistics().cacheHits, 0u);

    end("evict_lru_noop_when_under_limit_after_clear", fb);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== ShaderSystem Unit Tests ===\n\n";

    test_factory_directx11_returns_directx_compiler();
    test_factory_directx12_returns_directx_compiler();
    test_factory_opengl_returns_glsl_compiler();
    test_factory_vulkan_returns_spirv_compiler();
    test_factory_metal_throws();
    test_factory_same_api_returns_distinct_instances();

    test_statistics_to_json_contains_expected_keys();
    test_statistics_to_json_zero_values_after_reset();

    test_evict_lru_does_nothing_when_under_limit();
    test_evict_lru_on_empty_cache_is_safe();
    test_evict_lru_noop_when_under_limit_after_clear();

    std::cout << "\n---\n"
              << g_tests  << " tests, "
              << g_passed << " passed, "
              << g_failed << " failed.\n";

    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
