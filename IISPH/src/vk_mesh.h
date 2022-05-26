#pragma once

#include "vk_device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


struct VertexInputDescription {
    std::array<VkVertexInputBindingDescription, 1> bindings;
    std::array<VkVertexInputAttributeDescription, 2> attributes;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;

    static VertexInputDescription getVertexDescription() {
        VertexInputDescription description{};

        VkVertexInputBindingDescription mainBinding{};
        mainBinding.binding = 0;
        mainBinding.stride = sizeof(Vertex);
        mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription positionAttribute{};
        positionAttribute.binding = 0;
        positionAttribute.location = 0;
        positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionAttribute.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription texCoordAttribute{};
        texCoordAttribute.binding = 0;
        texCoordAttribute.location = 1;
        texCoordAttribute.format = VK_FORMAT_R32G32_SFLOAT;
        texCoordAttribute.offset = offsetof(Vertex, texCoord);

        description.bindings = { mainBinding };
        description.attributes = { positionAttribute, texCoordAttribute };

        return description;
    }
    bool operator==(const Vertex& other) const {
        return pos == other.pos && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return (hash<glm::vec3>()(vertex.pos) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
};

class Mesh {
public:
    VulkanDevice* device;

    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    std::vector<uint32_t> indices;
    AllocatedBuffer indexBuffer;

    Mesh(){}
    Mesh(VulkanDevice* device) : device(device) { }

    void loadFromObj(const char* filepath);
    void upload(VkCommandPool commandPool);
    void destroy();
};

