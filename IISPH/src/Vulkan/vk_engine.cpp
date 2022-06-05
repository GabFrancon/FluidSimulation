#include "vk_engine.h"


/*------------------------------------------MAIN FUNCTIONS---------------------------------------------*/

void VulkanEngine::init() {
    initInterface();

    initContext();
    initSwapChain();
    createRenderPass();

    createCommandPool();
    initCommands();
    createFramebuffers();

    createDescriptorPool();
    createDescriptorLayouts();
    initDescriptors();

    initAssets();
    initScene();
}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        update();
        draw();
    }
    vkDeviceWaitIdle(context.device);
}

void VulkanEngine::update() {
    updateScene();

    mapCameraData();
    mapSceneData();
    mapObjectsData();
}

void VulkanEngine::draw() {
    VkCommandBuffer cmd = commands[currentFrame].commandBuffer;

    vkWaitForFences(context.device, 1, &commands[currentFrame].inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context.device, swapChain.vkSwapChain, UINT64_MAX, commands[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(context.device, 1, &commands[currentFrame].inFlightFence);

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChain.framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChain.extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    renderScene(cmd);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSemaphore waitSemaphores[] = { commands[currentFrame].imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { commands[currentFrame].renderFinishedSemaphore };


    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, commands[currentFrame].inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit command buffer!");
    }

    VkSwapchainKHR swapChains[] = { swapChain.vkSwapChain };

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(context.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::cleanup() {
    cleanupSwapChain();

    vkDestroyCommandPool(context.device, commandPool, nullptr);
    vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(context.device, globalSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, objectsSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, textureSetLayout, nullptr);

    for (auto& texture : textures)
        texture.second.destroy();

    for (auto& mesh : meshes)
        mesh.second.destroy();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        descriptors[i].destroy();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        commands[i].destroy();

    context.destroy();

    glfwDestroyWindow(window);

    glfwTerminate();
}



/*----------------------------------------CLASS FUNCTIONS--------------------------------------------*/

// Interface
void VulkanEngine::initInterface() {
    createWindow();
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyboardCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void VulkanEngine::createWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "SPH simulation", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
}

void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    engine->framebufferResized = true;
}

void VulkanEngine::keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_V && action == GLFW_PRESS) {
        engine->wireframeViewOn = !engine->wireframeViewOn;
        engine->switchViewMode();
    }
    else if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        engine->appTimerStopped = !engine->appTimerStopped;
    }
    else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        engine->recordingModeOn = !engine->recordingModeOn;
    }
    else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    else if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        std::cout << "\n\nHot keys : \n"
            << "        V ---> activate/deactivate wireframe view\n"
            << "        T ---> start/stop the timer\n"
            << "        R ---> activate/deactivate recording mode\n"
            << "        P ---> activate/deactivate panorama view\n"
            << "        H ---> get help for hot keys\n"
            << "        ESC -> close window\n"
            << std::endl;
    }
}

void VulkanEngine::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    static bool firstMouse = true;
    static float lastX = 0.0f;
    static float lastY = 0.0f;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    auto engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    engine->camera.processMouseMovement(xpos - lastX, lastY - ypos);
    engine->appTimerStopped = true;

    lastX = xpos;
    lastY = ypos;
}

// Vulkan core
void VulkanEngine::initContext() {
    context = VulkanContext();

    context.createInstance();
    context.setupDebugMessenger();
    context.createSurface(window);
    context.pickPhysicalDevice();
    context.createLogicalDevice();
}

void VulkanEngine::initSwapChain() {
    swapChain = VulkanSwapChain(&context);

    swapChain.createSwapChain(window);
    swapChain.createImageViews();
}

void VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChain.imageFormat;
    colorAttachment.samples = context.msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = swapChain.depthFormat;
    depthAttachment.samples = context.msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChain.imageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanEngine::cleanupSwapChain() {

    for (auto& material : materials)
        material.second.pipeline.destroy();

    vkDestroyRenderPass(context.device, renderPass, nullptr);

    swapChain.destroy();
}

void VulkanEngine::recreateSwapChain() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(context.device);
    cleanupSwapChain();

    initSwapChain();
    createRenderPass();
    createFramebuffers();

    for (auto& material : materials)
        material.second.updatePipeline({ globalSetLayout, objectsSetLayout, textureSetLayout }, swapChain.extent, renderPass);

    camera.setPerspectiveProjection(swapChain.extent.width / (float)swapChain.extent.height);
}



// Commands
void VulkanEngine::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = context.findQueueFamilies(context.physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

}

