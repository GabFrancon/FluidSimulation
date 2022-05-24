#pragma once

//local
#include "vk_device.h"


class Texture {
public:
	int texWidth;
	int texHeight;
	int texChannels;

	ImageMap  albedoMap;
	VkSampler sampler;
	uint32_t  mipLevels;

	void loadFromFile(VulkanDevice* vulkanDevice, VkCommandPool commandPool, const char* filepath);
	void setTextureSampler(VulkanDevice* vulkanDevice, VkFilter filter, VkSamplerAddressMode addressMode);
	void generateMipmaps(VulkanDevice* vulkanDevice, VkCommandPool commandPool, VkFormat imageFormat);
	void destroy(VkDevice device);
};

