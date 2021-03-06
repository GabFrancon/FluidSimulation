#include "vk_command.h"


// intialization

void VulkanCommand::createCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffer!");
    }
}

void VulkanCommand::createSyncStructures() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(context->device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {

        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }
}

void VulkanCommand::destroy() {
    vkDestroySemaphore(context->device, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(context->device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(context->device, inFlightFence, nullptr);
}