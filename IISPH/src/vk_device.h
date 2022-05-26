#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// std
#include <array>
#include <optional>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <map>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <unordered_map>



#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif

static const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
static const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };



// struct definitions

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VkDeviceMemory allocation;

    void destroy(VkDevice device) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, allocation, nullptr);
    }
};

struct AllocatedImage {
    VkImage image;
    VkDeviceMemory allocation;

    void destroy(VkDevice device) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, allocation, nullptr);
    }
};

struct ImageMap {
    AllocatedImage allocatedImage;
    VkImageView imageView;

    void destroy(VkDevice device) {
        vkDestroyImageView(device, imageView, nullptr);
        allocatedImage.destroy(device);
    }
};



class VulkanDevice {
public:
    VkInstance instance;                                        // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger;                    // Vulkan debug output handle
    VkSurfaceKHR surface;                                       // Vulkan window surface
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;           // GPU chosen as default device
    VkDevice vkDevice;                                          // Vulkan device for commands
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;  // Multisampling count
    VkQueue graphicsQueue;                                      // GPU's port we submit graphics commands into
    VkQueue presentQueue;                                       // GPU's port we submit presentation commands into



    // initialization
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void destroy();

    // commands
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
    void endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer);

    // device properties
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // buffers
    AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // images
    AllocatedImage createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    void copyBufferToImage(VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

private:
    // helper functions
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    VkResult createDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    int rateDeviceSuitability(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();
};