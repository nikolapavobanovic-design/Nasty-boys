#include "ShaderManager.h"

#include <cassert>
#include <iostream>

// ---------------------------------------------------------------------------
// Helper: print a short result summary
// ---------------------------------------------------------------------------

static void printResult(const char* label,
                        const std::shared_ptr<ShaderProgram>& prog)
{
    std::cout << label << ": "
              << (prog && prog->isValid ? "OK" : "FAILED") << "\n";
}

// ---------------------------------------------------------------------------
// Example 1 – Basic vertex + pixel shader
// ---------------------------------------------------------------------------

static void example1_basicShader(ShaderManager& mgr) {
    std::cout << "\n=== Example 1: Basic Shader ===\n";

    ShaderProgramCPU desc;
    desc.vsPath = "example_shaders/basic.vs.hlsl";
    desc.psPath = "example_shaders/basic.ps.hlsl";

    auto shader = mgr.compile(desc);
    printResult("Basic VS+PS", shader);
}

// ---------------------------------------------------------------------------
// Example 2 – Compile twice; second call must be a cache hit
// ---------------------------------------------------------------------------

static void example2_caching(ShaderManager& mgr) {
    std::cout << "\n=== Example 2: Caching ===\n";

    mgr.resetStatistics();

    ShaderProgramCPU desc;
    desc.vsPath = "example_shaders/basic.vs.hlsl";
    desc.psPath = "example_shaders/basic.ps.hlsl";

    mgr.compile(desc); // compiles
    mgr.compile(desc); // cache hit

    const auto& s = mgr.getStatistics();
    std::cout << "Compilations : " << s.compilationCount  << "\n";
    std::cout << "Cache hits   : " << s.cacheHits         << "\n";
    assert(s.cacheHits >= 1 && "Expected at least one cache hit");
}

// ---------------------------------------------------------------------------
// Example 3 – Preprocessor defines
// ---------------------------------------------------------------------------

static void example3_defines(ShaderManager& mgr) {
    std::cout << "\n=== Example 3: Preprocessor Defines ===\n";

    ShaderProgramCPU desc;
    desc.vsPath  = "example_shaders/pbr.vs.hlsl";
    desc.psPath  = "example_shaders/basic.ps.hlsl";
    desc.defines = {
        {"MAX_LIGHTS",      "4"},
        {"USE_NORMAL_MAP",  "1"},
        {"SHADOW_MAP_SIZE", "2048"}
    };

    auto shader = mgr.compile(desc);
    printResult("PBR VS with defines", shader);
}

// ---------------------------------------------------------------------------
// Example 4 – Shader variants
// ---------------------------------------------------------------------------

static void example4_variants(ShaderManager& mgr) {
    std::cout << "\n=== Example 4: Shader Variants ===\n";

    ShaderProgramCPU base;
    base.vsPath = "example_shaders/pbr.vs.hlsl";
    base.psPath = "example_shaders/basic.ps.hlsl";

    std::vector<std::vector<std::string>> variants = {
        {},                          // Base
        {"SKINNED"},                 // Skinned meshes
        {"INSTANCED"},               // GPU instancing
        {"SKINNED", "INSTANCED"},    // Both
        {"ALPHA_TEST"}               // Alpha testing
    };

    auto shaders = mgr.compileVariants(base, variants);
    std::cout << "Compiled " << shaders.size() << " variants\n";
    for (size_t i = 0; i < shaders.size(); ++i) {
        std::cout << "  Variant " << i << ": "
                  << (shaders[i] && shaders[i]->isValid ? "OK" : "FAILED") << "\n";
    }
}

// ---------------------------------------------------------------------------
// Example 5 – Compute shader
// ---------------------------------------------------------------------------

static void example5_computeShader(ShaderManager& mgr) {
    std::cout << "\n=== Example 5: Compute Shader ===\n";

    ShaderProgramCPU desc;
    desc.csPath  = "example_shaders/basic.vs.hlsl"; // reuse file as mock
    desc.csEntry = "CSMain";

    auto shader = mgr.compile(desc);
    printResult("Compute shader", shader);
}

// ---------------------------------------------------------------------------
// Example 6 – All graphics pipeline stages
// ---------------------------------------------------------------------------

