#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

enum class ShaderStage : uint8_t {
    Vertex   = 0,
    Pixel    = 1,
    Compute  = 2,
    Geometry = 3,
    Hull     = 4,   // Tessellation Control
    Domain   = 5,   // Tessellation Evaluation
    Count
};

enum class GraphicsAPI : uint8_t {
    DirectX11 = 0,
    DirectX12 = 1,
    OpenGL    = 2,
    Vulkan    = 3,
    Metal     = 4
};

// ---------------------------------------------------------------------------
// Compilation result
// ---------------------------------------------------------------------------

struct ShaderCompileResult {
    bool                     success  = false;
    std::string              errorMsg;
    std::vector<uint8_t>     bytecode;   // Compiled bytecode / SPIR-V
};

// ---------------------------------------------------------------------------
// Reflection data (filled after successful compilation)
// ---------------------------------------------------------------------------

struct ShaderResourceBinding {
    std::string name;
    uint32_t    bindPoint  = 0;
    uint32_t    bindCount  = 1;
    ShaderStage stage      = ShaderStage::Vertex;
};

struct ShaderReflection {
    std::vector<ShaderResourceBinding> constantBuffers;
    std::vector<ShaderResourceBinding> textures;
    std::vector<ShaderResourceBinding> samplers;
    std::vector<ShaderResourceBinding> uavs;
};

// ---------------------------------------------------------------------------
// CPU-side shader program descriptor
// ---------------------------------------------------------------------------

struct ShaderProgramCPU {
    // Stage source paths (empty = stage unused)
    std::string vsPath;   // Vertex shader
    std::string psPath;   // Pixel / Fragment shader
    std::string csPath;   // Compute shader
    std::string gsPath;   // Geometry shader
    std::string tcsPath;  // Hull / Tessellation-control shader
    std::string tesPath;  // Domain / Tessellation-evaluation shader

    // Entry points (default "main" for each stage)
    std::string vsEntry = "main";
    std::string psEntry = "main";
    std::string csEntry = "main";
    std::string gsEntry = "main";
    std::string tcsEntry = "main";
    std::string tesEntry = "main";

    // Preprocessor defines: { "NAME", "VALUE" }
    std::vector<std::pair<std::string, std::string>> defines;

    // Hot-reload
    bool enableHotReload = false;
};

// ---------------------------------------------------------------------------
// Compiled shader program (GPU-ready bytecode for every active stage)
// ---------------------------------------------------------------------------

struct ShaderProgram {
    // Keyed by ShaderStage
    std::unordered_map<uint8_t, std::vector<uint8_t>> stageBytecode;
    ShaderReflection reflection;
    bool             isValid = false;
};
