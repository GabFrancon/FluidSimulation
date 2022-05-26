#pragma once

#include "vk_context.h"
#include "vk_descriptor.h"
#include "vk_texture.h"
#include "vk_pipeline.h"

class Material
{
public:
    VulkanContext* context;

    VulkanPipeline pipeline;
    Texture* texture;
    VkDescriptorSet textureDescriptor;

    Material() {}
    Material(VulkanContext* context, VulkanPipeline pipeline, Texture* texture) : 
        context(context), pipeline(pipeline), texture(texture) {}

    void updatePipeline(std::vector<VkDescriptorSetLayout> layouts, VkExtent2D extent, VkRenderPass renderPass);
    void updateTexture(VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool);
};

