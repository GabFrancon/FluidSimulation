#pragma once

#include "vk_context.h"


class VulkanSwapChain
{
public:
    VulkanContext* context;

    VkSwapchainKHR vkSwapChain;                         // Vulkan rendered images handler
    std::vector<VkImage> images;                       // images from the swapchain
    std::vector<VkImageView> imageViews;               // image-views from the swapchain
    std::vector<VkFramebuffer> framebuffers;           // memory buffers in which the images are stored
    ImageMap colorImage;                               // image map holding color infos
    ImageMap depthImage;                               // image map holding depth infos
    VkFormat imageFormat;                              // image format expected by the window and the render pass
    VkFormat depthFormat;                              // depth format expected by the render pass
    VkExtent2D extent;                                 // dimensions of the swapchain



    // initialization
    VulkanSwapChain(VulkanContext* context) : context(context) {}
    void createSwapChain(GLFWwindow* window);
    void createImageViews();
    void createFramebuffers(VkCommandPool commandPool, VkRenderPass renderPass);
    void destroy();

private:
    // helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities);
    void createColorResources();
    void createDepthResources(VkCommandPool commandPool);
    VkFormat findDepthFormat();
};

