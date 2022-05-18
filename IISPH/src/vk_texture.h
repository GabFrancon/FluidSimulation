#pragma once

//local
#include "vk_helper.h"


class Texture {
public:
	int texWidth;
	int texHeight;
	int texChannels;
	ImageMap albedoMap;
	VkSampler sampler;
	uint32_t mipLevels;

	void loadFromFile(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, const char* filepath);
	void generateMipmaps(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkFormat imageFormat);
	void destroy(VkDevice device);
};

