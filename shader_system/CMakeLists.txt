cmake_minimum_required(VERSION 3.20)
project(ShaderSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(ShaderSystemLib STATIC
    ShaderTypes.cpp
    ShaderCompiler.cpp
    ShaderManager.cpp
)

target_include_directories(ShaderSystemLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Example executable
# ---------------------------------------------------------------------------

add_executable(ShaderExample Example.cpp)
target_link_libraries(ShaderExample PRIVATE ShaderSystemLib)

# On MSVC link against D3DCompiler when building for DirectX (optional)
if(MSVC)
    target_link_libraries(ShaderExample PRIVATE d3dcompiler.lib)
endif()
