#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t windowWidth = 960;
constexpr uint32_t windowHeight = 640;
constexpr int maxFramesInFlight = 2;

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

constexpr std::array<Vertex, 4> vertices = {{
    {{0.0F, 0.9F, 0.0F}, {1.0F, 0.22F, 0.16F}},
    {{-0.8F, -0.65F, 0.58F}, {0.12F, 0.82F, 0.48F}},
    {{0.8F, -0.65F, 0.58F}, {0.18F, 0.48F, 1.0F}},
    {{0.0F, -0.65F, -0.82F}, {1.0F, 0.78F, 0.18F}},
}};

constexpr std::array<uint16_t, 12> indices = {
    0, 1, 2,
    0, 2, 3,
    0, 3, 1,
    1, 3, 2,
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool complete() const {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapChainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanPyramid {
public:
    VulkanPyramid() {
        try {
            initialize();
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~VulkanPyramid() {
        cleanup();
    }

    void run() {
        while (glfwWindowShouldClose(window) == GLFW_FALSE) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

private:
    GLFWwindow* window = nullptr;
    bool glfwInitialized = false;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkFormat swapChainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent{};
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores{};
    std::array<VkSemaphore, maxFramesInFlight> renderFinishedSemaphores{};
    std::array<VkFence, maxFramesInFlight> inFlightFences{};
    size_t currentFrame = 0;

    void initialize() {
        glfwInitVulkanLoader(vkGetInstanceProcAddr);
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwInitialized = true;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(static_cast<int>(windowWidth),
                                  static_cast<int>(windowHeight),
                                  "Vulkan Pyramid | interpolated vertex colors",
                                  nullptr, nullptr);
        if (window == nullptr) {
            throw std::runtime_error("Failed to create GLFW window");
        }

        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createDepthResources();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void cleanup() noexcept {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            for (auto semaphore : imageAvailableSemaphores) {
                if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
            }
            for (auto semaphore : renderFinishedSemaphores) {
                if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
            }
            for (auto fence : inFlightFences) {
                if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
            }
            if (indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, indexBuffer, nullptr);
            if (indexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, indexBufferMemory, nullptr);
            if (vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, vertexBuffer, nullptr);
            if (vertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, vertexBufferMemory, nullptr);
            if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
            for (auto framebuffer : framebuffers) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, graphicsPipeline, nullptr);
            if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, renderPass, nullptr);
            if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
            if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
            if (depthImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthImageMemory, nullptr);
            for (auto imageView : swapChainImageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }
            if (swapChain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapChain, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
        if (window != nullptr) glfwDestroyWindow(window);
        if (glfwInitialized) glfwTerminate();
    }

    void createInstance() {
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "Vulkan Pyramid";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (extensions == nullptr) {
            throw std::runtime_error("GLFW could not provide Vulkan instance extensions");
        }

        std::vector<const char*> enabledExtensions(extensions, extensions + extensionCount);
        VkInstanceCreateFlags instanceFlags = 0;
#ifdef __APPLE__
        enabledExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.flags = instanceFlags;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan window surface");
        }
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice candidate) const {
        QueueFamilyIndices result;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());

        for (uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) result.graphics = i;
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport);
            if (presentSupport == VK_TRUE) result.present = i;
            if (result.complete()) break;
        }
        return result;
    }

    SwapChainSupport querySwapChainSupport(VkPhysicalDevice candidate) const {
        SwapChainSupport support;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &support.capabilities);
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &count, nullptr);
        support.formats.resize(count);
        if (count > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &count, support.formats.data());
        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &count, nullptr);
        support.presentModes.resize(count);
        if (count > 0) vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &count, support.presentModes.data());
        return support;
    }

    bool hasSwapChainExtension(VkPhysicalDevice candidate) const {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data());
        return std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) {
            return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("No Vulkan-capable GPU was found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        for (auto candidate : devices) {
            const auto support = querySwapChainSupport(candidate);
            if (findQueueFamilies(candidate).complete() && hasSwapChainExtension(candidate) &&
                !support.formats.empty() && !support.presentModes.empty()) {
                physicalDevice = candidate;
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("No GPU supports the required graphics and presentation features");
        }
    }

    void createLogicalDevice() {
        const auto families = findQueueFamilies(physicalDevice);
        const std::array<uint32_t, 2> uniqueFamilies = {families.graphics.value(), families.present.value()};
        std::vector<uint32_t> queues;
        for (const auto family : uniqueFamilies) {
            if (std::find(queues.begin(), queues.end(), family) == queues.end()) queues.push_back(family);
        }

        constexpr float priority = 1.0F;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (const auto family : queues) {
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            queueInfos.push_back(queueInfo);
        }

        VkPhysicalDeviceFeatures features{};
        const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.pEnabledFeatures = &features;
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = &extension;

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }
        vkGetDeviceQueue(device, families.graphics.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, families.present.value(), 0, &presentQueue);
    }

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
        }
        return formats.front();
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
        for (const auto mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return extent;
    }

    void createSwapChain() {
        const auto support = querySwapChainSupport(physicalDevice);
        const auto surfaceFormat = chooseSurfaceFormat(support.formats);
        const auto presentMode = choosePresentMode(support.presentModes);
        swapChainExtent = chooseExtent(support.capabilities);

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        const auto families = findQueueFamilies(physicalDevice);
        const std::array<uint32_t, 2> queueFamilyIndices = {families.graphics.value(), families.present.value()};
        VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (families.graphics != families.present) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain");
        }
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainFormat = surfaceFormat.format;
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(device, &createInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
        return view;
    }

    void createImageViews() {
        swapChainImageViews.reserve(swapChainImages.size());
        for (const auto image : swapChainImages) {
            swapChainImageViews.push_back(createImageView(image, swapChainFormat, VK_IMAGE_ASPECT_COLOR_BIT));
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1U << i)) &&
                (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
        }
        throw std::runtime_error("Failed to find a compatible Vulkan memory type");
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& memory) const {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image");
        }
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &allocation, nullptr, &memory) != VK_SUCCESS ||
            vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory");
        }
    }

    void createDepthResources() {
        constexpr auto depthFormat = VK_FORMAT_D32_SFLOAT;
        createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        subpass.pDepthStencilAttachment = &depthReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = dependency.srcStageMask;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo createInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
        if (vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass");
        }
    }

    VkShaderModule createShaderModule(const std::string& path) const {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file) throw std::runtime_error("Failed to open shader: " + path);
        const auto fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> code(fileSize);
        file.seekg(0);
        file.read(code.data(), static_cast<std::streamsize>(fileSize));
        if (!file || fileSize == 0 || fileSize % sizeof(uint32_t) != 0) {
            throw std::runtime_error("Invalid SPIR-V shader: " + path);
        }
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module: " + path);
        }
        return module;
    }

    void createGraphicsPipeline() {
        const auto vertexShader = createShaderModule(std::string(PYRAMID_SHADER_DIR) + "/pyramid.vert.spv");
        VkShaderModule fragmentShader = VK_NULL_HANDLE;
        try {
            fragmentShader = createShaderModule(std::string(PYRAMID_SHADER_DIR) + "/pyramid.frag.spv");
        } catch (...) {
            vkDestroyShaderModule(device, vertexShader, nullptr);
            throw;
        }

        VkPipelineShaderStageCreateInfo vertexStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexShader;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentShader;
        fragmentStage.pName = "main";
        const std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertexStage, fragmentStage};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        const std::array<VkVertexInputAttributeDescription, 2> attributes = {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
        }};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0F;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthState.depthTestEnable = VK_TRUE;
        depthState.depthWriteEnable = VK_TRUE;
        depthState.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(glm::mat4);
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;
        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(device, fragmentShader, nullptr);
            vkDestroyShaderModule(device, vertexShader, nullptr);
            throw std::runtime_error("Failed to create pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthState;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        const auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
        vkDestroyShaderModule(device, fragmentShader, nullptr);
        vkDestroyShaderModule(device, vertexShader, nullptr);
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to create graphics pipeline");
    }

    void createFramebuffers() {
        framebuffers.reserve(swapChainImageViews.size());
        for (const auto imageView : swapChainImageViews) {
            const std::array<VkImageView, 2> attachments = {imageView, depthImageView};
            VkFramebufferCreateInfo createInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            createInfo.renderPass = renderPass;
            createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            createInfo.width = swapChainExtent.width;
            createInfo.height = swapChainExtent.height;
            createInfo.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            if (vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
            framebuffers.push_back(framebuffer);
        }
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        createInfo.queueFamilyIndex = findQueueFamilies(physicalDevice).graphics.value();
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) const {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &allocation, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory");
        }
    }

    void createVertexBuffer() {
        const VkDeviceSize size = sizeof(vertices);
        createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);
        void* data = nullptr;
        vkMapMemory(device, vertexBufferMemory, 0, size, 0, &data);
        std::memcpy(data, vertices.data(), static_cast<size_t>(size));
        vkUnmapMemory(device, vertexBufferMemory);
    }

    void createIndexBuffer() {
        const VkDeviceSize size = sizeof(indices);
        createBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     indexBuffer, indexBufferMemory);
        void* data = nullptr;
        vkMapMemory(device, indexBufferMemory, 0, size, 0, &data);
        std::memcpy(data, indices.data(), static_cast<size_t>(size));
        vkUnmapMemory(device, indexBufferMemory);
    }

    void createCommandBuffers() {
        commandBuffers.resize(maxFramesInFlight);
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = commandPool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        if (vkAllocateCommandBuffers(device, &allocation, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < maxFramesInFlight; ++i) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create frame synchronization objects");
            }
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        const std::array<VkClearValue, 2> clearValues = {{
            {{{0.035F, 0.055F, 0.09F, 1.0F}}},
            {.depthStencil = {1.0F, 0}},
        }};
        VkRenderPassBeginInfo renderInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderInfo.renderPass = renderPass;
        renderInfo.framebuffer = framebuffers[imageIndex];
        renderInfo.renderArea.extent = swapChainExtent;
        renderInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.maxDepth = 1.0F;
        VkRect2D scissor{{0, 0}, swapChainExtent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        const glm::mat4 model = glm::rotate(glm::mat4(1.0F), elapsed * glm::radians(35.0F), glm::vec3(0.0F, 1.0F, 0.0F));
        const glm::mat4 view = glm::lookAt(glm::vec3(2.4F, 1.9F, 3.1F), glm::vec3(0.0F), glm::vec3(0.0F, 1.0F, 0.0F));
        glm::mat4 projection = glm::perspective(glm::radians(48.0F),
            static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1F, 10.0F);
        projection[1][1] *= -1.0F;
        const glm::mat4 mvp = projection * view * model;
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(mvp), &mvp);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex = 0;
        const auto acquireResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) return;
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command");
        }

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;
        const auto presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR &&
            presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
            throw std::runtime_error("Failed to present swap chain image");
        }
        currentFrame = (currentFrame + 1) % maxFramesInFlight;
    }

    const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

}

int main() {
    try {
        VulkanPyramid app;
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "VulkanPyramid: " << error.what() << '\n';
        return 1;
    }
    return 0;
}