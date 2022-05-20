#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>


// local
#include "vk_mesh.h"
#include "vk_texture.h"
#include "WCSPH/wcsph_solver.h"

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
static const uint32_t WIDTH  = 1200;
static const uint32_t HEIGHT = 900;

static const int MAX_FRAMES_OVERLAP = 2;
static const int MAX_OBJECT         = 5000;

static const std::string SPHERE_MODEL_PATH  = "assets/models/sphere.obj";
static const std::string WATER_TEXTURE_PATH = "assets/textures/water.jpg";

static const std::string BASIC_VERT_SHADER_PATH     = "shaders/basic_vert.spv";
static const std::string INSTANCED_VERT_SHADER_PATH = "shaders/instanced_vert.spv";
static const std::string TEXTURED_FRAG_SHADER_PATH  = "shaders/textured_frag.spv";

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

    static glm::mat4 perspective(float fov, float aspect, float near, float far) {
        float f = 1.0f / tan(0.5f * fov);

        glm::mat4 perspectiveProjectionMatrix = {
            f / aspect, 0.0f, 0.0f                       ,  0.0f,

            0.0f      , -f  , 0.0f                       ,  0.0f,

            0.0f      , 0.0f, far / (near - far)         , -1.0f,

            0.0f      , 0.0f, (near * far) / (near - far),  0.0f
        };

        return perspectiveProjectionMatrix;
    }
    static glm::mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
        glm::mat4 orthographicProjectionMatrix = {
            2.0f / (right - left)           , 0.0f                            , 0.0f               , 0.0f,

            0.0f                            , 2.0f / (bottom - top)           , 0.0f               , 0.0f,

            0.0f                            , 0.0f                            , 1.0f / (near - far), 0.0f,

            -(right + left) / (right - left), -(bottom + top) / (bottom - top), near / (near - far), 1.0f
        };

        return orthographicProjectionMatrix;
    }
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

    // Interface
    GLFWwindow* window;

    // Descriptor handlers
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout objectsSetLayout;
    VkDescriptorSetLayout textureSetLayout;

    // Assets
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Material> materials;

    // Uniform buffers and descriptors
    std::vector<VkDescriptorSet> globalDescriptors;
    std::vector<AllocatedBuffer> cameraBuffers;
    std::vector<VkDescriptorSet> objectsDescriptors;
    std::vector<AllocatedBuffer> objectsBuffers;

    // Graphics pipelines
    bool wireframeModeOn  = false;
    uint32_t currentFrame = 0;

    // Scene objects
    Camera camera;
    std::vector<RenderObject> renderables;

    // Logic
    WCSPHSolver solver;
    float appTimer         = 0.0f;
    float lastClockTime    = 0.0f;
    float currentClockTime = 0.0f;
    bool  appTimerStopped  = true;



    /*--------------------------------------EDITABLE FUNCTIONS--------------------------------------*/

    // Interface
    void        initInterface();
    void        createWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
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
    void mapCameraData();
    void mapObjectsData();

    // Graphics pipelines
    void             initGraphicsPipelines();
    VkPipelineLayout createPipelineLayout();
    VkPipeline       createPipeline(VkPipelineLayout layout, const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPolygonMode polygonMode);

    // Scene Rendering
    void initScene();
    void updateScene();
    void renderScene(VkCommandBuffer commandBuffer);
    void drawObjects(VkCommandBuffer commandBuffer, RenderObject* firstObject, int objectsCount);
    void drawInstanced(VkCommandBuffer commandBuffer, RenderObject object, int instanceCount);
    void switchMode();





    /*-----------------------------------------CORE MEMBERS-----------------------------------------*/

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
