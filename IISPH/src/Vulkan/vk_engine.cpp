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
    showStatistics();
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
    //glfwSetCursorPosCallback(window, mouseCallback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

        if (engine->appTimerStopped)
            std::cout << "app timer stopped" << std::endl;
        else
            std::cout << "app timer started" << std::endl;
    }
    else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        engine->recordAnim = !engine->recordAnim;

        if(engine->recordAnim)
            std::cout << "record animation on" << std::endl;
        else
            std::cout << "record animation off" << std::endl;
    }
    else if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        engine->exportAnim = !engine->exportAnim;

        if (engine->exportAnim)
            std::cout << "export animation on" << std::endl;
        else
            std::cout << "export animation off" << std::endl;
    }
    else if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        engine->particleViewOn = !engine->particleViewOn;

        if (engine->particleViewOn)
            engine->updateParticles();
        else
            engine->updateSurface();
    }
    else if (key == GLFW_KEY_K && action == GLFW_PRESS) {
        engine->showStatistics();
    }
    else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    else if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        std::cout << "\n\nHot keys : \n"
            << "        T ---> start/stop animation timer\n"
            << "        R ---> on/off animation recording\n"
            << "        O ---> on/off animation exportation\n"
            << "        V ---> on/off wireframe view\n"
            << "        P ---> on/off particle view\n"
            << "        K ---> show statistics\n"
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

    if (!engine->appTimerStopped) {
        engine->appTimerStopped = true;
        std::cout << "app timer stopped" << std::endl;
    }

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
    createMaterial("inst_col_fill_back", VK_NULL_HANDLE, VulkanPipeline(&context, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    createMaterial("inst_col_line_back", VK_NULL_HANDLE, VulkanPipeline(&context, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_col_fill_back" , VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_col_line_back" , VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_col_fill_front", VK_NULL_HANDLE, VulkanPipeline(&context, BASIC_VERT_SHADER_PATH    , COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT));

    createMaterial("bas_water_fill_back", getTexture("water"), VulkanPipeline(&context, BASIC_VERT_SHADER_PATH, TEXTURED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_water_line_back", getTexture("water"), VulkanPipeline(&context, BASIC_VERT_SHADER_PATH, TEXTURED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_bunny_fill_back", getTexture("bunny"), VulkanPipeline(&context, BASIC_VERT_SHADER_PATH, TEXTURED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT));
    createMaterial("bas_bunny_line_back", getTexture("bunny"), VulkanPipeline(&context, BASIC_VERT_SHADER_PATH, TEXTURED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT));

    loadMeshes();
    getMesh("sphere")->upload(commandPool);
    getMesh("cube")->upload(commandPool);
    getMesh("bunny")->upload(commandPool);
}

void VulkanEngine::loadTextures() {
    Texture waterTex{ &context };
    waterTex.loadFromFile(commandPool, WATER_TEXTURE_PATH.c_str());
    textures["water"] = waterTex;

    Texture bunnyTex{ &context };
    bunnyTex.loadFromFile(commandPool, BUNNY_TEXTURE_PATH.c_str());
    textures["bunny"] = bunnyTex;
}

void VulkanEngine::loadMeshes() {
    Mesh sphere{ &context };
    sphere.loadFromObj(SPHERE_MODEL_PATH.c_str(), false, false);
    meshes["sphere"] = sphere;

    Mesh cube{ &context };
    cube.loadFromObj(CUBE_MODEL_PATH.c_str(), false, false);
    meshes["cube"] = cube;

    Mesh bunny{ &context };
    bunny.loadFromObj(BUNNY_MODEL_PATH.c_str(), false, true);
    meshes["bunny"] = bunny;

    Mesh geodesic{ &context };
    geodesic.genSphere(0);
    meshes["geodesic0"] = geodesic;

    geodesic.sphereSubdivision();
    meshes["geodesic1"] = geodesic;

    geodesic.sphereSubdivision();
    meshes["geodesic2"] = geodesic;

    geodesic.sphereSubdivision();
    geodesic.genSphere(1);
    meshes["geodesic3"] = geodesic;
}

void VulkanEngine::createMaterial(const std::string name, Texture* texture, VulkanPipeline pipeline) {    
    Material material{ &context, pipeline, texture };
    material.updatePipeline({ globalSetLayout, objectsSetLayout, textureSetLayout }, swapChain.extent, renderPass);
    material.updateTexture(textureSetLayout, descriptorPool);
    materials[name] = material;
}

void VulkanEngine::switchViewMode() {

    if (wireframeViewOn) {
        renderables[0].material = getMaterial("bas_bunny_line_back"); // bunny or glass
        renderables[renderables.size() - 3].material = getMaterial("bas_col_line_back"); // surface
    }
    else {
        renderables[0].material = getMaterial("bas_bunny_fill_back"); // bunny or glass
        renderables[renderables.size() - 3].material = getMaterial("bas_col_fill_back"); // surface
    }
}

void VulkanEngine::generateSurfaceMesh() {
    Mesh surface{ &context };
    sphSolver.reconstructSurface();

    const POINT3D* vertices     = sphSolver.vertices();
    const unsigned int* indices = sphSolver.indices();

    for (int i = 0; i < sphSolver.verticesCount(); i++) {
        Vertex vertex{};
        vertex.position = { vertices[i][0], vertices[i][1], vertices[i][2] };
        surface.vertices.push_back(vertex);
    }
    surface.indices.assign(indices, indices + sphSolver.indicesCount());

    surface.laplacianSmooth(3); // post-processing
    meshes["surface"] = surface;
}

void VulkanEngine::loadSurfaceMesh() {
    Mesh surface{ &context };
    std::string filename = "../results/meshes_break/surface_" + frameID(frameCount) + ".obj";
    surface.loadFromObj(filename.c_str(), true, true);

    meshes["surface"] = surface;
}

Texture* VulkanEngine::getTexture(const std::string& name)
{
    auto it = textures.find(name);

    if (it == textures.end())
        return nullptr;

    return &(*it).second;
}

Mesh* VulkanEngine::getMesh(const std::string& name) {
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
    // init scene parameters
    sceneInfo = SceneInfo();
    sceneInfo.lightPosition = glm::vec3(40.0f, 40.0f, 40.0f);
    sceneInfo.lightColor    = glm::vec3(1.0f);

    // init camera
    camera = Camera(glm::vec3(35, 22, 28), glm::vec3(0.0f, 1.0f, 0.0f), -25.0f, -135.0f); 
    camera.updateViewMatrix();
    camera.setPerspectiveProjection(swapChain.extent.width / (float)swapChain.extent.height);

    // init SPH solver
    initSphSolver();

    // init render objects
    initParticles();
    initSurface();
    initRoom();
}

void VulkanEngine::initSphSolver() {
    Real spacing = 1.0f / 4;
    sphSolver = IISPHsolver3D(spacing);

    Real  pCellSize = 2 * spacing;
    Real  sCellSize = spacing / 2;
    Vec3f gridSize(25.0f, 30.0f, 14.0f);

    sphSolver.setParticleHelper(pCellSize, gridSize);
    sphSolver.setSurfaceHelper(sCellSize, gridSize);

    // sample fluid mass

    /*---------------------------------------"drop in a basin" scenario------------------------------------------------*/

    /*Vec3f fluidSize(gridSize.x - 2 * pCellSize, 5.0f, gridSize.z - 2 * pCellSize); // filled basin
    sphSolver.sampleFluidCube(Vec3f(pCellSize), fluidSize + pCellSize);

    // obstacle
    glm::vec3 position(gridSize.x / 2, 18.0f, gridSize.z / 2), color(0.8f, 0.7f, 0.2f), size(2.0f), rotationAxis(0.0f, 1.0f, 0.0f), p{};
    float angle(0.0f);

    RenderObject waterDrop{};
    waterDrop.mesh = getMesh("geodesic3");
    waterDrop.material = getMaterial("bas_bunny_fill_back");
    waterDrop.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
    waterDrop.albedoColor = color;
    renderables.push_back(waterDrop);

    std::vector<Vec3f> vertices = std::vector<Vec3f>();
    std::vector<Index> indices = std::vector<Index>();

    for (int i : getMesh("geodesic3")->indices)
        indices.push_back(i);

    glm::mat4 modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic3")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic2")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic2")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic2")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic2")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic1")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);

    size -= spacing;
    vertices.clear();
    modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);
    for (const Vertex& v : getMesh("geodesic0")->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    sphSolver.sampleFluidMesh(vertices, indices);*/


    /*-----------------------------------------------------------------------------------------------------------------*/



    /*-----------------------------------"breaking dam on a bunny" scenario--------------------------------------------*/

    Vec3f fluidSize(5.0f, 12.0f, gridSize.z - 2 * pCellSize);
    sphSolver.sampleFluidCube(Vec3f(pCellSize), fluidSize + pCellSize);

    glm::vec3 position(gridSize.x / 2 + 3.0f, 2.5f, gridSize.z / 2 + 1.0f), color(0.8f, 0.7f, 0.2f), size(5.0f), rotationAxis(0.0f, 1.0f, 0.0f), p{};
    float angle(0.0f);

    RenderObject bunny{};
    bunny.mesh = getMesh("bunny");
    bunny.material = getMaterial("bas_col_fill_back");
    bunny.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
    bunny.albedoColor = color;
    renderables.push_back(bunny);

    std::vector<Vec3f> vertices = std::vector<Vec3f>();
    std::vector<Index> indices = std::vector<Index>();
    glm::mat4 modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);

    for (const Vertex& v : bunny.mesh->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    for (int i : bunny.mesh->indices)
        indices.push_back(i);

    sphSolver.sampleBoundaryMesh(vertices, indices);
    
    /*-----------------------------------------------------------------------------------------------------------------*/

    /*----------------------------------"water flow in a container" scenario-------------------------------------------*/
    /*Vec3f fluidSize(1.0f, 15.0f, 1.0f);
    Vec3f offset = Vec3f(gridSize.x / 2, 12.0f, gridSize.z/ 2);
    sphSolver.sampleFluidCube(offset, fluidSize + offset);

    glm::vec3 position(gridSize.x / 2, 2.0f, gridSize.z / 2), color(0.8f, 0.7f, 0.2f), size(4.0f), rotationAxis(0.0f, 1.0f, 0.0f), p{};
    float angle(0.0f);

    RenderObject bunny{};
    bunny.mesh = getMesh("bunny");
    bunny.material = getMaterial("bas_bunny_fill_back");
    bunny.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
    bunny.albedoColor = color;
    renderables.push_back(bunny);

    std::vector<Vec3f> vertices = std::vector<Vec3f>();
    std::vector<Index> indices = std::vector<Index>();
    glm::mat4 modelMat = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position + pCellSize), angle, rotationAxis), size);

    for (const Vertex& v : glass.mesh->vertices) {
        p = glm::vec3(modelMat * glm::vec4(v.position, 1.0f));
        vertices.push_back(Vec3f(p.x, p.y, p.z));
    }
    for (int i : glass.mesh->indices)
        indices.push_back(i);

    sphSolver.sampleBoundaryMesh(vertices, indices);*/
    /*-----------------------------------------------------------------------------------------------------------------*/


    // sample boundaries
    sphSolver.sampleBoundaryBox(Vec3f(0.0f), gridSize);

    // sample distance field
    sphSolver.sampleDistanceField(Vec3f(0.0f), gridSize);

    sphSolver.prepare();
}

void VulkanEngine::initParticles() {
    Vec3f p{}, c{};
    glm::vec3 position{}, color{}, size(sphSolver.particleSpacing() / 3.0f), rotationAxis(0.0f, 1.0f, 0.0f);
    float angle(0.0f);

    // create fluid particles
    for (int i = 0; i < sphSolver.fluidCount(); i++) {
        p = sphSolver.fluidPosition(i) - sphSolver.cellSize();
        c = sphSolver.fluidColor(i);

        position = glm::vec3(p.x, p.y, p.z);
        color = glm::vec3(c.x, c.y, c.z);

        RenderObject fluidParticle{};
        fluidParticle.mesh = getMesh("sphere");
        fluidParticle.material = getMaterial("inst_col_fill_back");
        fluidParticle.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
        fluidParticle.albedoColor = color;

        renderables.push_back(fluidParticle);
    }

    // create boundary particles
    /*for (int i = 0; i < sphSolver.boundaryCount(); i++) {
        p = sphSolver.boundaryPosition(i) - sphSolver.cellSize();
        c = sphSolver.boundaryColor(i);

        position = glm::vec3(p.x, p.y, p.z);
        color = glm::vec3(c.x, c.y, c.z);

        RenderObject boundaryParticle{};
        boundaryParticle.mesh = getMesh("sphere");
        boundaryParticle.material = getMaterial("inst_col_fill_back");
        boundaryParticle.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
        boundaryParticle.albedoColor = color;

        renderables.push_back(boundaryParticle);
    }*/
}

void VulkanEngine::initSurface() {
    RenderObject surface{};

    float angle(0.0f);
    glm::vec3 position(-sphSolver.cellSize()), color(0.06f, 0.24f, 0.7f), size(1.0f), rotationAxis(0.0f, 1.0f, 0.0f);

    if (simulationOn)
        generateSurfaceMesh();
    else
        loadSurfaceMesh();

    getMesh("surface")->upload(commandPool);

    surface.mesh = getMesh("surface");
    surface.material = getMaterial("bas_col_fill_back");
    surface.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
    surface.albedoColor = color;

    renderables.push_back(surface);
}

void VulkanEngine::initRoom() {
    float sizeX = sphSolver.sizeX() - 2 * sphSolver.cellSize();
    float sizeY = sphSolver.sizeY() - 2 * sphSolver.cellSize();
    float sizeZ = sphSolver.sizeZ() - 2 * sphSolver.cellSize();

    glm::vec3 rotationAxis(0.0f, 1.0f, 0.0f);
    float angle = 0.0f;

    // create support
    float height = 20.0f;
    glm::vec3 supportPos  = glm::vec3(sizeX,-height, sizeZ) / 2.0f;
    glm::vec3 supportSize = glm::vec3(sizeX, height, sizeZ) / 2.0f;

    RenderObject support{};
    support.mesh = getMesh("cube");
    support.material = getMaterial("bas_col_fill_back");
    support.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), supportPos), angle, rotationAxis), supportSize);
    support.albedoColor = glm::vec3(0.1f);

    renderables.push_back(support);

    // create back wall
    float width = 30.0f;
    glm::vec3 wallPos  = glm::vec3(sizeX + width, sizeY - height, sizeZ + width) / 2.0f;
    glm::vec3 wallSize = glm::vec3(sizeX + width, sizeY + height, sizeZ + width) / 2.0f;

    RenderObject backWall{};
    backWall.mesh = getMesh("cube");
    backWall.material = getMaterial("bas_col_fill_front");
    backWall.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), wallPos), angle, rotationAxis), wallSize);
    backWall.albedoColor = glm::vec3(0.7f);

    renderables.push_back(backWall);
}

