#include "vk_drawhandler.h"



// Commands

void VulkanDrawHandler::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = vulkanDevice->findQueueFamilies(vulkanDevice->physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(vulkanDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void VulkanDrawHandler::createCommandBuffers() {
    commandBuffers.resize(maxFramesOverlap);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(vulkanDevice->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanDrawHandler::destroyCommands() {
    vkDestroyCommandPool(vulkanDevice->device, commandPool, nullptr);
}


// Framebuffers

void VulkanDrawHandler::createFramebuffers(VulkanSwapChain* swapChain, VkRenderPass renderPass) {

    swapChainFramebuffers.resize(swapChain->imageViews.size());

    createColorResources(swapChain);
    createDepthResources(swapChain, commandPool);

    for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImage.imageView,
            depthImage.imageView,
            swapChain->imageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChain->extent.width;
        framebufferInfo.height = swapChain->extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice->device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanDrawHandler::createColorResources(VulkanSwapChain* swapChain) {
    VkFormat colorFormat = swapChain->imageFormat;

    colorImage.allocatedImage = vulkanDevice->createImage(swapChain->extent.width, swapChain->extent.height, 1, vulkanDevice->msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    colorImage.imageView = vulkanDevice->createImageView(colorImage.allocatedImage.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void VulkanDrawHandler::createDepthResources(VulkanSwapChain* swapChain, VkCommandPool commandPool) {
    VkFormat depthFormat = findDepthFormat(vulkanDevice->physicalDevice);

    depthImage.allocatedImage = vulkanDevice->createImage(swapChain->extent.width, swapChain->extent.height, 1, vulkanDevice->msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImage.imageView = vulkanDevice->createImageView(depthImage.allocatedImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    vulkanDevice->transitionImageLayout(commandPool, depthImage.allocatedImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

VkFormat VulkanDrawHandler::findDepthFormat(VkPhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanDrawHandler::findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

void VulkanDrawHandler::destroyFramebuffers() {
    depthImage.destroy(vulkanDevice->device);
    colorImage.destroy(vulkanDevice->device);

    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(vulkanDevice->device, swapChainFramebuffers[i], nullptr);
    }
}


// Sync structures

void VulkanDrawHandler::createSyncObjects() {
    imageAvailableSemaphores.resize(maxFramesOverlap);
    renderFinishedSemaphores.resize(maxFramesOverlap);
    inFlightFences.resize(maxFramesOverlap);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesOverlap; i++) {
        if (vkCreateSemaphore(vulkanDevice->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanDevice->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanDevice->device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanDrawHandler::destroySyncObjects() {
    for (size_t i = 0; i < maxFramesOverlap; i++) {
        vkDestroySemaphore(vulkanDevice->device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(vulkanDevice->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanDevice->device, inFlightFences[i], nullptr);
    }
}