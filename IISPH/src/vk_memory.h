#pragma once

#include "vk_utils.h"


struct AllocatedBuffer {
	VkBuffer buffer;
	VkDeviceMemory allocation;

    void create(
        VkPhysicalDevice physicalDevice, 
        VkDevice device, VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties);

    void destroy(VkDevice device);
};


struct AllocatedImage {
	VkImage image;
	VkDeviceMemory allocation;

    void create(
        VkPhysicalDevice physicalDevice, 
        VkDevice device, uint32_t width, uint32_t height,
        uint32_t mipLevels, VkSampleCountFlagBits numSamples, 
        VkFormat format, VkImageTiling tiling, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties);


    void destroy(VkDevice device);
};