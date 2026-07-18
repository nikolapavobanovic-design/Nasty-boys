// Unit tests for ShaderCompilerFactory::create() and ShaderManager::statisticsToJson()
// and ShaderManager::evictLRU().
//
// Uses a minimal self-contained test harness (no external framework required).

#include "ShaderCompiler.h"
#include "ShaderManager.h"

#include <cassert>
#include <cstdlib>
#include <future>
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

static void test_factory_metal_returns_metal_compiler() {
    begin("factory_metal_returns_metal_compiler");
    int fb = g_failed;

    auto compiler = ShaderCompilerFactory::create(GraphicsAPI::Metal);
    EXPECT_TRUE(compiler != nullptr);
    EXPECT_TRUE(dynamic_cast<MetalShaderCompiler*>(compiler.get()) != nullptr);

    end("factory_metal_returns_metal_compiler", fb);
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
// MetalShaderCompiler::compileSource()
// ---------------------------------------------------------------------------

static void test_metal_compile_source_success() {
    begin("metal_compile_source_success");
    int fb = g_failed;

    MetalShaderCompiler compiler;
    ShaderCompileResult res = compiler.compileSource(
        "kernel void main_kernel() {}", "main_kernel",
        ShaderStage::Compute, {});

    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.bytecode.empty());

    end("metal_compile_source_success", fb);
}

static void test_metal_compile_source_bytecode_matches_source() {
    begin("metal_compile_source_bytecode_matches_source");
    int fb = g_failed;

    const std::string src = "void vertex_main() {}";
    MetalShaderCompiler compiler;
    ShaderCompileResult res = compiler.compileSource(
        src, "vertex_main", ShaderStage::Vertex, {});

    EXPECT_TRUE(res.success);
    std::string decoded(res.bytecode.begin(), res.bytecode.end());
    EXPECT_EQ(decoded, src);

    end("metal_compile_source_bytecode_matches_source", fb);
}

// ---------------------------------------------------------------------------
// IShaderCompiler::compileSource() – all backends
// ---------------------------------------------------------------------------

static void test_compile_source_directx_success() {
    begin("compile_source_directx_success");
    int fb = g_failed;

    DirectXShaderCompiler compiler(GraphicsAPI::DirectX11);
    const std::string src = "float4 VS() : SV_Position { return 0; }";
    auto res = compiler.compileSource(src, "VS", ShaderStage::Vertex, {});
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.bytecode.empty());

    end("compile_source_directx_success", fb);
}

static void test_compile_source_glsl_success() {
    begin("compile_source_glsl_success");
    int fb = g_failed;

    GLSLShaderCompiler compiler;
    const std::string src = "void main() { gl_Position = vec4(0); }";
    auto res = compiler.compileSource(src, "main", ShaderStage::Vertex, {});
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.bytecode.empty());

    end("compile_source_glsl_success", fb);
}

static void test_compile_source_spirv_success() {
    begin("compile_source_spirv_success");
    int fb = g_failed;

    SPIRVShaderCompiler compiler;
    const std::string src = "void main() {}";
    auto res = compiler.compileSource(src, "main", ShaderStage::Vertex, {});
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.bytecode.empty());

    end("compile_source_spirv_success", fb);
}

// ---------------------------------------------------------------------------
// ShaderManager::compileFromSource()
// ---------------------------------------------------------------------------

static void test_compile_from_source_returns_valid_program() {
    begin("compile_from_source_returns_valid_program");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    ShaderProgramSource src;
    src.vsSource = "float4 VS() : SV_Position { return 0; }";
    src.psSource = "float4 PS() : SV_Target  { return 1; }";

    auto prog = mgr.compileFromSource(src);
    EXPECT_TRUE(prog != nullptr);
    EXPECT_TRUE(prog->isValid);
    EXPECT_EQ(prog->stageBytecode.count(uint8_t(ShaderStage::Vertex)), 1u);
    EXPECT_EQ(prog->stageBytecode.count(uint8_t(ShaderStage::Pixel)),  1u);

    end("compile_from_source_returns_valid_program", fb);
}

static void test_compile_from_source_cache_hit() {
    begin("compile_from_source_cache_hit");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::OpenGL);
    mgr.resetStatistics();

    ShaderProgramSource src;
    src.vsSource = "void main() { gl_Position = vec4(0); }";

    mgr.compileFromSource(src); // first call – compiles
    mgr.compileFromSource(src); // second call – cache hit

    EXPECT_EQ(mgr.getStatistics().compilationCount, 1u);
    EXPECT_TRUE(mgr.getStatistics().cacheHits >= 1u);

    end("compile_from_source_cache_hit", fb);
}

static void test_compile_from_source_distinct_sources_distinct_programs() {
    begin("compile_from_source_distinct_sources_distinct_programs");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::Vulkan);
    ShaderProgramSource src1, src2;
    src1.vsSource = "void main() { gl_Position = vec4(1.0); }";
    src2.vsSource = "void main() { gl_Position = vec4(2.0); }";

    auto p1 = mgr.compileFromSource(src1);
    auto p2 = mgr.compileFromSource(src2);

    EXPECT_TRUE(p1 != nullptr);
    EXPECT_TRUE(p2 != nullptr);
    EXPECT_NE(p1.get(), p2.get());

    end("compile_from_source_distinct_sources_distinct_programs", fb);
}

