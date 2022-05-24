#pragma once

#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_drawhandler.h"
#include "vk_tools.h"
#include "vk_mesh.h"
#include "vk_texture.h"
#include "WCSPH/wcsph_solver.h"



// global constants
static const uint32_t WIDTH  = 1200;
static const uint32_t HEIGHT = 900;

static const int MAX_OBJECTS_RENDERED = 5000;

static const std::string SPHERE_MODEL_PATH  = "assets/models/sphere.obj";
static const std::string WATER_TEXTURE_PATH = "assets/textures/water.jpg";

static const std::string BASIC_VERT_SHADER_PATH     = "shaders/basic_vert.spv";
static const std::string INSTANCED_VERT_SHADER_PATH = "shaders/instanced_vert.spv";
static const std::string COLORED_FRAG_SHADER_PATH   = "shaders/colored_frag.spv";
static const std::string TEXTURED_FRAG_SHADER_PATH  = "shaders/textured_frag.spv";


// struct definitions
struct PipelineBuilder
{
    VkPipelineLayout pipelineLayout;
    VkPipelineShaderStageCreateInfo shaderStages[2];
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineViewportStateCreateInfo viewportState{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    VkPipeline buildPipeline(VkDevice device, VkRenderPass renderPass, const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
        // set vertex shader
        auto vertShaderCode = readFile(vertexShaderPath);
        VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);

        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        // set fragment shader
        auto fragShaderCode = readFile(fragmentShaderPath);
        VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";

        // build pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        return graphicsPipeline;
    }
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }
    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();
        return buffer;
    }
};

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet textureDescriptor;
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 modelMatrix;
    glm::vec3 albedoColor;
};

struct ObjectData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 albedo;
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

    /*---------------------------------------CLASS MEMBERS---------------------------------------*/

    // Interface
    GLFWwindow* window;

    // Vulkan
    VulkanDevice* vulkanDevice;
    VulkanSwapChain* swapChain;
    VkRenderPass renderPass;
    VulkanDrawHandler* vulkanDrawHandler;

    // Descriptors
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout objectsSetLayout;
    VkDescriptorSetLayout textureSetLayout;
    std::vector<VkDescriptorSet> globalDescriptors;
    std::vector<AllocatedBuffer> cameraBuffers;
    std::vector<VkDescriptorSet> objectsDescriptors;
    std::vector<AllocatedBuffer> objectsBuffers;
    uint32_t currentFrame = 0;

    // Assets
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Material> materials;

    // Scene objects
    Camera camera;
    std::vector<RenderObject> renderables;

    // Logic
    WCSPHSolver solver;
    float appTimer         = 0.0f;
    float lastClockTime    = 0.0f;
    float currentClockTime = 0.0f;

    // Flags
    bool appTimerStopped    = true;
    bool recordingModeOn    = false;
    bool framebufferResized = false;
    bool wireframeViewOn    = false;




    /*--------------------------------------EDITABLE FUNCTIONS--------------------------------------*/

    // Interface
    void        initInterface();
    void        createWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Vulkan
    void initDevice();
    void initSwapChain();
    void initRenderPass();
    void initDrawHandler();
    void cleanupSwapChain();
    void recreateSwapChain();

    // Descriptor handlers and data
    void initDescriptors();
    void createDescriptorPool();
    void createDescriptorLayouts();
    void createDescriptorBuffers();
    void createDescriptorSets();
    void mapCameraData();
    void mapObjectsData();

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

    // Graphics pipelines
    void             initGraphicsPipelines();
    VkPipelineLayout createPipelineLayout();
    VkPipeline       createPipeline(VkPipelineLayout layout, const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPolygonMode polygonMode);
    void             switchWireframeView();

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
