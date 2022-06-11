#include "vk_mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


void Mesh::loadFromObj(const char* filepath, bool autoTexCoord, bool autoNormal) {
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

            if (!autoNormal && attrib.normals.size() > 0) {
                vertex.normal = {
                    attrib.normals[3 * (size_t)index.normal_index + 0],
                    attrib.normals[3 * (size_t)index.normal_index + 1],
                    attrib.normals[3 * (size_t)index.normal_index + 2]
                };
            }

            if (!autoTexCoord && attrib.texcoords.size() > 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * (size_t)index.texcoord_index + 0],
                    attrib.texcoords[2 * (size_t)index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
    if (autoNormal)
        computeNormals();

    if (autoTexCoord)
        computePlanarTexCoords();
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
    std::cout << "Mesh saved to disk" << std::endl;

}

void Mesh::destroy() {
    indexBuffer.destroy(context->device);
    vertexBuffer.destroy(context->device);
}

void Mesh::subdivideLoop() {
    // Declare new vertices and new triangles. Initialize the new positions for the even vertices with (0,0,0):
    std::vector<Vertex> newVertices(vertices.size(), Vertex());
    std::vector<uint32_t> newIndices;

    struct Edge {
        unsigned int a, b;
        Edge(unsigned int c, unsigned int d) : a(std::min<unsigned int>(c, d)), b(std::max<unsigned int>(c, d)) {}
        bool operator < (Edge const& o) const { return a < o.a || (a == o.a && b < o.b); }
        bool operator == (Edge const& o) const { return a == o.a && b == o.b; }
    };

    std::map< Edge, unsigned int > newVertexOnEdge; // this will be useful to find out whether we already inserted an odd vertex or not
    std::map< Edge, std::set< unsigned int > > trianglesOnEdge; // this will be useful to find out if an edge is boundary or not
    std::vector< std::set< unsigned int > > neighboringVertices(vertices.size()); // this will be used to store the adjacent vertices, i.e., neighboringVertices[i] will be the list of vertices that are adjacent to vertex i.
    std::vector< bool > evenVertexIsBoundary(vertices.size(), false);


    // I) First, compute the valences of the even vertices, the neighboring vertices required to update the position of the even vertices, and the boundaries :
    for (unsigned int i = 0; i < indices.size(); i += 3)
    {
        unsigned int a = indices[i + 0];
        unsigned int b = indices[i + 1];
        unsigned int c = indices[i + 2];

        neighboringVertices[a].insert(b);
        neighboringVertices[a].insert(c);
        neighboringVertices[b].insert(a);
        neighboringVertices[b].insert(c);
        neighboringVertices[c].insert(a);
        neighboringVertices[c].insert(b);

        // Remember the faces shared by the edge
        Edge Eab(a, b);
        trianglesOnEdge[Eab].insert(i);
        Edge Ebc(b, c);
        trianglesOnEdge[Ebc].insert(i);
        Edge Eca(c, a);
        trianglesOnEdge[Eca].insert(i);
    }

    // The valence of a vertex is the number of adjacent vertices:
    std::vector< unsigned int > evenVertexValence(vertices.size(), 0);
    for (unsigned int i = 0; i < vertices.size(); ++i)
        evenVertexValence[i] = neighboringVertices[i].size();

    // Identify extraordinary vertices
    for (unsigned int i = 0; i < vertices.size(); ++i)
        evenVertexIsBoundary[i] = evenVertexValence[i] <= 3;

    // II) Then, compute the positions for the even vertices :
    for (unsigned int i = 0; i < vertices.size(); ++i)
    {
        // Compute the coordinates for even vertices - check both the cases - ordinary and extraordinary
        int n = evenVertexValence[i];
        float beta = 3.;

        if (evenVertexIsBoundary[i])
            beta /= 16;
        else
            beta /= (8 * n);

        newVertices[i].position = (1 - n * beta) * vertices[i].position;
        for (auto& it : neighboringVertices[i])
            newVertices[i].position += beta * vertices[it].position;
    }

    // III) Then, compute the odd vertices:
    for (unsigned int i = 0; i <indices.size(); i += 3)
    {
        unsigned int a = indices[i + 0];
        unsigned int b = indices[i + 1];
        unsigned int c = indices[i + 2];


        Edge Eab(a, b);
        unsigned int oddVertexOnEdgeEab = 0;
        if (newVertexOnEdge.find(Eab) == newVertexOnEdge.end())
        {
            Vertex vertex{};
            vertex.position = (vertices[a].position + vertices[b].position) / 2.0f;
            newVertices.push_back(vertex);
            oddVertexOnEdgeEab = newVertices.size() - 1;
            newVertexOnEdge[Eab] = oddVertexOnEdgeEab;
        }
        else
        {
            oddVertexOnEdgeEab = newVertexOnEdge[Eab];
            newVertices[oddVertexOnEdgeEab].position = 3.0f * newVertices[oddVertexOnEdgeEab].position / 4.0f;

            for (unsigned int faceID : trianglesOnEdge[Eab]) 
                for (int j = 0; j < 3; j++) {
                    unsigned int v = indices[faceID + j];
                    if (v != a && v != b)
                        newVertices[oddVertexOnEdgeEab].position += vertices[v].position / 8.0f;
                }

        }

        Edge Ebc(b, c);
        unsigned int oddVertexOnEdgeEbc = 0;
        if (newVertexOnEdge.find(Ebc) == newVertexOnEdge.end())
        {
            Vertex vertex{};
            vertex.position = (vertices[b].position + vertices[c].position) / 2.0f;
            newVertices.push_back(vertex);
            oddVertexOnEdgeEbc = newVertices.size() - 1;
            newVertexOnEdge[Ebc] = oddVertexOnEdgeEbc;
        }
        else
        {
            oddVertexOnEdgeEbc = newVertexOnEdge[Ebc];
            newVertices[oddVertexOnEdgeEbc].position = 3.0f * newVertices[oddVertexOnEdgeEbc].position / 4.0f;

            for (unsigned int faceID : trianglesOnEdge[Ebc])
                for (int j = 0; j < 3; j++) {
                    unsigned int v = indices[faceID + j];
                    if (v != b && v != c)
                        newVertices[oddVertexOnEdgeEbc].position += vertices[v].position / 8.0f;
                }
        }

        Edge Eca(c, a);
        unsigned int oddVertexOnEdgeEca = 0;
        if (newVertexOnEdge.find(Eca) == newVertexOnEdge.end())
        {
            Vertex vertex{};
            vertex.position = (vertices[c].position + vertices[a].position) / 2.0f;
            newVertices.push_back(vertex);
            oddVertexOnEdgeEca = newVertices.size() - 1;
            newVertexOnEdge[Eca] = oddVertexOnEdgeEca;
        }
        else
        {
            oddVertexOnEdgeEca = newVertexOnEdge[Eca];
            newVertices[oddVertexOnEdgeEca].position = 3.0f * newVertices[oddVertexOnEdgeEca].position / 4.0f;
            for (unsigned int faceID : trianglesOnEdge[Eca])
                for (int j = 0; j < 3; j++) {
                    unsigned int v = indices[faceID + j];
                    if (v != c && v != a)
                        newVertices[oddVertexOnEdgeEca].position += vertices[v].position / 8.0f;
                }
        }

        // set new triangles :
        std::vector<uint32_t> trianglesToAdd = {
             a, oddVertexOnEdgeEab, oddVertexOnEdgeEca,
             oddVertexOnEdgeEab, b, oddVertexOnEdgeEbc,
             oddVertexOnEdgeEca, oddVertexOnEdgeEbc, c,
             oddVertexOnEdgeEab, oddVertexOnEdgeEbc, oddVertexOnEdgeEca
        };

        newIndices.insert(newIndices.end(), trianglesToAdd.begin(), trianglesToAdd.end());
    }

    // after that:
    indices = newIndices;
    vertices = newVertices;
    computeNormals();
    computePlanarTexCoords();
}

void Mesh::computeNormals() {
    for (Vertex& v : vertices)
        v.normal = { 0.0f, 0.0f, 0.0f };

    for (unsigned int i = 0; i < indices.size(); i += 3) {

        glm::vec3 newNormal = glm::cross(
            vertices[indices[i + 1]].position - vertices[indices[i + 0]].position,
            vertices[indices[i + 2]].position - vertices[indices[i + 0]].position);

        vertices[indices[i + 0]].normal += newNormal;
        vertices[indices[i + 1]].normal += newNormal;
        vertices[indices[i + 2]].normal += newNormal;
    }

    for (Vertex& v : vertices)
        glm::normalize(v.normal);
}

void Mesh::computePlanarTexCoords() {
    for (Vertex& v : vertices)
        v.texCoord = { 0.0f, 0.0f };

    float xMin = FLT_MAX, xMax = FLT_MIN;
    float yMin = FLT_MAX, yMax = FLT_MIN;
    float zMin = FLT_MAX, zMax = FLT_MIN;

    for (Vertex& v : vertices) {
        xMin = std::min(xMin, v.position.x);
        xMax = std::max(xMax, v.position.x);
        yMin = std::min(yMin, v.position.y);
        yMax = std::max(yMax, v.position.y);
        zMin = std::min(zMin, v.position.z);
        zMax = std::max(zMax, v.position.z);
    }

    for (Vertex& v : vertices) {
        float x = fabs(v.normal.x);
        float y = fabs(v.normal.y);
        float z = fabs(v.normal.z);

        if (x >= y && x >= z) {
            v.texCoord = { (v.position.y - yMin) / (yMax - yMin), (v.position.z - zMin) / (zMax - zMin) };        
        }
        else if (y >= x && y >= z) {
            v.texCoord = { (v.position.z - zMin) / (zMax - zMin), (v.position.x - xMin) / (xMax - xMin) };
        }
        else if (z >= x && z >= y) {
            v.texCoord = { (v.position.x - xMin) / (xMax - xMin), (v.position.y - yMin) / (yMax - yMin) };
        }
    }
}

void Mesh::computeSphericalTexCoords() {
    for (Vertex& v : vertices)
        v.texCoord = { 0.0f, 0.0f };

    for (Vertex& v : vertices) {
        if (v.position.z != 0)
            v.texCoord.x = atan(sqrt(v.position.x * v.position.x + v.position.y * v.position.y) / v.position.z);

        if (v.position.x > 0)
            v.texCoord.y = atan(v.position.y / v.position.x);
        else if (v.position.x < 0 && v.position.y >= 0)
            v.texCoord.y = atan(v.position.y / v.position.x) + 3.14f;
        else if (v.position.x < 0 && v.position.y < 0)
            v.texCoord.y = atan(v.position.y / v.position.x) - 3.14f;
        else if (v.position.x == 0 && v.position.y > 0)
            v.texCoord.y = 3.14f / 2.0f;
        else if (v.position.x == 0 && v.position.y < 0)
            v.texCoord.y = -3.14f / 2.0f;

        v.texCoord.x = abs(v.texCoord.x) / 3.14f;
        v.texCoord.y = abs(v.texCoord.y) / (2 * 3.14f);
    }
}
