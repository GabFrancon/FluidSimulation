#pragma once

#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_command.h"
#include "vk_descriptor.h"
#include "vk_material.h"
#include "vk_mesh.h"
#include "vk_texture.h"
#include "vk_camera.h"
#include "vk_tools.h"

#include "../SPH/sph_solver3D.h"



// global constants
static const uint32_t WIDTH  = 1600;
static const uint32_t HEIGHT = 1200;

static const int MAX_OBJECTS_RENDERED  = 80000;
static const int MAX_MATERIALS_CREATED = 10;
static const int MAX_FRAMES_IN_FLIGHT  = 2;

static const std::string SPHERE_MODEL_PATH  = "assets/models/sphere.obj";
static const std::string CUBE_MODEL_PATH    = "assets/models/cube.obj";

static const std::string WATER_TEXTURE_PATH = "assets/textures/water.jpg";
static const std::string GLASS_TEXTURE_PATH = "assets/textures/glass.jpg";

static const std::string BASIC_VERT_SHADER_PATH     = "shaders/basic_vert.spv";
static const std::string INSTANCED_VERT_SHADER_PATH = "shaders/instanced_vert.spv";
static const std::string COLORED_FRAG_SHADER_PATH   = "shaders/colored_frag.spv";
static const std::string TEXTURED_FRAG_SHADER_PATH  = "shaders/textured_frag.spv";


// struct definitions

struct SceneInfo {
    glm::vec3 lightPosition;
    glm::vec3 lightColor;
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 modelMatrix;
    glm::vec3 albedoColor;
};


class VulkanEngine {

public:
    /*----------------------------------------MAIN FUNCTIONS----------------------------------------*/

    void init();
    void run();
    void update();
    void draw();
    void cleanup();

private:

    /*-----------------------------------------CLASS MEMBERS----------------------------------------*/

    // Interface
    GLFWwindow* window;

    // Vulkan core
    VulkanContext context;
    VulkanSwapChain swapChain;
    VkRenderPass renderPass;

    // Commands
    VkCommandPool commandPool;
    std::vector<VulkanCommand> commands;
    uint32_t currentFrame = 0;

    // Descriptors
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout globalSetLayout, objectsSetLayout, textureSetLayout;
    std::vector<VulkanDescriptor> descriptors;

    // Assets
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Material> materials;

    // Scene objects
    SceneInfo sceneInfo;
    Camera camera;
    std::vector<RenderObject> renderables;

    // Logic
    IISPHsolver3D sphSolver;

    float appTimer         = 0.0f;
    float lastClockTime    = 0.0f;
    float currentClockTime = 0.0f;

    // Flags
    bool appTimerStopped    = true;
    bool recordingModeOn    = false;
    bool framebufferResized = false;
    bool wireframeViewOn    = false;



    /*--------------------------------------CLASS FUNCTIONS--------------------------------------*/

    // Interface
    void        initInterface();
    void        createWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    // Vulkan core
    void initContext();
    void initSwapChain();
    void createRenderPass();
    void cleanupSwapChain();
    void recreateSwapChain();

    // Commands
    void createCommandPool();
    void initCommands();
    void createFramebuffers();

    // Descriptors
    void createDescriptorPool();
    void createDescriptorLayouts();
    void initDescriptors();
    void mapCameraData();
    void mapSceneData();
    void mapObjectsData();

    // Assets
    void      initAssets();
    void      loadTextures();
    void      loadMeshes();
    void      createMaterial(const std::string name, Texture* texture, VulkanPipeline pipeline);
    void      switchViewMode();
    Texture*  getTexture(const std::string& name);
    Mesh*     getMesh(const std::string& name);
    Material* getMaterial(const std::string& name);

    // Scene Rendering
    void initScene();
    void updateScene();
    void renderScene(VkCommandBuffer commandBuffer);
    void drawSingleObject(VkCommandBuffer commandBuffer, int i);
    void drawInstanced(VkCommandBuffer commandBuffer, RenderObject object, int instanceCount);

    // Exportation
    void savesFrames();
    void saveScreenshot(const char* filename);
};
