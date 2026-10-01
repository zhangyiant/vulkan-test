from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class VulkanPyramidConan(ConanFile):
    name = "vulkan-pyramid"
    version = "1.0.0"
    package_type = "application"

    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "glfw/3.4",
        "glm/1.0.1",
    )
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)