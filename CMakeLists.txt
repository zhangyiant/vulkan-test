cmake_minimum_required(VERSION 3.21)

project(VulkanPyramid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

find_program(GLSLC_EXECUTABLE NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid.frag"
)
set(SHADER_BINARIES)

foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    set(SHADER_BINARY "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${SHADER_BINARY}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.0
                "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
        DEPENDS "${SHADER_SOURCE}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES "${SHADER_BINARY}")
endforeach()

add_custom_target(compile_shaders DEPENDS ${SHADER_BINARIES})

add_executable(VulkanPyramid src/main.cpp)
add_dependencies(VulkanPyramid compile_shaders)
target_link_libraries(VulkanPyramid PRIVATE Vulkan::Vulkan glfw glm::glm-header-only)
target_compile_definitions(VulkanPyramid PRIVATE
    PYRAMID_SHADER_DIR="${SHADER_OUTPUT_DIR}"
    GLFW_INCLUDE_VULKAN
)

if(MSVC)
    target_compile_options(VulkanPyramid PRIVATE /W4 /permissive-)
else()
    target_compile_options(VulkanPyramid PRIVATE -Wall -Wextra -Wpedantic)
endif()