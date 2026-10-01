# Vulkan 彩色三角锥

这是一个 C++20 Vulkan 示例程序：渲染持续旋转的四面体。四个顶点使用不同颜色，GPU 会在每个三角面上平滑插值。

## 环境要求

- CMake 3.21 或更高版本
- Conan 2
- 支持 C++20 的编译器
- Vulkan SDK（包含 Vulkan loader、头文件和 `glslc`）
- 支持 Vulkan 的显卡驱动

Windows 用户请先安装 Vulkan SDK，并确保 `VULKAN_SDK` 指向 SDK 安装目录。GLFW 和 GLM 由 Conan 管理，Vulkan 本身由 SDK 提供。

## Windows 构建

在项目目录的 Visual Studio Developer PowerShell，或已配置 C++ 编译器的终端中运行：

```powershell
conan profile detect --force
conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build/app "-DCMAKE_TOOLCHAIN_FILE=build/build/generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build/app --config Release
.\build\app\Release\VulkanPyramid.exe
```

如果使用单配置生成器，可执行文件通常位于 `build/app/VulkanPyramid.exe`。

## Linux 构建

安装 Vulkan SDK 和支持 Vulkan 的显卡驱动，然后运行：

```sh
conan profile detect --force
conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build/app -DCMAKE_TOOLCHAIN_FILE=build/build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/app --config Release
./build/app/VulkanPyramid
```

示例窗口固定尺寸；关闭窗口即可退出渲染循环。