#pragma once

//local
#include "vk_memory.h"

// std
#include <cmath>


struct ImageMap{
	AllocatedImage allocatedImage;
	VkImageView imageView;

	void destroy(VkDevice device);
};


class Texture {
public:
	int texWidth;
	int texHeight;
	int texChannels;
	ImageMap albedoMap;
	VkSampler sampler;
	uint32_t mipLevels;

	void loadFromFile(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, const char* filepath);
	void destroy(VkDevice device);

private:
	void generateMipmaps(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkFormat imageFormat);
	void createTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t mipLevels);
};