void VulkanEngine::updateScene() {
    currentClockTime = static_cast<float>(glfwGetTime());
    const float dt = currentClockTime - lastClockTime;
    lastClockTime = currentClockTime;

    // update camera
    //camera.processKeyboardInput(window, dt);

    if (!appTimerStopped) {
        appTimer += dt;

        if (simulationOn) {
            // solve SPH equations
            updateSphSolver();
           
            // update render objects
            if(particleViewOn)
                updateParticles();
            else
                updateSurface();

            if (frameCount >= 1500)
                glfwSetWindowShouldClose(window, true);
        }
        else {
            if (frameCount >= 1500)
                frameCount = 1;

            // upload next surface mesh from .obj file
            updateSurface();
        }
        frameCount++;
    }
}

void VulkanEngine::updateSphSolver() {
    auto start = Clock::now();

    for (int i = 0; i < 2; i++)
        sphSolver.solveSimulation();

    std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    sphTimeComputation = (elapsed.count() + (frameCount - 1) * sphTimeComputation) / frameCount;
}

void VulkanEngine::updateParticles() {
    Vec3f p{}, c{};
    glm::vec3 position{}, color{}, size(sphSolver.particleSpacing() / 3.0f), rotationAxis(0.0f, 1.0f, 0.0f);
    float angle(0.0f);

    for (int i = 0; i < sphSolver.fluidCount(); i++) {
        p = sphSolver.fluidPosition(i) - sphSolver.cellSize();
        c = sphSolver.fluidColor(i);

        position = glm::vec3(p.x, p.y, p.z);
        color = glm::vec3(c.x, c.y, c.z);

        renderables[i + 1].modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
        renderables[i + 1].albedoColor = color;
    }
}

