#pragma once

#include "vk_context.h"
#include "vk_command.h"

#include <glm/glm.hpp>


struct SceneData {
    alignas(16) glm::vec3 lightPosition;
    alignas(16) glm::vec3 lightColor;
};

struct CameraData {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 position;
};

struct ObjectData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 albedo;
};

class VulkanDescriptor
{
public:
    VulkanContext* context;
    const int maxObjectsToRender = 1000;

    VkDescriptorSet globalDescriptorSet;
    AllocatedBuffer cameraBuffer;
    AllocatedBuffer sceneBuffer;

    VkDescriptorSet objectsDescriptorSet;
    AllocatedBuffer objectsBuffer;

    VulkanDescriptor() {}
    VulkanDescriptor(VulkanContext* context, const int maxObjects) : context(context), maxObjectsToRender(maxObjects) {}

    void createBuffers();
    void allocateGlobalDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout);
    void allocateObjectsDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout);
    void updateDescriptors();
    void destroy();
};

