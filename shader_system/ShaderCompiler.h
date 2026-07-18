#pragma once

#include "ShaderTypes.h"
#include <memory>

// ---------------------------------------------------------------------------
// Abstract base – one concrete subclass per graphics API / compiler back-end
// ---------------------------------------------------------------------------

class IShaderCompiler {
public:
    virtual ~IShaderCompiler() = default;

    // Compile a single shader stage from a source file.
    // path   – source file path
    // entry  – entry-point function name
    // stage  – which pipeline stage
    // defines– preprocessor defines (name, value)
    virtual ShaderCompileResult compile(
        const std::string&                                    path,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) = 0;

    // Compile a single shader stage from an in-memory source string.
    // source – shader source code
    // entry  – entry-point function name
    // stage  – which pipeline stage
    // defines– preprocessor defines (name, value)
    virtual ShaderCompileResult compileSource(
        const std::string&                                    source,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) = 0;
};

// ---------------------------------------------------------------------------
// DirectX 11 / 12 compiler  (wraps D3DCompile / DXC)
// ---------------------------------------------------------------------------

class DirectXShaderCompiler : public IShaderCompiler {
public:
    explicit DirectXShaderCompiler(GraphicsAPI api);
    ShaderCompileResult compile(
        const std::string&                                    path,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;

    ShaderCompileResult compileSource(
        const std::string&                                    source,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;

private:
    GraphicsAPI api_;
    // Returns the HLSL shader-model target string, e.g. "vs_5_0"
    std::string targetProfile(ShaderStage stage) const;
};

// ---------------------------------------------------------------------------
// OpenGL / GLSL compiler  (wraps glslang)
// ---------------------------------------------------------------------------

class GLSLShaderCompiler : public IShaderCompiler {
public:
    GLSLShaderCompiler();
    ShaderCompileResult compile(
        const std::string&                                    path,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;

    ShaderCompileResult compileSource(
        const std::string&                                    source,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;
};

// ---------------------------------------------------------------------------
// Vulkan / SPIR-V compiler  (wraps glslangValidator or shaderc)
// ---------------------------------------------------------------------------

class SPIRVShaderCompiler : public IShaderCompiler {
public:
    SPIRVShaderCompiler();
    ShaderCompileResult compile(
        const std::string&                                    path,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;

    ShaderCompileResult compileSource(
        const std::string&                                    source,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;
};

// ---------------------------------------------------------------------------
// Metal compiler  (wraps Metal.framework / xcrun metal)
// ---------------------------------------------------------------------------

class MetalShaderCompiler : public IShaderCompiler {
public:
    MetalShaderCompiler();
    ShaderCompileResult compile(
        const std::string&                                    path,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;

    ShaderCompileResult compileSource(
        const std::string&                                    source,
        const std::string&                                    entry,
        ShaderStage                                           stage,
        const std::vector<std::pair<std::string,std::string>>& defines) override;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

class ShaderCompilerFactory {
public:
    // Returns the appropriate compiler for the requested API.
    static std::unique_ptr<IShaderCompiler> create(GraphicsAPI api);
};
