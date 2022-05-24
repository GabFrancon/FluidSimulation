#include "vk_engine.h"


/*------------------------------------------MAIN FUNCTIONS---------------------------------------------*/

void VulkanEngine::init() {
    initInterface();
    initVulkan();
    initSwapChain();
    initRenderPass();
    initCommands();
    initFramebuffers();
    initSyncStructures();

    initDescriptors();
    loadAssets();
    initGraphicsPipelines();
    initScene();
}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        updateScene();
        draw();
    }
    vkDeviceWaitIdle(vulkanDevice->device);
}

void VulkanEngine::draw() {
    VkCommandBuffer cmd = commandBuffers[currentFrame];

    vkWaitForFences(vulkanDevice->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanDevice->device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(vulkanDevice->device, 1, &inFlightFences[currentFrame]);
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
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

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

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vulkanDevice->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkSwapchainKHR swapChains[] = { swapChain };

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(vulkanDevice->presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_OVERLAP;
}

void VulkanEngine::cleanup() {
    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        cameraBuffers[i].destroy(vulkanDevice->device);
        objectsBuffers[i].destroy(vulkanDevice->device);
    }

    vkDestroyDescriptorPool(vulkanDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice->device, globalSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice->device, objectsSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice->device, textureSetLayout, nullptr);

    for (auto& tex: textures)
        tex.second.destroy(vulkanDevice->device);

    for (auto& mesh : meshes)
        mesh.second.destroy(vulkanDevice->device);

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        vkDestroySemaphore(vulkanDevice->device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(vulkanDevice->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanDevice->device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(vulkanDevice->device, commandPool, nullptr);

    vulkanDevice->destroy();

    glfwDestroyWindow(window);

    glfwTerminate();
}



/*----------------------------------------EDITABLE FUNCTIONS--------------------------------------------*/

// Interface
void VulkanEngine::initInterface() {
    createWindow();
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyboardCallback);
}

void VulkanEngine::createWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "WCSPH simulation", nullptr, nullptr);
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
        engine->switchWireframeView();
    }
    else if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        engine->appTimerStopped = !engine->appTimerStopped;

        if (!engine->appTimerStopped)
            engine->lastClockTime = static_cast<float>(glfwGetTime());
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
            << "        H ---> get help for hot keys\n"
            << "        ESC -> close window\n"
            << std::endl;
    }
}


// Descriptors
void VulkanEngine::initDescriptors() {
    createDescriptorPool();
    createDescriptorLayouts();
    createUniformBuffers();
    createUniformSets();
}

