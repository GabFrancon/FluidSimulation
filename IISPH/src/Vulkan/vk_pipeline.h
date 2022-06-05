#pragma once

#include "vk_context.h"
#include "vk_mesh.h"



class VulkanPipeline
{
public:
    VulkanContext* context;

	VkPipeline vkPipeline;
	VkPipelineLayout pipelineLayout;

    std::vector<char> vertexShader;
    std::vector<char> fragmentShader;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;

    VulkanPipeline(){}
    VulkanPipeline(VulkanContext* vulkanContext, const std::string& vertexPath, const std::string& fragmentPath, VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL, VkCullModeFlags _cullMode = VK_CULL_MODE_BACK_BIT) {
        context     = vulkanContext;
        polygonMode = _polygonMode;
        cullMode    = _cullMode;

        vertexShader   = readFile(vertexPath);
        fragmentShader = readFile(fragmentPath);
    }

    void createPipelineLayout(std::vector<VkDescriptorSetLayout> descriptorLayouts);
    void createPipeline(VkExtent2D extent, VkRenderPass renderPass);
    void destroy();

private:
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

