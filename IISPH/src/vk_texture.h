#pragma once

#include "vk_context.h"


class Texture {
public:
	VulkanContext* context;

	int texWidth;
	int texHeight;
	int texChannels;

	ImageMap  albedoMap;
	VkSampler sampler;
	uint32_t  mipLevels;

	Texture() {}
	Texture(VulkanContext* context) : context(context) {}

	void loadFromFile(VkCommandPool commandPool, const char* filepath);
	void setTextureSampler(VkFilter filter, VkSamplerAddressMode addressMode);
	void generateMipmaps(VkCommandPool commandPool, VkFormat imageFormat);
	void destroy();
};

