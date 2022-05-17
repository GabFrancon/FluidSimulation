#pragma once

#include "vk_memory.h"
#include "vk_mesh.h"


struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	void destroy(VkDevice device);
};


class PipelineBuilder
{
public:
	VkPipelineLayout pipelineLayout;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	VkPipelineDepthStencilStateCreateInfo depthStencil{};

	VkPipeline buildPipeline(
		VkDevice device, VkRenderPass renderPass, 
		const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

private:
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

