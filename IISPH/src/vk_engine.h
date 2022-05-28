#pragma once

#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_command.h"
#include "vk_descriptor.h"
#include "vk_material.h"
#include "vk_mesh.h"
#include "vk_texture.h"
#include "vk_tools.h"

#include "SPH/wcsph_solver.h"
#include "SPH/iisph_solver.h"



// global constants
static const uint32_t WIDTH  = 1920;
static const uint32_t HEIGHT = 1080;

static const int MAX_OBJECTS_RENDERED  = 5000;
static const int MAX_MATERIALS_CREATED = 10;
static const int MAX_FRAMES_IN_FLIGHT  = 2;

static const std::string SPHERE_MODEL_PATH  = "assets/models/sphere.obj";
static const std::string WATER_TEXTURE_PATH = "assets/textures/water.jpg";

static const std::string BASIC_VERT_SHADER_PATH     = "shaders/basic_vert.spv";
static const std::string INSTANCED_VERT_SHADER_PATH = "shaders/instanced_vert.spv";
static const std::string COLORED_FRAG_SHADER_PATH   = "shaders/colored_frag.spv";
static const std::string TEXTURED_FRAG_SHADER_PATH  = "shaders/textured_frag.spv";


// struct definitions

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 modelMatrix;
    glm::vec3 albedoColor;
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


class VulkanEngine {

public:
    /*----------------------------------------MAIN FUNCTIONS----------------------------------------*/

    void init();
    void run();
    void update();
    void draw();
    void cleanup();

private:

    /*---------------------------------------CLASS MEMBERS---------------------------------------*/

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
    Camera camera;
    std::vector<RenderObject> renderables;

    // Logic
    //WCSPHsolver solver;
    IISPHsolver solver;
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
    void drawObjects(VkCommandBuffer commandBuffer, RenderObject* firstObject, int objectsCount);
    void drawInstanced(VkCommandBuffer commandBuffer, RenderObject object, int instanceCount);

    // Exportation
    void savesFrames();
    void saveScreenshot(const char* filename);
};
