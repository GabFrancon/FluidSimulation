#pragma once

#include "vk_memory.h"
#include "vk_mesh.h"

class PipelineBuilder
{
public:
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	VkPipelineDepthStencilStateCreateInfo depthStencil{};


	VkPipeline buildPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

private:
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

