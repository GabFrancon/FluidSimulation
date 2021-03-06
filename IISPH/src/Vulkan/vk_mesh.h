#pragma once

#include "vk_context.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <iomanip>

struct VertexInputDescription {
    std::array<VkVertexInputBindingDescription, 1> bindings;
    std::array<VkVertexInputAttributeDescription, 3> attributes;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    Vertex() {
        position = glm::vec3(0.0f);
        normal   = glm::vec3(0.0f);
        texCoord = glm::vec2(0.0f);
    }

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
        positionAttribute.offset = offsetof(Vertex, position);

        VkVertexInputAttributeDescription normalAttribute{};
        normalAttribute.binding = 0;
        normalAttribute.location = 1;
        normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
        normalAttribute.offset = offsetof(Vertex, normal);

        VkVertexInputAttributeDescription texCoordAttribute{};
        texCoordAttribute.binding = 0;
        texCoordAttribute.location = 2;
        texCoordAttribute.format = VK_FORMAT_R32G32_SFLOAT;
        texCoordAttribute.offset = offsetof(Vertex, texCoord);

        description.bindings = { mainBinding };
        description.attributes = { positionAttribute, normalAttribute, texCoordAttribute };

        return description;
    }
    bool operator==(const Vertex& other) const {
        return position == other.position && normal == other.normal && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return (
                (hash<glm::vec3>()(vertex.position) ^
                (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
};

struct Edge {
    uint32_t v0;
    uint32_t v1;

    Edge(uint32_t v0, uint32_t v1)
        : v0(v0 < v1 ? v0 : v1)
        , v1(v0 < v1 ? v1 : v0) {}

    bool operator <(const Edge& o) const { return v0 < o.v0 || (v0 == o.v0 && v1 < o.v1); }
    bool operator == (const Edge& o) const { return v0 == o.v0 && v1 == o.v1; }

};

class Mesh {
public:
    VulkanContext* context;

    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    std::vector<uint32_t> indices;
    AllocatedBuffer indexBuffer;

    Mesh(){}
    Mesh(VulkanContext* context) : context(context) { }

    void loadFromObj(const char* filepath, bool autoTexCoord, bool autoNormal);
    void upload(VkCommandPool commandPool);
    void saveToObj(const char* filepath);
    void destroy();

    void loopSubdivision();
    void sphereSubdivision();
    void laplacianSmooth(unsigned int smoothness);

    void computeNormals();
    void computePlanarTexCoords();
    void computeSphericalTexCoords();

    void genSphere(unsigned int resolution);
};

