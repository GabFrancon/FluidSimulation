#pragma once

#include "vk_context.h"


class VulkanCommand {
public:
    VulkanContext* context;

    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;


    // initialization
    VulkanCommand() {}
    VulkanCommand(VulkanContext* context) : context(context) {}
    void createCommandBuffer(VkCommandPool commandPool);
    void createSyncStructures();
    void destroy();
};

