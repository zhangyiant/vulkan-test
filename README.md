# Vulkan Colored Pyramid

This is a C++20 Vulkan sample program that renders a continuously rotating tetrahedron. The four vertices use different colors, and the GPU smoothly interpolates them across each triangular face.

## Requirements

- CMake 3.21 or newer
- Conan 2
- A C++20-capable compiler
- Vulkan SDK (includes the Vulkan loader, headers, and `glslc`)
- A Vulkan-capable graphics driver

On Windows, install the Vulkan SDK first and make sure `VULKAN_SDK` points to the SDK installation directory. GLFW and GLM are managed by Conan, while Vulkan itself is provided by the SDK.

## Windows build

Run the following in Visual Studio Developer PowerShell in the project directory, or in any terminal with the C++ toolchain configured:

```powershell
conan profile detect --force
conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build/app "-DCMAKE_TOOLCHAIN_FILE=build/build/generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build/app --config Release
.\build\app\Release\VulkanPyramid.exe
```

If you are using a single-config generator, the executable is typically located at `build/app/VulkanPyramid.exe`.

## Linux build

Install the Vulkan SDK and a Vulkan-capable graphics driver, then run:

```sh
conan profile detect --force
conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build/app -DCMAKE_TOOLCHAIN_FILE=build/build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/app --config Release
./build/app/VulkanPyramid
```

The example window has a fixed size; closing the window exits the render loop.