#include "vk_mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


void Mesh::loadFromObj(const char* filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void Mesh::upload(VkCommandPool commandPool) {
    // Vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();

    AllocatedBuffer vertexStagingBuffer{};
    vertexStagingBuffer = device->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* vertexData;
    vkMapMemory(device->vkDevice, vertexStagingBuffer.allocation, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(device->vkDevice, vertexStagingBuffer.allocation);

    vertexBuffer = device->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device->copyBuffer(commandPool, vertexStagingBuffer.buffer, vertexBuffer.buffer, vertexBufferSize);

    vertexStagingBuffer.destroy(device->vkDevice);

    // Index buffer
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    AllocatedBuffer indexStagingBuffer{};
    indexStagingBuffer = device->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* indexData{};
    vkMapMemory(device->vkDevice, indexStagingBuffer.allocation, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device->vkDevice, indexStagingBuffer.allocation);

    indexBuffer = device->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device->copyBuffer(commandPool, indexStagingBuffer.buffer, indexBuffer.buffer, indexBufferSize);

    indexStagingBuffer.destroy(device->vkDevice);
}

void Mesh::destroy() {
    indexBuffer.destroy(device->vkDevice);
    vertexBuffer.destroy(device->vkDevice);
}
