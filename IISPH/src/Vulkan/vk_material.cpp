#include "vk_material.h"


void Material::updatePipeline(std::vector<VkDescriptorSetLayout> layouts, VkExtent2D extent, VkRenderPass renderPass) {
    pipeline.createPipelineLayout(layouts);
    pipeline.createPipeline(extent, renderPass);
}

void Material::updateTexture(VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool) {
    if (texture == VK_NULL_HANDLE)
        return;

    VkDescriptorSetAllocateInfo textureAllocInfo{};
    textureAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    textureAllocInfo.descriptorPool = descriptorPool;
    textureAllocInfo.descriptorSetCount = 1;
    textureAllocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(context->device, &textureAllocInfo, &textureDescriptor) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate texture descriptor set!");
    }

    VkDescriptorImageInfo textureImageInfo{};
    textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textureImageInfo.imageView = texture->albedoMap.imageView;
    textureImageInfo.sampler = texture->sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = textureDescriptor;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &textureImageInfo;

    vkUpdateDescriptorSets(context->device, 1, &descriptorWrite, 0, nullptr);
}