void VulkanEngine::initCommands() {
    commands = std::vector(MAX_FRAMES_IN_FLIGHT, VulkanCommand(&context));

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        commands[i].createCommandBuffer(commandPool);
        commands[i].createSyncStructures();
    }
}

void VulkanEngine::createFramebuffers() {
    swapChain.createFramebuffers(commandPool, renderPass);
}



// Descriptors
void VulkanEngine::createDescriptorPool() {

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) },     // nb of UBO  * MAX_FRAMES_IN_FLIGHT
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) },     // nb of SSBO * MAX_FRAMES_IN_FLIGHT
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_MATERIALS_CREATED) } // MAX_MATERIALS_CREATED
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 10;

    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanEngine::createDescriptorLayouts() {
    // global set layout
    VkDescriptorSetLayoutBinding cameraLayoutBinding{};
    cameraLayoutBinding.binding = 0;
    cameraLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraLayoutBinding.descriptorCount = 1;
    cameraLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding sceneLayoutBinding{};
    sceneLayoutBinding.binding = 1;
    sceneLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneLayoutBinding.descriptorCount = 1;
    sceneLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> globalBindings = { cameraLayoutBinding, sceneLayoutBinding };

    VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
    globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalLayoutInfo.bindingCount = globalBindings.size();
    globalLayoutInfo.pBindings = globalBindings.data();

    if (vkCreateDescriptorSetLayout(context.device, &globalLayoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create global descriptor set layout!");
    }

    // objects set layout
    VkDescriptorSetLayoutBinding objectsLayoutBinding{};
    objectsLayoutBinding.binding = 0;
    objectsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objectsLayoutBinding.descriptorCount = 1;
    objectsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo objectsLayoutInfo{};
    objectsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objectsLayoutInfo.bindingCount = 1;
    objectsLayoutInfo.pBindings = &objectsLayoutBinding;

    if (vkCreateDescriptorSetLayout(context.device, &objectsLayoutInfo, nullptr, &objectsSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create objects descriptor set layout!");
    }

    // texture set layout
    VkDescriptorSetLayoutBinding textureLayoutBinding{};
    textureLayoutBinding.binding = 0;
    textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureLayoutBinding.descriptorCount = 1;
    textureLayoutBinding.pImmutableSamplers = nullptr;
    textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.bindingCount = 1;
    textureLayoutInfo.pBindings = &textureLayoutBinding;

    if (vkCreateDescriptorSetLayout(context.device, &textureLayoutInfo, nullptr, &textureSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void VulkanEngine::initDescriptors() {
    descriptors = std::vector(MAX_FRAMES_IN_FLIGHT, VulkanDescriptor(&context, MAX_OBJECTS_RENDERED));

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        descriptors[i].createBuffers();
        descriptors[i].allocateGlobalDescriptorSet(descriptorPool, globalSetLayout);
        descriptors[i].allocateObjectsDescriptorSet(descriptorPool, objectsSetLayout);
        descriptors[i].updateDescriptors();
    }
}

void VulkanEngine::mapCameraData() {
    CameraData cameraData{};
    cameraData.view = camera.viewMatrix;
    cameraData.proj = camera.projMatrix;
    cameraData.position = camera.camPos;

    void* data;
    vkMapMemory(context.device, descriptors[currentFrame].cameraBuffer.allocation, 0, sizeof(CameraData), 0, &data);
    memcpy(data, &cameraData, sizeof(CameraData));
    vkUnmapMemory(context.device, descriptors[currentFrame].cameraBuffer.allocation);
}

void VulkanEngine::mapSceneData() {
    SceneData sceneData{};
    sceneData.lightPosition = sceneInfo.lightPosition;
    sceneData.lightColor    = sceneInfo.lightColor;;

    void* data;
    vkMapMemory(context.device, descriptors[currentFrame].sceneBuffer.allocation, 0, sizeof(SceneData), 0, &data);
    memcpy(data, &sceneData, sizeof(SceneData));
    vkUnmapMemory(context.device, descriptors[currentFrame].sceneBuffer.allocation);
}

void VulkanEngine::mapObjectsData() {
    void* data;
    vkMapMemory(context.device, descriptors[currentFrame].objectsBuffer.allocation, 0, sizeof(ObjectData), 0, &data);
    ObjectData* objectsData = (ObjectData*)data;

    for (int i = 0; i < renderables.size(); i++) {
        RenderObject& object = renderables[i];
        objectsData[i].model = object.modelMatrix;
        objectsData[i].albedo = object.albedoColor;
    }

    vkUnmapMemory(context.device, descriptors[currentFrame].objectsBuffer.allocation);
}



// Assets
void VulkanEngine::initAssets() {
    loadTextures();
    createMaterial("particle"     , VK_NULL_HANDLE, VulkanPipeline(&context, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    createMaterial("particle_wfm" , VK_NULL_HANDLE, VulkanPipeline(&context, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));
    createMaterial("back_wall"    , VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT));
    createMaterial("support"      , VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    //createMaterial("front_container", VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH , VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));

    loadMeshes();
    getMesh("sphere")->upload(commandPool);
    getMesh("cube")->upload(commandPool);
}

void VulkanEngine::loadTextures() {
    Texture glassTex{ &context };
    glassTex.loadFromFile(commandPool, GLASS_TEXTURE_PATH.c_str());
    textures["glass"] = glassTex;
}

void VulkanEngine::loadMeshes() {
    Mesh sphereMesh{ &context };
    sphereMesh.loadFromObj(SPHERE_MODEL_PATH.c_str());
    meshes["sphere"] = sphereMesh;

    Mesh cube{ &context };
    cube.loadFromObj(CUBE_MODEL_PATH.c_str());
    meshes["cube"] = cube;
    /*
    std::vector<Mesh> faces = std::vector<Mesh>(6, Mesh( &context ));
    for (int i = 0; i < 6; i++)
        faces[i].vertices = cube.vertices;

    for (int i = 0; i < 12; i++)
        for (int j = 0; j < 3; j++) {
            faces[i % 6].indices.push_back(cube.indices[3 * (size_t)i + j]);
        }

    meshes["bottom"] = faces[0];
    meshes["top"] = faces[1];
    meshes["right"] = faces[2];
    meshes["front"] = faces[3];
    meshes["left"] = faces[4];
    meshes["back"] = faces[5];*/
}

void VulkanEngine::createMaterial(const std::string name, Texture* texture, VulkanPipeline pipeline) {    
    Material material{ &context, pipeline, texture };
    material.updatePipeline({ globalSetLayout, objectsSetLayout, textureSetLayout }, swapChain.extent, renderPass);
    material.updateTexture(textureSetLayout, descriptorPool);
    materials[name] = material;
}

void VulkanEngine::switchViewMode() {
    Material* material;

    if (wireframeViewOn)
        material = getMaterial("particle_wfm");
    else
        material = getMaterial("particle");

    for (int i=0; i < renderables.size() - 2; i++)
        renderables[i].material = material;
}

Texture* VulkanEngine::getTexture(const std::string& name)
{
    auto it = textures.find(name);

    if (it == textures.end())
        return nullptr;

    return &(*it).second;
}

Mesh* VulkanEngine::getMesh(const std::string& name)
{
    auto it = meshes.find(name);

    if (it == meshes.end())
        return nullptr;

    return &(*it).second;
}

Material* VulkanEngine::getMaterial(const std::string& name) {
    auto it = materials.find(name);

    if (it == materials.end())
        return nullptr;

    return &(*it).second;
}



// Scene
void VulkanEngine::initScene() {
    // init SPH logic
    sphSolver = IISPHsolver3D();
    Vec3i gridDim  = Vec3i(25, 40, 25);
    Vec3i fluidDim = Vec3i(12, 12, 12);
    sphSolver.init(gridDim, fluidDim);

    float epsilon = 0.21f;
    float sizeX = sphSolver.sizeX() - 2.0f;
    float sizeY = sphSolver.sizeY() - 2.0f;
    float sizeZ = sphSolver.sizeZ() - 2.0f;

    // init scene
    sceneInfo = SceneInfo();
    sceneInfo.lightPosition = glm::vec3(-20.0f, 80.0f, 20.0f);
    sceneInfo.lightColor    = glm::vec3(1.0f);

    // init camera
    float coeff = 1.75f;
    camera = Camera(glm::vec3(sphSolver.sizeX() * coeff, sphSolver.sizeY() / coeff, sphSolver.sizeZ() * coeff), glm::vec3(0.0f, 1.0f, 0.0f), -20.0f, -135.0f);
    camera.updateViewMatrix();
    camera.setPerspectiveProjection(swapChain.extent.width / (float)swapChain.extent.height);

    Vec3f p{}, c{};
    glm::vec3 position{}, color{}, size{}, rotationAxis(0.0f, 1.0f, 0.0f);
    float angle = 0.0f;

    // create fluid particles
    for (int i = 0; i < sphSolver.fluidCount(); i++) {
        p = sphSolver.fluidPosition(i) - 1.0f;
        c = sphSolver.fluidColor(i);
        position   = glm::vec3(p.x, p.y, p.z);
        color = glm::vec3(c.x, c.y, c.z);
        size = glm::vec3(0.2f);

        RenderObject fluidParticle{};
        fluidParticle.mesh = getMesh("sphere");
        fluidParticle.material = getMaterial("particle");
        fluidParticle.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
        fluidParticle.albedoColor = color;

        renderables.push_back(fluidParticle);
    }
    
    std::cout
        << "number of fluid particles    : " << sphSolver.fluidCount() << "\n"
        << "number of boundary particles : " << sphSolver.boundaryCount() << "\n"
        << std::endl;

    // create support
    float height = 20.0f;
    glm::vec3 supportPos  = glm::vec3(sizeX +     epsilon, -height - 2 * epsilon, sizeZ +     epsilon) / 2.0f;
    glm::vec3 supportSize = glm::vec3(sizeX + 2 * epsilon,  height +     epsilon, sizeZ + 2 * epsilon) / 2.0f;

    RenderObject support{};
    support.mesh = getMesh("cube");
    support.material = getMaterial("support");
    support.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), supportPos), angle, rotationAxis), supportSize);
    support.albedoColor = glm::vec3(0.1f);

    renderables.push_back(support);

    // create back wall
    float width = 30.0f;
    glm::vec3 wallPos  = glm::vec3(sizeX +     epsilon + width, sizeY - 2 * epsilon - height, sizeZ +     epsilon + width) / 2.0f;
    glm::vec3 wallSize = glm::vec3(sizeX + 2 * epsilon + width, sizeY +     epsilon + height, sizeZ + 2 * epsilon + width) / 2.0f;

    RenderObject backWall{};
    backWall.mesh = getMesh("cube");
    backWall.material = getMaterial("back_wall");
    backWall.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), wallPos), angle, rotationAxis), wallSize);
    backWall.albedoColor = glm::vec3(0.7f);

    renderables.push_back(backWall);
}