// ---------------------------------------------------------------------------
// ShaderManager::compileAsync() and compileFromSourceAsync()
// ---------------------------------------------------------------------------

static void test_compile_async_returns_valid_program() {
    begin("compile_async_returns_valid_program");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    ShaderProgramCPU desc;
    desc.vsPath = "example_shaders/basic.vs.hlsl";
    desc.psPath = "example_shaders/basic.ps.hlsl";

    auto fut = mgr.compileAsync(desc);
    auto prog = fut.get();

    EXPECT_TRUE(prog != nullptr);
    EXPECT_TRUE(prog && prog->isValid);

    end("compile_async_returns_valid_program", fb);
}

static void test_compile_async_result_equals_sync_result() {
    begin("compile_async_result_equals_sync_result");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    ShaderProgramCPU desc;
    desc.vsPath = "example_shaders/basic.vs.hlsl";

    auto sync_prog  = mgr.compile(desc);
    auto async_prog = mgr.compileAsync(desc).get(); // should be a cache hit

    EXPECT_TRUE(sync_prog  != nullptr);
    EXPECT_TRUE(async_prog != nullptr);
    // Both should point to the same cached object.
    EXPECT_EQ(sync_prog.get(), async_prog.get());

    end("compile_async_result_equals_sync_result", fb);
}

static void test_compile_from_source_async_returns_valid_program() {
    begin("compile_from_source_async_returns_valid_program");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::Vulkan);
    ShaderProgramSource src;
    src.vsSource = "void main() { gl_Position = vec4(0); }";

    auto fut  = mgr.compileFromSourceAsync(src);
    auto prog = fut.get();

    EXPECT_TRUE(prog != nullptr);
    EXPECT_TRUE(prog->isValid);

    end("compile_from_source_async_returns_valid_program", fb);
}

// ---------------------------------------------------------------------------
// ShaderManager::setCacheCapacity()
// ---------------------------------------------------------------------------

static void test_set_cache_capacity_limits_entries() {
    begin("set_cache_capacity_limits_entries");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);
    mgr.setCacheCapacity(1);

    // Compile two distinct programs; only the most recent should survive.
    ShaderProgramSource s1, s2;
    s1.vsSource = "void vsA() {}";
    s2.vsSource = "void vsB() {}";

    mgr.compileFromSource(s1);
    mgr.compileFromSource(s2); // should evict s1

    // Re-compiling s1 must now be a cache miss (it was evicted).
    mgr.resetStatistics();
    mgr.compileFromSource(s1);
    EXPECT_EQ(mgr.getStatistics().cacheMisses, 1u);

    end("set_cache_capacity_limits_entries", fb);
}

static void test_set_cache_capacity_zero_is_unlimited() {
    begin("set_cache_capacity_zero_is_unlimited");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::OpenGL);
    mgr.setCacheCapacity(0); // unlimited

    ShaderProgramSource s1, s2, s3;
    s1.vsSource = "void vsA() {}";
    s2.vsSource = "void vsB() {}";
    s3.vsSource = "void vsC() {}";

    mgr.compileFromSource(s1);
    mgr.compileFromSource(s2);
    mgr.compileFromSource(s3);

    // All three should still be cached.
    mgr.resetStatistics();
    mgr.compileFromSource(s1);
    mgr.compileFromSource(s2);
    mgr.compileFromSource(s3);
    EXPECT_EQ(mgr.getStatistics().cacheHits,  3u);
    EXPECT_EQ(mgr.getStatistics().cacheMisses, 0u);

    end("set_cache_capacity_zero_is_unlimited", fb);
}

static void test_set_cache_capacity_immediate_eviction() {
    begin("set_cache_capacity_immediate_eviction");
    int fb = g_failed;

    ShaderManager mgr(GraphicsAPI::DirectX11);

    // Compile two entries first, then cap to 1 – the LRU entry must vanish.
    ShaderProgramSource s1, s2;
    s1.vsSource = "void vsA() {}";
    s2.vsSource = "void vsB() {}";

    mgr.compileFromSource(s1); // LRU: [s1]
    mgr.compileFromSource(s2); // LRU: [s2, s1]

    mgr.setCacheCapacity(1); // should evict s1; cache now holds only s2

    mgr.resetStatistics();
    // With capacity 1 each new compilation evicts the previous entry.
    // So both lookups below are cache misses.
    mgr.compileFromSource(s1); // miss – was evicted; also evicts s2
    mgr.compileFromSource(s2); // miss – was just evicted when s1 was added
    EXPECT_EQ(mgr.getStatistics().cacheMisses, 2u);
    EXPECT_EQ(mgr.getStatistics().cacheHits,   0u);

    end("set_cache_capacity_immediate_eviction", fb);
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
    test_factory_metal_returns_metal_compiler();
    test_factory_same_api_returns_distinct_instances();

    test_metal_compile_source_success();
    test_metal_compile_source_bytecode_matches_source();

    test_compile_source_directx_success();
    test_compile_source_glsl_success();
    test_compile_source_spirv_success();

    test_compile_from_source_returns_valid_program();
    test_compile_from_source_cache_hit();
    test_compile_from_source_distinct_sources_distinct_programs();

    test_compile_async_returns_valid_program();
    test_compile_async_result_equals_sync_result();
    test_compile_from_source_async_returns_valid_program();

    test_set_cache_capacity_limits_entries();
    test_set_cache_capacity_zero_is_unlimited();
    test_set_cache_capacity_immediate_eviction();

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
