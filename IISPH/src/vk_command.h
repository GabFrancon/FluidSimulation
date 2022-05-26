#pragma once

#include "vk_device.h"


class VulkanCommand {
public:
    VulkanDevice* device;

    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;


    // initialization
    VulkanCommand(VulkanDevice* device) : device(device) {}
    void createCommandBuffer(VkCommandPool commandPool);
    void createSyncStructures();
    void destroy();
};