static void example6_allStages(ShaderManager& mgr) {
    std::cout << "\n=== Example 6: All Pipeline Stages ===\n";

    ShaderProgramCPU desc;
    desc.vsPath  = "example_shaders/basic.vs.hlsl";
    desc.gsPath  = "example_shaders/basic.vs.hlsl"; // mock reuse
    desc.psPath  = "example_shaders/basic.ps.hlsl";
    desc.tcsPath = "example_shaders/basic.vs.hlsl"; // mock reuse
    desc.tesPath = "example_shaders/basic.vs.hlsl"; // mock reuse

    auto shader = mgr.compile(desc);
    printResult("All stages", shader);
    if (shader && shader->isValid)
        std::cout << "Active stages: " << shader->stageBytecode.size() << "\n";
}

// ---------------------------------------------------------------------------
// Example 7 – Statistics
// ---------------------------------------------------------------------------

static void example7_statistics(ShaderManager& mgr) {
    std::cout << "\n=== Example 7: Statistics ===\n";

    const auto& s = mgr.getStatistics();
    std::cout << "Total compilations : " << s.compilationCount   << "\n";
    std::cout << "Cache hits         : " << s.cacheHits          << "\n";
    std::cout << "Cache misses       : " << s.cacheMisses        << "\n";
    std::cout << "Avg compile time   : " << s.averageCompileTime << " ms\n";
}

// ---------------------------------------------------------------------------
// Example 8 – Disk cache
// ---------------------------------------------------------------------------

static void example8_diskCache() {
    std::cout << "\n=== Example 8: Disk Cache ===\n";

    {
        ShaderManager mgr1(GraphicsAPI::DirectX11);

        ShaderProgramCPU desc;
        desc.vsPath = "example_shaders/basic.vs.hlsl";
        desc.psPath = "example_shaders/basic.ps.hlsl";
        mgr1.compile(desc);

        mgr1.saveDiskCache("/tmp/shader_cache");
        std::cout << "Cache saved to /tmp/shader_cache\n";
    }

    {
        ShaderManager mgr2(GraphicsAPI::DirectX11);
        mgr2.loadDiskCache("/tmp/shader_cache");
        std::cout << "Cache loaded in new manager instance\n";
    }
}

// ---------------------------------------------------------------------------
// Example 9 – Hot-reload (simulated; no actual file modification here)
// ---------------------------------------------------------------------------

static void example9_hotReload(ShaderManager& mgr) {
    std::cout << "\n=== Example 9: Hot-Reload Setup ===\n";

    mgr.setReloadCallback([](const std::string& key) {
        std::cout << "  [Hot-Reload] Shader reloaded, key=" << key << "\n";
    });

    ShaderProgramCPU desc;
    desc.vsPath        = "example_shaders/basic.vs.hlsl";
    desc.psPath        = "example_shaders/basic.ps.hlsl";
    desc.enableHotReload = true;

    mgr.compile(desc);

    // In a real game loop you would call mgr.update() each frame.
    mgr.update(); // no file changes – no reload triggered
    std::cout << "Hot-reload check passed (no changes detected)\n";
}

// ---------------------------------------------------------------------------
// Example 10 – Clear cache
// ---------------------------------------------------------------------------

static void example10_clearCache(ShaderManager& mgr) {
    std::cout << "\n=== Example 10: Clear Cache ===\n";

    mgr.clearMemoryCache();

    ShaderProgramCPU desc;
    desc.vsPath = "example_shaders/basic.vs.hlsl";
    desc.psPath = "example_shaders/basic.ps.hlsl";

    // Must recompile after clear
    auto shader = mgr.compile(desc);
    printResult("Recompiled after cache clear", shader);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== Shader Management System – Usage Examples ===\n";

    ShaderManager mgr(GraphicsAPI::DirectX11);

    example1_basicShader(mgr);
    example2_caching(mgr);
    example3_defines(mgr);
    example4_variants(mgr);
    example5_computeShader(mgr);
    example6_allStages(mgr);
    example7_statistics(mgr);
    example8_diskCache();
    example9_hotReload(mgr);
    example10_clearCache(mgr);

    std::cout << "\nAll examples completed.\n";
    return 0;
}
