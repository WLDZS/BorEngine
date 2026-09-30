cmake_minimum_required(VERSION 4.3)
project(BorEngine)

set(CMAKE_CXX_STANDARD 26)

add_executable(BorEngine
        src/main.cpp
        src/Application.cpp
        src/Application.h
        src/GLFW/GLFWRuntime.cpp
        src/GLFW/GLFWRuntime.h
        src/Vulkan/VulkanRuntime.cpp
        src/Vulkan/VulkanRuntime.h
)
add_subdirectory(ThirdParty/glfw-3.5.1)

find_package(Vulkan REQUIRED)
target_compile_definitions(BorEngine PRIVATE VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)
target_link_libraries(BorEngine PRIVATE glfw Vulkan::Vulkan)

if (MINGW)
    target_link_libraries(BorEngine PRIVATE stdc++exp)
endif()

# 将 HLSL 编译为 SPIR-V，并跟踪源文件和 #include 文件的变化。
find_program(BOR_ENGINE_DXC_EXECUTABLE
        NAMES dxc
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
              "${Vulkan_INCLUDE_DIR}/../Bin" "${Vulkan_INCLUDE_DIR}/../bin"
        REQUIRED
)

set(BOR_ENGINE_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shader")
target_compile_definitions(BorEngine PRIVATE
        BOR_ENGINE_SHADER_DIR="${BOR_ENGINE_SHADER_OUTPUT_DIR}"
)

function(borengine_compile_shader source entry profile output)
    add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOR_ENGINE_SHADER_OUTPUT_DIR}"
            COMMAND "${BOR_ENGINE_DXC_EXECUTABLE}"
                    -spirv -fspv-target-env=vulkan1.3
                    -T "${profile}" -E "${entry}"
                    "${source}" -Fo "${output}"
            # DXC 的 -MD 只生成依赖信息，需要单独执行一次。
            COMMAND "${BOR_ENGINE_DXC_EXECUTABLE}"
                    -spirv -fspv-target-env=vulkan1.3
                    -T "${profile}" -E "${entry}"
                    -MD -MF "${output}.d"
                    "${source}" -Fo "${output}"
            DEPENDS "${source}" "${BOR_ENGINE_DXC_EXECUTABLE}"
            DEPFILE "${output}.d"
            COMMENT "编译 HLSL 着色器：${entry}"
            VERBATIM
    )
endfunction()

set(BOR_ENGINE_TRIANGLE_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/Shader/triangle.hlsl")
set(BOR_ENGINE_VERTEX_SHADER "${BOR_ENGINE_SHADER_OUTPUT_DIR}/triangle.vert.spv")
set(BOR_ENGINE_FRAGMENT_SHADER "${BOR_ENGINE_SHADER_OUTPUT_DIR}/triangle.frag.spv")

borengine_compile_shader(
        "${BOR_ENGINE_TRIANGLE_SHADER}" VSMain vs_6_0 "${BOR_ENGINE_VERTEX_SHADER}"
)
borengine_compile_shader(
        "${BOR_ENGINE_TRIANGLE_SHADER}" PSMain ps_6_0 "${BOR_ENGINE_FRAGMENT_SHADER}"
)

add_custom_target(BorEngineShaders
        DEPENDS "${BOR_ENGINE_VERTEX_SHADER}" "${BOR_ENGINE_FRAGMENT_SHADER}"
)
add_dependencies(BorEngine BorEngineShaders)
target_sources(BorEngine PRIVATE "${BOR_ENGINE_TRIANGLE_SHADER}")
