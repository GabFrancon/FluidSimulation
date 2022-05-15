#pragma once

#include "vk_mesh.h"


// third-parties

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


// std

#include <chrono>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <unordered_map>


// general variables

static const uint32_t WIDTH = 1600;
static const uint32_t HEIGHT = 1200;

static const std::string MODEL_PATH = "models/viking_room.obj";
static const std::string TEXTURE_PATH = "textures/viking_room.png";

static const int MAX_FRAMES_IN_FLIGHT = 2;

static const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
static const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
static const bool enableValidationLayers = false;
static const bool displayExtensionInfo = false;
#else
static const bool enableValidationLayers = true;
static const bool displayExtensionInfo = true;
#endif


// useful struct definitions

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

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};



// the Vulkan engine

class VulkanEngine {

public:

    void init();
    void run();
    void cleanup();

private:

    /*-------------------------------------------CLASS MEMBERS-------------------------------------------*/

    // Core Vulkan objects
    VkInstance instance;                                        // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger;                    // Vulkan debug output handle
    VkSurfaceKHR surface;                                       // Vulkan window surface
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;           // GPU chosen as default device
    VkDevice device;                                            // Vulkan device for commands

    // Swap chain
    GLFWwindow* window;                                         // window to present rendered images
    VkSwapchainKHR swapChain;                                   // Vulkan rendered images handle
    std::vector<VkImage> swapChainImages;                       // array of images from the swapchain
    VkFormat swapChainImageFormat;                              // image format expected by the windowing system
    std::vector<VkImageView> swapChainImageViews;               // array of image-views from the swapchain
    VkExtent2D swapChainExtent;                                 // dimensions of the swapchain

    // Render pass
    VkRenderPass renderPass;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // Framebuffers
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    bool framebufferResized = false;

    // Commands
    VkCommandPool commandPool;                                  // background allocator for command buffers
    std::vector<VkCommandBuffer> commandBuffers;                // array of recorded commands to submit to the GPU
    VkQueue graphicsQueue;                                      // GPU's port we submit graphics commands into
    VkQueue presentQueue;                                       // GPU's port we submit presentation commands into

    // Graphics pipelines
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // Sync structures
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // Scene resources
    uint32_t mipLevels;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    Mesh mesh;

    // Utils
    uint32_t currentFrame = 0;



    /*------------------------------------------HELPER FUNCTIONS-----------------------------------------*/

    // Window
    void initWindow();

    // Instance
    void createInstance();
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    // Debug messenger
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

    // Surface
    void createSurface();

    // Physical device
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    int rateDeviceSuitability(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();

    // Logical device
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // Swap chain
    void createSwapChain();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void cleanupSwapChain();
    void recreateSwapChain();

    // Image views
    void createImageViews();
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

    // Render pass
    void createRenderPass();

    // Descriptor set layout
    void createDescriptorSetLayout();

    // Graphics pipeline
    void createGraphicsPipeline();
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // Framebuffers
    void createFramebuffers();

    // Command pool
    void createCommandPool();

    // Color resources
    void createColorResources();

    // Depth resources
    void createDepthResources();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);

    // Texture image
    void createTextureImage();
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // Texture image view
    void createTextureImageView();

    // Texture sampler
    void createTextureSampler();

    // Model
    void loadModel();

    // Vertex buffer
    void createVertexBuffer();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Index buffer
    void createIndexBuffer();

    // Uniform buffers
    void createUniformBuffers();

    // Descriptor pool
    void createDescriptorPool();

    // Descriptor sets
    void createDescriptorSets();

    // Command buffers
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Sync objects
    void createSyncObjects();

    // Draw call
    void drawFrame();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    void updateUniformBuffer(uint32_t currentImage);
};

