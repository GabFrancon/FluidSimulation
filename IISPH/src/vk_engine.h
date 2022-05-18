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

// std
#include <chrono>
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


// global constants
static const uint32_t WIDTH = 1200;
static const uint32_t HEIGHT = 900;
static const int MAX_FRAMES_IN_FLIGHT = 2;

static const std::string MODEL_PATH           = "models/viking_room.obj";
static const std::string TEXTURE_PATH         = "textures/viking_room.png";
static const std::string STATUE_TEXTURE_PATH  = "textures/texture.jpg";

static const std::string VERTEX_SHADER__PATH  = "shaders/vert.spv";
static const std::string FRAGMENT_SHADER_PATH = "shaders/frag.spv";

static const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
static const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif

// struct definitions
struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet textureDescriptor;

    void destroy(VkDevice device) {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 modelMatrix;
};

struct ObjectData {
    alignas(16) glm::mat4 model;
};

struct Camera {
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
};

struct CameraData {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};




class VulkanEngine {

public:
    /*----------------------------------------MAIN FUNCTIONS----------------------------------------*/

    void init();
    void run();
    void draw();
    void cleanup();

private:

    /*---------------------------------------EDITABLE MEMBERS---------------------------------------*/

    // Descriptors and Uniform values
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout textureSetLayout;

    // Assets
    std::unordered_map <std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Material> materials;

    // Global
    std::vector<VkDescriptorSet> globalDescriptors;
    std::vector<AllocatedBuffer> cameraBuffers;
    std::vector<AllocatedBuffer> objectBuffers;

    // Graphics pipelines
    bool wireframeModeOn = false;

    // Scene objects
    Camera camera;
    std::vector<RenderObject> renderables;
    uint32_t currentFrame = 0;



    /*--------------------------------------EDITABLE FUNCTIONS--------------------------------------*/

    // Interface
    void        initInterface();
    static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Descriptors
    void initDescriptors();
    void createDescriptorPool();
    void createDescriptorLayouts();

    // Assets
    void      loadAssets();
    void      loadTextures();
    void      loadMeshes();
    void      createMaterial(Texture texture, const std::string name);
    void      setMaterialTexture(Texture texture, Material& material);
    void      uploadMesh(Mesh& mesh);
    Texture*  getTexture(const std::string& name);
    Mesh*     getMesh(const std::string& name);
    Material* getMaterial(const std::string& name);

    //Uniform values
    void createUniformBuffers();
    void createUniformSets();
    void mapCameraData(Camera camera);
    void mapObjectData(RenderObject object);

    // Graphics pipelines
    void             initGraphicsPipelines();
    VkPipelineLayout createPipelineLayout();
    VkPipeline       createPipeline(VkPipelineLayout layout, const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPolygonMode polygonMode);

    // Scene Rendering
    void initScene();
    void updateScene();
    void renderScene(VkCommandBuffer commandBuffer);
    void drawObject(VkCommandBuffer commandBuffer, RenderObject* object);








    /*-----------------------------------------CORE MEMBERS-----------------------------------------*/

    // Interface
    GLFWwindow* window;                                         // window to present rendered images

    // Vulkan
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



    /*----------------------------------------CORE FUNCTIONS----------------------------------------*/

    // Interface
    void createWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    // Vulkan
    void initVulkan();
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
    void initSwapChain();
    void createSwapChain();
    void cleanupSwapChain();
    void recreateSwapChain();
    void createImageViews();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Render pass
    void initRenderPass();
    void createDefaultRenderPass();

    // Commands
    void initCommands();
    void createCommandPool();
    void createCommandBuffers();

    // Frambuffers
    void initFramebuffers();
    void createFramebuffers();
    void createColorResources();
    void createDepthResources();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Sync structures
    void initSyncStructures();
    void createSyncObjects();
};


    

