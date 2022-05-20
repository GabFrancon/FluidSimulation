#pragma once

#include <vulkan/vulkan.h>


// std
#include <stdexcept>
#include <vector>
#include <array>
#include <fstream>
#include <optional>
#include <cmath>


// general
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue graphicsQueue);
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);


// buffer
struct AllocatedBuffer {
    VkBuffer buffer;
    VkDeviceMemory allocation;

    void destroy(VkDevice device) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, allocation, nullptr);
    }
};

AllocatedBuffer createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);


// image
struct AllocatedImage {
    VkImage image;
    VkDeviceMemory allocation;

    void destroy(VkDevice device) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, allocation, nullptr);
    }
};
struct ImageMap {
    AllocatedImage allocatedImage;
    VkImageView imageView;

    void destroy(VkDevice device) {
        vkDestroyImageView(device, imageView, nullptr);
        allocatedImage.destroy(device);
    }
};

AllocatedImage createImage(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);


// pipeline
struct PipelineBuilder
{
    VkPipelineLayout pipelineLayout;
    VkPipelineShaderStageCreateInfo shaderStages[2];
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineViewportStateCreateInfo viewportState{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    VkPipeline buildPipeline(VkDevice device, VkRenderPass renderPass, const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
};

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
std::vector<char> readFile(const std::string& filename);