# AGENTS.md

## Project context

This repository contains a small C++20 Vulkan sample named `VulkanPyramid`. It renders a rotating colored pyramid and compiles the GLSL shaders to SPIR-V during the CMake build.

## Working rules

- Keep source edits in `src/` and `shaders/`.
- Do not modify generated files under `build/`; those are produced by Conan/CMake and should be treated as build output.
- Preserve the current Vulkan + GLFW + GLM dependency stack unless the task specifically requires changing dependencies.
- Prefer minimal, targeted edits. Avoid broad refactors or unrelated cleanup during bug-fix work.

## Build and validation

Use the project’s native build pipeline before claiming the app is working:

```powershell
conan profile detect --force
conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build/build -DCMAKE_TOOLCHAIN_FILE=build/build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/build --config Release
```

If you change the Vulkan pipeline or shader source, rebuild and verify the binary still runs without shader or validation errors.

## Shader notes

- Source shaders live in `shaders/pyramid.vert` and `shaders/pyramid.frag`.
- The CMake build compiles these into SPIR-V under the build output directory.
- Keep shader names and entry points aligned with `src/main.cpp`.

## Preferred change scope

- For rendering issues: inspect `src/main.cpp` first, then the shader source if the problem is graphical.
- For build problems: check the Conan/CMake configuration and ensure the Vulkan SDK is available before changing code.
- For documentation changes: keep user-facing instructions in `README.md`; use this file for agent-only operational guidance.
