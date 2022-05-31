#include "vk_descriptor.h"


void VulkanDescriptor::createBuffers() {
    VkDeviceSize cameraBufferSize = sizeof(CameraData);
    cameraBuffer = context->createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize sceneBufferSize = sizeof(SceneData);
    sceneBuffer = context->createBuffer(sceneBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize objectsBufferSize = sizeof(ObjectData) * maxObjectsToRender;
	objectsBuffer = context->createBuffer(objectsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void VulkanDescriptor::allocateGlobalDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo globalAllocInfo{};
    globalAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    globalAllocInfo.descriptorPool = descriptorPool;
    globalAllocInfo.descriptorSetCount = 1;
    globalAllocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(context->device, &globalAllocInfo, &globalDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate global descriptor set!");
    }
}

void VulkanDescriptor::allocateObjectsDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo objectsAllocInfo{};
    objectsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objectsAllocInfo.descriptorPool = descriptorPool;
    objectsAllocInfo.descriptorSetCount = 1;
    objectsAllocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(context->device, &objectsAllocInfo, &objectsDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }
}

void VulkanDescriptor::updateDescriptors() {
    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = cameraBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(CameraData);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = globalDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &cameraBufferInfo;


    VkDescriptorBufferInfo sceneBufferInfo{};
    sceneBufferInfo.buffer = sceneBuffer.buffer;
    sceneBufferInfo.offset = 0;
    sceneBufferInfo.range = sizeof(SceneData);

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = globalDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &sceneBufferInfo;


    VkDescriptorBufferInfo objectsBufferInfo{};
    objectsBufferInfo.buffer = objectsBuffer.buffer;
    objectsBufferInfo.offset = 0;
    objectsBufferInfo.range = sizeof(ObjectData) * maxObjectsToRender;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = objectsDescriptorSet;
    descriptorWrites[2].dstBinding = 0;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &objectsBufferInfo;

    vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanDescriptor::destroy() {
    cameraBuffer.destroy(context->device);
    sceneBuffer.destroy(context->device);
    objectsBuffer.destroy(context->device);
}
