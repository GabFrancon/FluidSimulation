#pragma once

#include "vk_device.h"
#include "vk_swapchain.h"


class VulkanDrawHandler
{
public:
    VulkanDevice* vulkanDevice;

    // Commands
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Frambuffers
    std::vector<VkFramebuffer> swapChainFramebuffers;
    ImageMap colorImage;
    ImageMap depthImage;
    const int maxFramesOverlap = 2;

    // Sync structures
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;



    /*---------------------------------------FUNCTIONS----------------------------------------------------*/

    VulkanDrawHandler(VulkanDevice* device) : vulkanDevice(device) {}

    // Commands
    void createCommandPool();
    void createCommandBuffers();
    void destroyCommands();

    // Frambuffers
    void createFramebuffers(VulkanSwapChain* swapChain, VkRenderPass renderPass);
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
    void destroyFramebuffers();

    // Sync structures
    void createSyncObjects();
    void destroySyncObjects();

private:
    void createColorResources(VulkanSwapChain* swapChain);
    void createDepthResources(VulkanSwapChain* swapChain, VkCommandPool commandPool);
    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

};

