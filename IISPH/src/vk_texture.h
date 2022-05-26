#pragma once

#include "vk_device.h"


class Texture {
public:
	VulkanDevice* device;

	int texWidth;
	int texHeight;
	int texChannels;

	ImageMap  albedoMap;
	VkSampler sampler;
	uint32_t  mipLevels;

	Texture() {}
	Texture(VulkanDevice* device) : device(device) {}

	void loadFromFile(VkCommandPool commandPool, const char* filepath);
	void setTextureSampler(VkFilter filter, VkSamplerAddressMode addressMode);
	void generateMipmaps(VkCommandPool commandPool, VkFormat imageFormat);
	void destroy();
};