void VulkanEngine::updateSurface() {
    auto start = Clock::now();

    vkDeviceWaitIdle(context.device);
    getMesh("surface")->destroy();

    if (simulationOn)
        generateSurfaceMesh();
    else
        loadSurfaceMesh();

    getMesh("surface")->upload(commandPool);
    renderables[renderables.size() - 3].mesh = getMesh("surface");

    std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    surfaceTimeComputation = (elapsed.count() + (frameCount - 1) * surfaceTimeComputation) / frameCount;
}

void VulkanEngine::renderScene(VkCommandBuffer commandBuffer) {

    drawSingleObject(commandBuffer, 0); // bunny or glass

    drawSingleObject(commandBuffer, renderables.size() - 2); // support

    if (particleViewOn) {
        drawInstanced(commandBuffer, sphSolver.fluidCount(), 1); // fluid particles
        //drawInstanced(commandBuffer, sphSolver.boundaryCount(), 1 + sphSolver.fluidCount()); // boundary particles
    }
    else
        drawSingleObject(commandBuffer, renderables.size() - 3); // surface

    drawSingleObject(commandBuffer, renderables.size() - 1); // back wall

    if(recordAnim && !appTimerStopped)
        saveFrame();

    if (exportAnim && !appTimerStopped)
        saveSurfaceMesh();
}

