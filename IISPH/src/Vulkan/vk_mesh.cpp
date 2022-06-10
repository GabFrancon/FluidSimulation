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

            vertex.position = {
                attrib.vertices[3 * (size_t)index.vertex_index + 0],
                attrib.vertices[3 * (size_t)index.vertex_index + 1],
                attrib.vertices[3 * (size_t)index.vertex_index + 2]
            };

            vertex.normal = {
                attrib.normals[3 * (size_t)index.normal_index + 0],
                attrib.normals[3 * (size_t)index.normal_index + 1],
                attrib.normals[3 * (size_t)index.normal_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * (size_t)index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * (size_t)index.texcoord_index + 1]
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
    vertexStagingBuffer = context->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* vertexData;
    vkMapMemory(context->device, vertexStagingBuffer.allocation, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(context->device, vertexStagingBuffer.allocation);

    vertexBuffer = context->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    context->copyBuffer(commandPool, vertexStagingBuffer.buffer, vertexBuffer.buffer, vertexBufferSize);

    vertexStagingBuffer.destroy(context->device);

    // Index buffer
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    AllocatedBuffer indexStagingBuffer{};
    indexStagingBuffer = context->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* indexData{};
    vkMapMemory(context->device, indexStagingBuffer.allocation, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(context->device, indexStagingBuffer.allocation);

    indexBuffer = context->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    context->copyBuffer(commandPool, indexStagingBuffer.buffer, indexBuffer.buffer, indexBufferSize);

    indexStagingBuffer.destroy(context->device);
}

void Mesh::saveToObj(const char* filepath) {
    std::ofstream file(filepath, std::ios::out | std::ios::binary);

    for (Vertex& v : vertices) {
        file << "v " 
            << std::fixed << std::setprecision(6) << v.position.x << " " 
            << std::fixed << std::setprecision(6) << v.position.y << " " 
            << std::fixed << std::setprecision(6) << v.position.z << "\n";
    }

    for (Vertex& v : vertices) {
        file << "vt "
            << std::fixed << std::setprecision(6) << v.texCoord.x << " "
            << std::fixed << std::setprecision(6) << v.texCoord.y << "\n";
    }

    for (Vertex& v : vertices) {
        file << "vn "
            << std::fixed << std::setprecision(6) << v.normal.x << " "
            << std::fixed << std::setprecision(6) << v.normal.y << " "
            << std::fixed << std::setprecision(6) << v.normal.z << "\n";
    }

    file << "\n";

    for (int i = 0; i < indices.size(); i += 3) {
        file << "f "
            << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << " "
            << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << " "
            << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << "\n";
    }

    file.close();
    std::cout << "Mesh saved saved to disk" << std::endl;

}

void Mesh::destroy() {
    indexBuffer.destroy(context->device);
    vertexBuffer.destroy(context->device);
}