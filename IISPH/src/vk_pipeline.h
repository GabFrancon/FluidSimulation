#pragma once

#include "vk_device.h"
#include "vk_mesh.h"



class VulkanPipeline
{
public:
    VulkanDevice* device;

	VkPipeline vkPipeline;
	VkPipelineLayout pipelineLayout;

    std::vector<char> vertexShader;
    std::vector<char> fragmentShader;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

    VulkanPipeline(){}
    VulkanPipeline(VulkanDevice* vulkanDevice, const std::string& vertexPath, const std::string& fragmentPath, VkPolygonMode mode) {
        device = vulkanDevice;
        polygonMode = mode;
        vertexShader = readFile(vertexPath);
        fragmentShader = readFile(fragmentPath);
    }

    void createPipelineLayout(std::vector<VkDescriptorSetLayout> descriptorLayouts);
    void createPipeline(VkExtent2D extent, VkRenderPass renderPass);
    void destroy();

private:
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