void VulkanEngine::drawSingleObject(VkCommandBuffer commandBuffer, int objectIndex) {
    RenderObject& object = renderables[objectIndex];

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
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(object.mesh->indices.size()), 1, 0, 0, objectIndex);
}

void VulkanEngine::drawInstanced(VkCommandBuffer commandBuffer, int instanceCount, int firstInstance) {
    RenderObject& object = renderables[firstInstance];

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
    vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, firstInstance);
}



// Exportation
void VulkanEngine::saveSurfaceMesh() {
    std::string filename = "../results/meshes/surface_" + frameID(frameCount) + ".obj";
    getMesh("surface")->saveToObj(filename.c_str());
}

void VulkanEngine::saveFrame() {
    std::string filename = "../results/screenshots/frame_" + frameID(frameCount) + ".ppm";
    swapChain.takeScreenshot(filename.c_str(), commandPool, currentFrame);
}

std::string VulkanEngine::frameID(int frameCount) {
    static int maxDigits = 6;

    float val = frameCount;
    int leadingZeros = maxDigits - 1;
    while (val >= 10) {
        val /= 10;
        leadingZeros--;
    }

    std::string frameID = std::to_string(frameCount);
    for (int i = 0; i < leadingZeros; i++)
        frameID = "0" + frameID;

    return frameID;
}

void VulkanEngine::showStatistics() {
    std::cout << "\n"
        << "time for SPH simulation : " << std::setw(6) << sphTimeComputation << " ms\n" 
        << "time for surface reconstruction : " << std::setw(6) << surfaceTimeComputation << " ms\n"
        << std::endl;
}