void VulkanEngine::createDescriptorPool() {

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * static_cast<uint32_t>(MAX_FRAMES_OVERLAP) }, //  nb of UBO * MAX_FRAMES_IN_FLIGHT
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * static_cast<uint32_t>(MAX_FRAMES_OVERLAP) }, //  nb of SSBO * MAX_FRAMES_IN_FLIGHT
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 } // materials.size();
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 10;

    if (vkCreateDescriptorPool(vulkanDevice->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanEngine::createDescriptorLayouts() {
    // global set layout
    VkDescriptorSetLayoutBinding cameraLayoutBinding{};
    cameraLayoutBinding.binding = 0;
    cameraLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraLayoutBinding.descriptorCount = 1;
    cameraLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
    globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalLayoutInfo.bindingCount = 1;
    globalLayoutInfo.pBindings = &cameraLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanDevice->device, &globalLayoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // object set layout
    VkDescriptorSetLayoutBinding objectsLayoutBinding{};
    objectsLayoutBinding.binding = 0;
    objectsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objectsLayoutBinding.descriptorCount = 1;
    objectsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo objectsLayoutInfo{};
    objectsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objectsLayoutInfo.bindingCount = 1;
    objectsLayoutInfo.pBindings = &objectsLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanDevice->device, &objectsLayoutInfo, nullptr, &objectsSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
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

    if (vkCreateDescriptorSetLayout(vulkanDevice->device, &textureLayoutInfo, nullptr, &textureSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}


// Assets
void VulkanEngine::loadAssets() {
    loadTextures();
    createMaterial(*getTexture("water"), "water");
    createMaterial(*getTexture("water"), "wireframe");

    loadMeshes();
    uploadMesh(*getMesh("sphere"));
}

void VulkanEngine::loadTextures() {
    Texture waterTex{};
    waterTex.loadFromFile(vulkanDevice, commandPool, WATER_TEXTURE_PATH.c_str());
    textures["water"] = waterTex;
}

void VulkanEngine::loadMeshes() {
    Mesh sphereMesh;
    sphereMesh.loadFromObj(SPHERE_MODEL_PATH.c_str());
    meshes["sphere"] = sphereMesh;
}

void VulkanEngine::createMaterial(Texture texture, const std::string name) {
    Material material{};
    setMaterialTexture(texture, material);
    materials[name] = material;
}

void VulkanEngine::setMaterialTexture(Texture texture, Material& material) {
    VkDescriptorSetAllocateInfo textureAllocInfo{};
    textureAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    textureAllocInfo.descriptorPool = descriptorPool;
    textureAllocInfo.descriptorSetCount = 1;
    textureAllocInfo.pSetLayouts = &textureSetLayout;

    if (vkAllocateDescriptorSets(vulkanDevice->device, &textureAllocInfo, &material.textureDescriptor) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorImageInfo textureImageInfo{};
    textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textureImageInfo.imageView = texture.albedoMap.imageView;
    textureImageInfo.sampler = texture.sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = material.textureDescriptor;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &textureImageInfo;

    vkUpdateDescriptorSets(vulkanDevice->device, 1, &descriptorWrite, 0, nullptr);
}

void VulkanEngine::uploadMesh(Mesh& mesh) {
    // Vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();

    AllocatedBuffer vertexStagingBuffer{};
    vertexStagingBuffer = vulkanDevice->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* vertexData;
    vkMapMemory(vulkanDevice->device, vertexStagingBuffer.allocation, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, mesh.vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(vulkanDevice->device, vertexStagingBuffer.allocation);

    mesh.vertexBuffer = vulkanDevice->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vulkanDevice->copyBuffer(commandPool, vertexStagingBuffer.buffer, mesh.vertexBuffer.buffer, vertexBufferSize);

    vertexStagingBuffer.destroy(vulkanDevice->device);

    // Index buffer
    VkDeviceSize indexBufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();

    AllocatedBuffer indexStagingBuffer{};
    indexStagingBuffer = vulkanDevice->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* indexData{};
    vkMapMemory(vulkanDevice->device, indexStagingBuffer.allocation, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, mesh.indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(vulkanDevice->device, indexStagingBuffer.allocation);

    mesh.indexBuffer = vulkanDevice->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vulkanDevice->copyBuffer(commandPool, indexStagingBuffer.buffer, mesh.indexBuffer.buffer, indexBufferSize);

    indexStagingBuffer.destroy(vulkanDevice->device);
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


// Uniform values
void VulkanEngine::createUniformBuffers() {

    // create camera buffers
    VkDeviceSize cameraBufferSize = sizeof(CameraData);
    cameraBuffers.resize(MAX_FRAMES_OVERLAP);

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        cameraBuffers[i] = vulkanDevice->createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    // create object buffers
    VkDeviceSize objectBufferSize = sizeof(ObjectData);
    objectsBuffers.resize(MAX_FRAMES_OVERLAP);

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        objectsBuffers[i] = vulkanDevice->createBuffer(objectBufferSize * MAX_OBJECTS_RENDERED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void VulkanEngine::createUniformSets() {
    // global descriptor set
    std::vector<VkDescriptorSetLayout> globalLayouts(MAX_FRAMES_OVERLAP, globalSetLayout);
    VkDescriptorSetAllocateInfo globalAllocInfo{};
    globalAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    globalAllocInfo.descriptorPool = descriptorPool;
    globalAllocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_OVERLAP);
    globalAllocInfo.pSetLayouts = globalLayouts.data();

    globalDescriptors.resize(MAX_FRAMES_OVERLAP);
    if (vkAllocateDescriptorSets(vulkanDevice->device, &globalAllocInfo, globalDescriptors.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> objectsLayouts(MAX_FRAMES_OVERLAP, objectsSetLayout);
    VkDescriptorSetAllocateInfo objectsAllocInfo{};
    objectsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objectsAllocInfo.descriptorPool = descriptorPool;
    objectsAllocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_OVERLAP);
    objectsAllocInfo.pSetLayouts = objectsLayouts.data();

    objectsDescriptors.resize(MAX_FRAMES_OVERLAP);
    if (vkAllocateDescriptorSets(vulkanDevice->device, &objectsAllocInfo, objectsDescriptors.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = cameraBuffers[i].buffer;
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = sizeof(CameraData);

        VkDescriptorBufferInfo objectsBufferInfo{};
        objectsBufferInfo.buffer = objectsBuffers[i].buffer;
        objectsBufferInfo.offset = 0;
        objectsBufferInfo.range = sizeof(ObjectData) * MAX_OBJECTS_RENDERED;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = globalDescriptors[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &cameraBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = objectsDescriptors[i];
        descriptorWrites[1].dstBinding = 0;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &objectsBufferInfo;

        vkUpdateDescriptorSets(vulkanDevice->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanEngine::mapCameraData() {
    CameraData cameraData{};
    cameraData.view = camera.viewMatrix;
    cameraData.proj = camera.projMatrix;

    void* data;
    vkMapMemory(vulkanDevice->device, cameraBuffers[currentFrame].allocation, 0, sizeof(CameraData), 0, &data);
    memcpy(data, &cameraData, sizeof(CameraData));
    vkUnmapMemory(vulkanDevice->device, cameraBuffers[currentFrame].allocation);
}

void VulkanEngine::mapObjectsData() {
    void* data;
    vkMapMemory(vulkanDevice->device, objectsBuffers[currentFrame].allocation, 0, sizeof(ObjectData), 0, &data);
    ObjectData* objectsData = (ObjectData*) data;

    for (int i = 0; i < renderables.size(); i++) {
        RenderObject& object = renderables[i];
        objectsData[i].model  = object.modelMatrix;
        objectsData[i].albedo = object.albedoColor;
    }

    vkUnmapMemory(vulkanDevice->device, objectsBuffers[currentFrame].allocation);
}


// Graphics pipelines
void VulkanEngine::initGraphicsPipelines() {
    Material* waterMaterial = getMaterial("water");
    waterMaterial->pipelineLayout = createPipelineLayout();
    waterMaterial->pipeline = createPipeline(waterMaterial->pipelineLayout, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_FILL);

    Material* wireframeMaterial = getMaterial("wireframe");
    wireframeMaterial->pipelineLayout = createPipelineLayout();
    wireframeMaterial->pipeline = createPipeline(wireframeMaterial->pipelineLayout, INSTANCED_VERT_SHADER_PATH, COLORED_FRAG_SHADER_PATH, VK_POLYGON_MODE_LINE);
}

VkPipelineLayout VulkanEngine::createPipelineLayout() {
    std::array<VkDescriptorSetLayout, 3> layouts = { globalSetLayout, objectsSetLayout, textureSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(vulkanDevice->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    return pipelineLayout;
}

VkPipeline VulkanEngine::createPipeline(VkPipelineLayout pipelineLayout, const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPolygonMode polygonMode) {

    //  build pipeline
    PipelineBuilder builder{};
    builder.pipelineLayout = pipelineLayout;

    // vertex input info
    auto vertexDescription = Vertex::getVertexDescription();
    builder.vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    builder.vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexDescription.bindings.size());
    builder.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributes.size());
    builder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    builder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();

    // input assembly
    builder.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    builder.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    builder.inputAssembly.primitiveRestartEnable = VK_FALSE;

    // viewport state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    builder.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    builder.viewportState.viewportCount = 1;
    builder.viewportState.pViewports = &viewport;
    builder.viewportState.scissorCount = 1;
    builder.viewportState.pScissors = &scissor;

    // rasterizer
    builder.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    builder.rasterizer.depthClampEnable = VK_FALSE;
    builder.rasterizer.rasterizerDiscardEnable = VK_FALSE;
    builder.rasterizer.polygonMode = polygonMode;
    builder.rasterizer.lineWidth = 1.0f;
    builder.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    builder.rasterizer.depthBiasEnable = VK_FALSE;

    // multisampling
    builder.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    builder.multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
    builder.multisampling.rasterizationSamples = vulkanDevice->msaaSamples;
    builder.multisampling.minSampleShading = .2f; // closer to one is smoother

    // color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    builder.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    builder.colorBlending.logicOpEnable = VK_FALSE;
    builder.colorBlending.logicOp = VK_LOGIC_OP_COPY;
    builder.colorBlending.attachmentCount = 1;
    builder.colorBlending.pAttachments = &colorBlendAttachment;
    builder.colorBlending.blendConstants[0] = 0.0f;
    builder.colorBlending.blendConstants[1] = 0.0f;
    builder.colorBlending.blendConstants[2] = 0.0f;
    builder.colorBlending.blendConstants[3] = 0.0f;

    // depth stencil
    builder.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    builder.depthStencil.depthTestEnable = VK_TRUE;
    builder.depthStencil.depthWriteEnable = VK_TRUE;
    builder.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    builder.depthStencil.depthBoundsTestEnable = VK_FALSE;
    builder.depthStencil.stencilTestEnable = VK_FALSE;

    VkPipeline pipeline = builder.buildPipeline(vulkanDevice->device, renderPass, vertexShaderPath, fragmentShaderPath);
    return pipeline;
}


// Scene
void VulkanEngine::initScene() {
    // init logic
    solver = WCSPHSolver();
    solver.init(48, 32, 16, 16);

    // init camera
    camera.viewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    camera.projMatrix = Camera::ortho(0.0f, (float)swapChainExtent.width / 30, 0.0f, (float)swapChainExtent.height / 30, 0.1f, 100.0f);

    // create particles
    for (int i = 0 ; i < solver.particleCount() ; i++) {
        glm::vec3 position = glm::vec3(solver.position(i).x, solver.position(i).y, 0.0f);
        glm::vec3 size = glm::vec3(0.15f);
        glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        float angle = 0.0f;

        RenderObject waterParticle{};
        waterParticle.mesh        = getMesh("sphere");
        waterParticle.material    = getMaterial("water");
        waterParticle.modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
        waterParticle.albedoColor = solver.color(i);

        renderables.push_back(waterParticle);
    }
    std::cout << "number of particles : " << solver.particleCount() << "\n" << std::endl;
}

void VulkanEngine::updateScene() {
    if (!appTimerStopped) {
        currentClockTime = static_cast<float>(glfwGetTime());
        const float dt = currentClockTime - lastClockTime;
        lastClockTime = currentClockTime;
        appTimer += dt;

        //compute SPH logic
        for (int i = 0; i < 10; ++i) solver.update();

        // update particles
        for (int i = 0; i < solver.particleCount(); i++) {
            glm::vec3 position = glm::vec3(solver.position(i).x, solver.position(i).y, 0.0f);
            glm::vec3 size = glm::vec3(0.15f);
            glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
            float angle = 0.0f;

            renderables[i].modelMatrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), angle, rotationAxis), size);
            renderables[i].albedoColor = solver.color(i);
        }
    }

    // update camera
    camera.viewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    camera.projMatrix = Camera::ortho(0.0f, (float)swapChainExtent.width / 30, 0.0f, (float)swapChainExtent.height / 30, 0.1f, 100.0f);
    //camera.projMatrix = Camera::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
}

void VulkanEngine::renderScene(VkCommandBuffer commandBuffer) {
    mapCameraData();
    mapObjectsData();

    //drawObjects(commandBuffer, renderables.data(), renderables.size());
    drawInstanced(commandBuffer, renderables[0], renderables.size());

    if(recordingModeOn && !appTimerStopped)
        savesFrames();
}

void VulkanEngine::drawObjects(VkCommandBuffer commandBuffer, RenderObject* firstObject, int objectsCount) {

    for (int i = 0; i < objectsCount; i++) {
        RenderObject& object = firstObject[i];

        // bind shader pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);

        // bind global uniform resources
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &globalDescriptors[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &objectsDescriptors[currentFrame], 0, nullptr);
        // bind object resources
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureDescriptor, 0, nullptr);

        // bind vertices
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, object.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        // draw object
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(object.mesh->indices.size()), 1, 0, 0, i);
    }
}

void VulkanEngine::drawInstanced(VkCommandBuffer commandBuffer, RenderObject object, int instanceCount) {
    // bind shader pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);

    // bind global uniform resources
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &globalDescriptors[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &objectsDescriptors[currentFrame], 0, nullptr);
    // bind object resources
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureDescriptor, 0, nullptr);

    // bind vertices
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, object.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // draw object
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(object.mesh->indices.size()), instanceCount, 0, 0, 0);
}

void VulkanEngine::switchWireframeView() {
    Material* material;

    if (wireframeViewOn)
        material = getMaterial("wireframe");
    else
        material = getMaterial("water");

    for (RenderObject& object : renderables)
        object.material = material;
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
    // Getting the image date directly from a swapchain image wouldn't work as they're usually stored in an implementation dependent optimal tiling format
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, swapChainImageFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        supportsBlit = false;
    }

    // Source for the copy is the last rendered swapchain image
    VkImage srcImage = swapChainImages[currentFrame];

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateCI(tools::imageCreateInfo());
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
    imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateCI.extent.width = swapChainExtent.width;
    imageCreateCI.extent.height = swapChainExtent.height;
    imageCreateCI.extent.depth = 1;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    if (vkCreateImage(vulkanDevice->device, &imageCreateCI, nullptr, &dstImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create screenshot image!");
    }
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo(tools::memoryAllocateInfo());
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(vulkanDevice->device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = vulkanDevice->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(vulkanDevice->device, &memAllocInfo, nullptr, &dstImageMemory);
    vkBindImageMemory(vulkanDevice->device, dstImage, dstImageMemory, 0);

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandBuffer copyCmd = vulkanDevice->beginSingleTimeCommands(commandPool);

    // Transition destination image to transfer destination layout
    VkImageMemoryBarrier imageMemoryBarrier = tools::imageMemoryBarrier();
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.image = dstImage;
    imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        copyCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

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
        VkOffset3D blitSize;
        blitSize.x = swapChainExtent.width;
        blitSize.y = swapChainExtent.height;
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
        imageCopyRegion.extent.width = swapChainExtent.width;
        imageCopyRegion.extent.height = swapChainExtent.height;
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

    vulkanDevice->endSingleTimeCommands(commandPool, copyCmd);

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(vulkanDevice->device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* data;
    vkMapMemory(vulkanDevice->device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subResourceLayout.offset;

    std::ofstream file(filename, std::ios::out | std::ios::binary);

    // ppm header
    file << "P6\n" << swapChainExtent.width << "\n" << swapChainExtent.height << "\n" << 255 << "\n";

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    bool colorSwizzle = false;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
    if (!supportsBlit)
    {
        std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
        colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChainImageFormat) != formatsBGR.end());
    }

    // ppm binary pixel data
    for (uint32_t y = 0; y < swapChainExtent.height; y++)
    {
        unsigned int* row = (unsigned int*)data;
        for (uint32_t x = 0; x < swapChainExtent.width; x++)
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
    vkUnmapMemory(vulkanDevice->device, dstImageMemory);
    vkFreeMemory(vulkanDevice->device, dstImageMemory, nullptr);
    vkDestroyImage(vulkanDevice->device, dstImage, nullptr);
}




/*------------------------------------------CORE FUNCTIONS---------------------------------------------*/

// Vulkan
void VulkanEngine::initVulkan() {
    vulkanDevice = new VulkanDevice();

    vulkanDevice->createInstance();
    vulkanDevice->setupDebugMessenger();
    vulkanDevice->createSurface(window);
    vulkanDevice->pickPhysicalDevice();
    vulkanDevice->createLogicalDevice();
}

// Swap chain
void VulkanEngine::initSwapChain() {
    createSwapChain();
    createImageViews();
}
void VulkanEngine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = vulkanDevice->querySwapChainSupport(vulkanDevice->physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vulkanDevice->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = vulkanDevice->findQueueFamilies(vulkanDevice->physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(vulkanDevice->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(vulkanDevice->device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vulkanDevice->device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}
void VulkanEngine::cleanupSwapChain() {
    depthImage.destroy(vulkanDevice->device);
    colorImage.destroy(vulkanDevice->device);

    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(vulkanDevice->device, swapChainFramebuffers[i], nullptr);
    }

    for (auto& material : materials) {
        vkDestroyPipeline(vulkanDevice->device, material.second.pipeline, nullptr);
        vkDestroyPipelineLayout(vulkanDevice->device, material.second.pipelineLayout, nullptr);
    }

    vkDestroyRenderPass(vulkanDevice->device, renderPass, nullptr);

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(vulkanDevice->device, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(vulkanDevice->device, swapChain, nullptr);
}
void VulkanEngine::recreateSwapChain() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(vulkanDevice->device);
    cleanupSwapChain();
    initSwapChain();
    initRenderPass();
    initFramebuffers();
    initGraphicsPipelines();
}
void VulkanEngine::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = vulkanDevice->createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}
VkSurfaceFormatKHR VulkanEngine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}
VkPresentModeKHR VulkanEngine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}
VkExtent2D VulkanEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// Render pass
void VulkanEngine::initRenderPass() {
    createDefaultRenderPass();
}
void VulkanEngine::createDefaultRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = vulkanDevice->msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = vulkanDevice->msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
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

    if (vkCreateRenderPass(vulkanDevice->device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

// Commands
void VulkanEngine::initCommands() {
    createCommandPool();
    createCommandBuffers();
}
void VulkanEngine::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = vulkanDevice->findQueueFamilies(vulkanDevice->physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(vulkanDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}
void VulkanEngine::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_OVERLAP);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(vulkanDevice->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

// Framebuffers
void VulkanEngine::initFramebuffers() {
    createColorResources();
    createDepthResources();
    createFramebuffers();
}
void VulkanEngine::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImage.imageView,
            depthImage.imageView,
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice->device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
void VulkanEngine::createColorResources() {
    VkFormat colorFormat = swapChainImageFormat;

    colorImage.allocatedImage = vulkanDevice->createImage(swapChainExtent.width, swapChainExtent.height, 1, vulkanDevice->msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    colorImage.imageView = vulkanDevice->createImageView(colorImage.allocatedImage.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}
void VulkanEngine::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    depthImage.allocatedImage = vulkanDevice->createImage(swapChainExtent.width, swapChainExtent.height, 1, vulkanDevice->msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImage.imageView = vulkanDevice->createImageView(depthImage.allocatedImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    vulkanDevice->transitionImageLayout(commandPool, depthImage.allocatedImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}
VkFormat VulkanEngine::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}
VkFormat VulkanEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

// Sync structures
void VulkanEngine::initSyncStructures() {
    createSyncObjects();
}
void VulkanEngine::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_OVERLAP);
    renderFinishedSemaphores.resize(MAX_FRAMES_OVERLAP);
    inFlightFences.resize(MAX_FRAMES_OVERLAP);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_OVERLAP; i++) {
        if (vkCreateSemaphore(vulkanDevice->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanDevice->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanDevice->device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}