void VulkanEngine::updateScene() {
    currentClockTime = static_cast<float>(glfwGetTime());
    const float dt = currentClockTime - lastClockTime;
    lastClockTime = currentClockTime;

    // update camera
    camera.processKeyboardInput(window, dt);
          
    if (!appTimerStopped) {
        appTimer += dt;

        //compute SPH logic
        sphSolver.update();
        
        Vec3f p{}, c{};
        glm::vec3 position{}, color{}, size{}, rotationAxis(0.0f, 1.0f, 0.0f);
        float angle = 0.0f;

        // update fluid particles
        for (int i = 0; i < sphSolver.fluidCount(); i++) {
            p = sphSolver.fluidPosition(i) - 1.0f;
            c = sphSolver.fluidColor(i);
            position = glm::vec3(p.x, p.y, p.z);
            color = glm::vec3(c.x, c.y, c.z);
            size = glm::vec3(0.2f);

            renderables[i].modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
            renderables[i].albedoColor = color;
        }
    }
}

void VulkanEngine::renderScene(VkCommandBuffer commandBuffer) {
    /*for (int i = 0; i < renderables.size(); i++) {
        drawSingleObject(commandBuffer, i);
    }*/

    drawSingleObject(commandBuffer, renderables.size() - 2); // support
    drawInstanced(commandBuffer, renderables[0], renderables.size() - 2);
    drawSingleObject(commandBuffer, renderables.size() - 1); // back wall

    if(recordingModeOn && !appTimerStopped)
        savesFrames();
}

