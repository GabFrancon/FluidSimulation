#pragma once

#include "vk_device.h"
#include "vk_descriptor.h"
#include "vk_texture.h"
#include "vk_pipeline.h"

class Material
{
public:
    VulkanDevice* device;

    VulkanPipeline pipeline;
    Texture* texture;
    VkDescriptorSet textureDescriptor;

    Material() {}
    Material(VulkanDevice* device, VulkanPipeline pipeline, Texture* texture) : 
        device(device), pipeline(pipeline), texture(texture) {}

    void updatePipeline(std::vector<VkDescriptorSetLayout> layouts, VkExtent2D extent, VkRenderPass renderPass);
    void updateTexture(VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool);
};

