# Shader Management System

A production-ready, platform-agnostic shader management system for C++17 graphics engines and rendering frameworks.

## Features

- **Multi-API support** – DirectX 11/12, OpenGL (GLSL), Vulkan (SPIR-V), Metal
- **Two-tier caching** – in-memory (instant) + disk (persistent across runs)
- **Hot-reload** – file watching with per-frame `update()` call
- **Shader variants** – compile permutations from a single base descriptor
- **Preprocessor defines** – pass `#define` name/value pairs at compile time
- **Statistics tracking** – compilation count, cache hit/miss, average time
- **Thread-safe cache** – mutex-guarded for multi-threaded renderers
- **Async compilation** – non-blocking `compileAsync()` / `compileFromSourceAsync()` returning `std::future`
- **In-memory source compilation** – compile from source strings without touching the filesystem via `compileFromSource()`
- **Bounded cache** – automatic LRU eviction when a capacity limit is set with `setCacheCapacity()`

## File Structure

```
shader_system/
├── ShaderTypes.h/cpp          # Core types (stages, results, descriptors)
├── ShaderCompiler.h/cpp       # Per-API compiler back-ends + factory
├── ShaderManager.h/cpp        # Central manager (cache, hot-reload, variants)
├── Example.cpp                # 10 usage examples
├── CMakeLists.txt             # Build configuration
├── README.md                  # This file
└── example_shaders/
    ├── basic.vs.hlsl          # Simple HLSL vertex shader
    ├── basic.ps.hlsl          # Simple HLSL pixel shader
    └── pbr.vs.hlsl            # PBR vertex shader with variant defines
```

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
./ShaderExample
```

## Quick Start

```cpp
#include "ShaderManager.h"

// Create manager for your target API
ShaderManager manager(GraphicsAPI::DirectX11);

// Describe the program
ShaderProgramCPU desc;
desc.vsPath = "shaders/basic.vs.hlsl";
desc.psPath = "shaders/basic.ps.hlsl";

// Compile (or retrieve from cache)
auto shader = manager.compile(desc);
if (shader && shader->isValid) {
    auto& vsBytecode = shader->stageBytecode[uint8_t(ShaderStage::Vertex)];
    // Pass vsBytecode to your graphics API...
}
```

## Caching

```cpp
// First call – compiles from source
auto s1 = manager.compile(desc);   // ~10 ms

// Second call – instant memory cache hit
auto s2 = manager.compile(desc);   // < 1 ms

// Persist to disk for the next run
manager.saveDiskCache("/tmp/shader_cache");

// Load in a subsequent run
manager.loadDiskCache("/tmp/shader_cache");
```

## Shader Variants

```cpp
ShaderProgramCPU base;
base.vsPath = "shaders/standard.vs.hlsl";
base.psPath = "shaders/standard.ps.hlsl";

auto shaders = manager.compileVariants(base, {
    {},                          // Base
    {"SKINNED"},                 // Skinned meshes
    {"INSTANCED"},               // GPU instancing
    {"SKINNED", "INSTANCED"},    // Both
    {"ALPHA_TEST"}               // Alpha testing
});
```

## Hot-Reload

```cpp
desc.enableHotReload = true;
manager.setReloadCallback([](const std::string& key) {
    // Recreate GPU resources with the new bytecode...
});

while (running) {
    manager.update(); // checks file timestamps; recompiles if changed
    render();
}
```

## Preprocessor Defines

```cpp
desc.defines = {
    {"MAX_LIGHTS",      "4"},
    {"USE_NORMAL_MAP",  "1"},
    {"SHADOW_MAP_SIZE", "2048"}
};
```

## Statistics

```cpp
const auto& stats = manager.getStatistics();
std::cout << "Compilations : " << stats.compilationCount  << "\n";
std::cout << "Cache hits   : " << stats.cacheHits         << "\n";
std::cout << "Avg time     : " << stats.averageCompileTime << " ms\n";
```

## In-Memory Source Compilation

Compile shaders from in-memory source strings without touching the filesystem:

```cpp
ShaderProgramSource src;
src.vsSource = R"(float4 VS() : SV_Position { return float4(0,0,0,1); })";
src.psSource = R"(float4 PS() : SV_Target   { return float4(1,0,0,1); })";

auto shader = manager.compileFromSource(src);
if (shader && shader->isValid) {
    // use bytecode...
}
```

## Async Compilation

Offload compilation to a background thread using `std::future`:

```cpp
// File-based async
auto fut1 = manager.compileAsync(desc);

// Source-string async
auto fut2 = manager.compileFromSourceAsync(src);

// ... do other work ...

auto shader1 = fut1.get(); // blocks until done
auto shader2 = fut2.get();
```

## Bounded Cache

Limit the number of in-memory cache entries. The least-recently-used shader
is evicted automatically whenever the limit is exceeded:

```cpp
manager.setCacheCapacity(256); // keep at most 256 compiled shaders

// Passing 0 disables the limit (the default).
manager.setCacheCapacity(0);
```

## Integrating Real Compilers

The back-ends in `ShaderCompiler.cpp` ship with **mock implementations** that return the raw source as bytecode. To use real compilers:

| API        | Library / Tool          | Replace in                          |
|------------|-------------------------|-------------------------------------|
| DirectX 11 | `D3DCompiler.lib`       | `DirectXShaderCompiler::compileSource` |
| DirectX 12 | DXC (`dxcompiler.dll`)  | `DirectXShaderCompiler::compileSource` |
| OpenGL     | `glslang`               | `GLSLShaderCompiler::compileSource`    |
| Vulkan     | `shaderc` / SPIRV-Tools | `SPIRVShaderCompiler::compileSource`   |
| Metal      | `Metal.framework`       | `MetalShaderCompiler::compileSource`   |

Both `compile()` (file-based) and `compileSource()` (in-memory string) delegate to the same back-end
once the source is loaded, so only `compileSource()` needs to be updated per back-end.

## Supported Shader Stages

| Enum                  | Stage                        |
|-----------------------|------------------------------|
| `ShaderStage::Vertex` | Vertex shader                |
| `ShaderStage::Pixel`  | Pixel / fragment shader      |
| `ShaderStage::Compute`| Compute shader               |
| `ShaderStage::Geometry`| Geometry shader             |
| `ShaderStage::Hull`   | Hull / tessellation-control  |
| `ShaderStage::Domain` | Domain / tessellation-eval   |

## License

See the repository `LICENSE` file.
