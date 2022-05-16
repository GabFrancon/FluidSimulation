#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//local
#include "vk_mesh.h"
#include "vk_texture.h"
#include "vk_pipeline.h"

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

static const uint32_t WIDTH = 1200;
static const uint32_t HEIGHT = 900;
static const int MAX_FRAMES_IN_FLIGHT = 2;

static const std::string MODEL_PATH           = "models/viking_room.obj";
static const std::string TEXTURE_PATH         = "textures/viking_room.png";
static const std::string VERTEX_SHADER__PATH  = "shaders/vert.spv";
static const std::string FRAGMENT_SHADER_PATH = "shaders/frag.spv";

static const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
static const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
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
    /*-----------------------------------------MAIN FUNCTIONS----------------------------------------*/

    void init();
    void run();
    void draw();
    void cleanup();

private:

    /*-----------------------------------------CLASS MEMBERS-----------------------------------------*/

    // Interface
    GLFWwindow* window;                                         // window to present rendered images

    // Core Vulkan
    VkInstance instance;                                        // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger;                    // Vulkan debug output handle
    VkSurfaceKHR surface;                                       // Vulkan window surface
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;           // GPU chosen as default device
    VkDevice device;                                            // Vulkan device for commands

    // Swap chain
    VkSwapchainKHR swapChain;                                   // Vulkan rendered images handle
    std::vector<VkImage> swapChainImages;                       // array of images from the swapchain
    std::vector<VkImageView> swapChainImageViews;               // array of image-views from the swapchain
    VkFormat swapChainImageFormat;                              // image format expected by the windowing system
    VkExtent2D swapChainExtent;                                 // dimensions of the swapchain

    // Render pass
    VkRenderPass renderPass;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // Commands
    VkCommandPool commandPool;                                  // background allocator for command buffers
    std::vector<VkCommandBuffer> commandBuffers;                // array of recorded commands to submit to the GPU
    VkQueue graphicsQueue;                                      // GPU's port we submit graphics commands into
    VkQueue presentQueue;                                       // GPU's port we submit presentation commands into

    // Framebuffers
    std::vector<VkFramebuffer> swapChainFramebuffers;
    ImageMap colorImage;
    ImageMap depthImage;
    bool framebufferResized = false;

    // Sync structures
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // Scene resources
    Texture texture;
    Mesh mesh;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<AllocatedBuffer> uniformBuffers;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // Graphics pipelines
    VkPipelineLayout pipelineLayout;
    VkPipeline solidPipeline;
    VkPipeline wireframePipeline;
    bool wireframeModeOn = false;

    // Utils
    uint32_t currentFrame = 0;



    /*------------------------------------------INIT FUNCTIONS-----------------------------------------*/

    void initInterface();
    void initVulkan();
    void initSwapChain();
    void initRenderPass();
    void initCommands();
    void initFramebuffers();
    void initSyncStructures();
    void initScene();
    void initDescriptors();
    void initGraphicsPipelines();



    /*-----------------------------------------HELPER FUNCTIONS----------------------------------------*/

    // Interface
    void createWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Vulkan
    void createInstance();
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    void createSurface();
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    int rateDeviceSuitability(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // Swap chain
    void createSwapChain();
    void cleanupSwapChain();
    void recreateSwapChain();
    void createImageViews();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Render pass
    void createDefaultRenderPass();

    // Commands
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Frambuffers
    void createFramebuffers();
    void createColorResources();
    void createDepthResources();
    VkFormat findDepthFormat();

    // Sync structures
    void createSyncObjects();

    // Scene
    void loadTextures();
    void loadMeshes();
    void uploadMesh(Mesh& mesh);
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);

    // Descriptors
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

    // Graphics pipelines
    void createPipelineLayout();
    VkPipeline createPipeline(const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPolygonMode polygonMode);
};

