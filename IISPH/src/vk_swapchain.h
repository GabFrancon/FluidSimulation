#pragma once

#include "vk_device.h"


class VulkanSwapChain
{
public:
    VulkanDevice* vulkanDevice;

    VkSwapchainKHR swapChain;                          // Vulkan rendered images handle
    std::vector<VkImage> images;                       // array of images from the swapchain
    std::vector<VkImageView> imageViews;               // array of image-views from the swapchain
    VkFormat imageFormat;                              // image format expected by the windowing system
    VkExtent2D extent;                                 // dimensions of the swapchain



    VulkanSwapChain(VulkanDevice* device) : vulkanDevice(device) {}
    void createSwapChain(GLFWwindow* window);
    void createImageViews();
    void destroy();

private:
    // helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities);
};