void VulkanEngine::drawSingleObject(VkCommandBuffer commandBuffer, int i) {
    RenderObject& object = renderables[i];

    // bind shader pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.vkPipeline);

    // bind global uniform resources
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 0, 1, &descriptors[currentFrame].globalDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 1, 1, &descriptors[currentFrame].objectsDescriptorSet, 0, nullptr);

    // bind object resources
    if (object.material->texture != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 2, 1, &object.material->textureDescriptor, 0, nullptr);
    // bind vertices
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, object.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // draw object
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(object.mesh->indices.size()), 1, 0, 0, i);
}

void VulkanEngine::drawInstanced(VkCommandBuffer commandBuffer, RenderObject object, int instanceCount) {
    // bind shader pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.vkPipeline);

    // bind frame descriptors
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 0, 1, &descriptors[currentFrame].globalDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 1, 1, &descriptors[currentFrame].objectsDescriptorSet, 0, nullptr);
    
    // bind texture descriptor
    if(object.material->texture != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline.pipelineLayout, 2, 1, &object.material->textureDescriptor, 0, nullptr);

    // bind vertices
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, object.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // draw object
    uint32_t indexCount = static_cast<uint32_t>(object.mesh->indices.size());
    vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
}



// Exportation
void VulkanEngine::savesFrames() {
    static int saveCount = 0;
    static int maxDigits = 6;

    float val = saveCount;
    int leadingZeros = maxDigits - 1;
    while (val >= 10) {
        val /= 10;
        leadingZeros--;
    }

    std::string frameID = std::to_string(saveCount++);
    for (int i = 0; i < leadingZeros; i++)
        frameID = "0" + frameID;

    std::string filename = "../results/screenshots/frame_" + frameID + ".ppm";
    saveScreenshot(filename.c_str());
}

