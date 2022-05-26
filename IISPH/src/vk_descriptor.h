#pragma once

#include "vk_context.h"
#include "vk_command.h"

#include <glm/glm.hpp>


struct ObjectData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 albedo;
};

struct CameraData {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};



class VulkanDescriptor
{
public:
    VulkanContext* context;
    const int maxObjectsToRender;

    VkDescriptorSet globalDescriptorSet;
    AllocatedBuffer cameraBuffer;

    VkDescriptorSet objectsDescriptorSet;
    AllocatedBuffer objectsBuffer;

    VulkanDescriptor(VulkanContext* context, const int maxObjects) : context(context), maxObjectsToRender(maxObjects) {}

    void createBuffers();
    void allocateGlobalDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout);
    void allocateObjectsDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout);
    void updateDescriptors();
    void destroy();
};

