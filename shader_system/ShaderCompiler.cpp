#include "ShaderCompiler.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f)
        throw std::runtime_error("Cannot open shader file: " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<uint8_t> stringToBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// DirectXShaderCompiler
// ---------------------------------------------------------------------------

DirectXShaderCompiler::DirectXShaderCompiler(GraphicsAPI api) : api_(api) {}

std::string DirectXShaderCompiler::targetProfile(ShaderStage stage) const {
    const char* model = (api_ == GraphicsAPI::DirectX12) ? "5_1" : "5_0";
    switch (stage) {
        case ShaderStage::Vertex:   return std::string("vs_") + model;
        case ShaderStage::Pixel:    return std::string("ps_") + model;
        case ShaderStage::Compute:  return std::string("cs_") + model;
        case ShaderStage::Geometry: return std::string("gs_") + model;
        case ShaderStage::Hull:     return std::string("hs_") + model;
        case ShaderStage::Domain:   return std::string("ds_") + model;
        default:                    return "vs_5_0";
    }
}

ShaderCompileResult DirectXShaderCompiler::compile(
    const std::string&                                    path,
    const std::string&                                    entry,
    ShaderStage                                           stage,
    const std::vector<std::pair<std::string,std::string>>& defines)
{
    ShaderCompileResult result;
    try {
        std::string src = readFile(path);

        // -----------------------------------------------------------------
        // Production: replace this block with a real D3DCompile / DXC call.
        //
        //   ID3DBlob* code = nullptr;
        //   ID3DBlob* errors = nullptr;
        //   HRESULT hr = D3DCompile(src.c_str(), src.size(), path.c_str(),
        //                           macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        //                           entry.c_str(), targetProfile(stage).c_str(),
        //                           D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        //                           &code, &errors);
        //   if (FAILED(hr)) { ... }
        // -----------------------------------------------------------------

        (void)entry;
        (void)defines;
        (void)targetProfile(stage); // suppress unused-function warning

        result.bytecode = stringToBytes(src); // mock: ship source as bytecode
        result.success  = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// GLSLShaderCompiler
// ---------------------------------------------------------------------------

GLSLShaderCompiler::GLSLShaderCompiler() {}

ShaderCompileResult GLSLShaderCompiler::compile(
    const std::string&                                    path,
    const std::string&                                    entry,
    ShaderStage                                           stage,
    const std::vector<std::pair<std::string,std::string>>& defines)
{
    ShaderCompileResult result;
    try {
        std::string src = readFile(path);

        // Production: integrate glslang TProgram/TShader here.
        (void)entry;
        (void)stage;
        (void)defines;

        result.bytecode = stringToBytes(src);
        result.success  = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// SPIRVShaderCompiler
// ---------------------------------------------------------------------------

SPIRVShaderCompiler::SPIRVShaderCompiler() {}

ShaderCompileResult SPIRVShaderCompiler::compile(
    const std::string&                                    path,
    const std::string&                                    entry,
    ShaderStage                                           stage,
    const std::vector<std::pair<std::string,std::string>>& defines)
{
    ShaderCompileResult result;
    try {
        std::string src = readFile(path);

        // Production: call shaderc_compiler_compile_into_spv() here.
        (void)entry;
        (void)stage;
        (void)defines;

        result.bytecode = stringToBytes(src);
        result.success  = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IShaderCompiler> ShaderCompilerFactory::create(GraphicsAPI api) {
    switch (api) {
        case GraphicsAPI::DirectX11:
        case GraphicsAPI::DirectX12:
            return std::make_unique<DirectXShaderCompiler>(api);
        case GraphicsAPI::OpenGL:
            return std::make_unique<GLSLShaderCompiler>();
        case GraphicsAPI::Vulkan:
            return std::make_unique<SPIRVShaderCompiler>();
        default:
            throw std::runtime_error("Unsupported graphics API");
    }
}