void VulkanEngine::saveScreenshot(const char* filename) {
    // Take a screenshot from the current swapchain image
    // This is done using a blit from the swapchain image to a linear image whose memory content is then saved as a ppm image
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(context.physicalDevice, swapChain.imageFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(context.physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        supportsBlit = false;
    }

    // Source for the copy is the last rendered swapchain image
    VkImage srcImage = swapChain.images[currentFrame];

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateCI(tools::imageCreateInfo());
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
    imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateCI.extent.width = swapChain.extent.width;
    imageCreateCI.extent.height = swapChain.extent.height;
    imageCreateCI.extent.depth = 1;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    if (vkCreateImage(context.device, &imageCreateCI, nullptr, &dstImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create screenshot image!");
    }
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo(tools::memoryAllocateInfo());
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(context.device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = context.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(context.device, &memAllocInfo, nullptr, &dstImageMemory);
    vkBindImageMemory(context.device, dstImage, dstImageMemory, 0);

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandBuffer copyCmd = context.beginSingleTimeCommands(commandPool);

    // Transition destination image to transfer destination layout
    VkImageMemoryBarrier imageMemoryBarrier0 = tools::imageMemoryBarrier();
    imageMemoryBarrier0.srcAccessMask = 0;
    imageMemoryBarrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier0.image = dstImage;
    imageMemoryBarrier0.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        copyCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier0);

    // Transition swapchain image from present to transfer source layout
    VkImageMemoryBarrier imageMemoryBarrier1 = tools::imageMemoryBarrier();
    imageMemoryBarrier1.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier1.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier1.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageMemoryBarrier1.image = srcImage;
    imageMemoryBarrier1.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        copyCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier1);

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    if (supportsBlit)
    {
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize{};
        blitSize.x = swapChain.extent.width;
        blitSize.y = swapChain.extent.height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(
            copyCmd,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlitRegion,
            VK_FILTER_NEAREST);
    }
    else
    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = swapChain.extent.width;
        imageCopyRegion.extent.height = swapChain.extent.height;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            copyCmd,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
    }

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    VkImageMemoryBarrier imageMemoryBarrier2 = tools::imageMemoryBarrier();
    imageMemoryBarrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier2.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier2.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier2.image = dstImage;
    imageMemoryBarrier2.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        copyCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier2);

    // Transition back the swap chain image after the blit is done
    VkImageMemoryBarrier imageMemoryBarrier3 = tools::imageMemoryBarrier();
    imageMemoryBarrier3.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier3.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageMemoryBarrier3.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier3.image = srcImage;
    imageMemoryBarrier3.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        copyCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier3);

    context.endSingleTimeCommands(commandPool, copyCmd);

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(context.device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* data;
    vkMapMemory(context.device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subResourceLayout.offset;

    std::ofstream file(filename, std::ios::out | std::ios::binary);

    // ppm header
    file << "P6\n" << swapChain.extent.width << "\n" << swapChain.extent.height << "\n" << 255 << "\n";

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    bool colorSwizzle = false;
    // Check if source is BGR
    if (!supportsBlit)
    {
        std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
        colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.imageFormat) != formatsBGR.end());
    }

    // ppm binary pixel data
    for (uint32_t y = 0; y < swapChain.extent.height; y++)
    {
        unsigned int* row = (unsigned int*)data;
        for (uint32_t x = 0; x < swapChain.extent.width; x++)
        {
            if (colorSwizzle)
            {
                file.write((char*)row + 2, 1);
                file.write((char*)row + 1, 1);
                file.write((char*)row, 1);
            }
            else
            {
                file.write((char*)row, 3);
            }
            row++;
        }
        data += subResourceLayout.rowPitch;
    }
    file.close();

    std::cout << "Screenshot saved to disk" << std::endl;

    // Clean up resources
    vkUnmapMemory(context.device, dstImageMemory);
    vkFreeMemory(context.device, dstImageMemory, nullptr);
    vkDestroyImage(context.device, dstImage, nullptr);
}