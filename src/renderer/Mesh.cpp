#include "Mesh.h"
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
// Match shader locations 0 and 1 with a position and normal for each corner.
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

glm::vec3 positionAt(const tinyobj::attrib_t &attributes, int index)
{
    if (index < 0 || static_cast<size_t>(index) >= attributes.vertices.size() / 3)
        throw std::runtime_error("OBJ has an invalid position index");
    const size_t i = static_cast<size_t>(index) * 3;
    glm::vec3 p(attributes.vertices[i], attributes.vertices[i + 1], attributes.vertices[i + 2]);
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::runtime_error("OBJ has a non-finite position");
    return p;
}
} // namespace

Mesh::Mesh(const std::filesystem::path &path) : Mesh(std::vector<std::filesystem::path>{path})
{
}

Mesh::Mesh(const std::vector<std::filesystem::path> &paths)
{
    if (paths.empty())
        throw std::runtime_error("Expected at least one OBJ file");

    std::vector<Vertex> vertices;
    // Append parts in their original coordinates, then fit the combined bounds.
    // Centering individual files would stack wheels, wings, and body at the origin.
    for (const auto &path : paths)
    {
        const size_t firstVertex = vertices.size();
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;

        // Polygon conversion happens on the CPU; the GPU receives triangles only.
        config.triangulate = true;
        config.mtl_search_path = path.parent_path().string();

        if (!reader.ParseFromFile(path.string(), config))
            throw std::runtime_error("Cannot load OBJ " + path.string() + ": " + reader.Error());
        if (!reader.Warning().empty())
            std::cerr << reader.Warning();

        const auto &attributes = reader.GetAttrib();

        // Expand triangles to support OBJ's independent position/normal indices.
        for (const auto &shape : reader.GetShapes())
        {
            size_t offset = 0;
            for (const auto faceSize : shape.mesh.num_face_vertices)
            {
                if (faceSize != 3)
                    throw std::runtime_error(
                        "OBJ triangulation failed; export a triangulated mesh");

                const auto *indices = shape.mesh.indices.data() + offset;
                glm::vec3 p[3];

                for (int i = 0; i < 3; ++i)
                    p[i] = positionAt(attributes, indices[i].vertex_index);

                // Cross-product length is twice the triangle area. Zero-area faces have
                // no usable normal, so skip them before normalization.
                glm::vec3 faceNormal = glm::cross(p[1] - p[0], p[2] - p[0]);
                const float area = glm::length(faceNormal);
                offset += 3;

                if (area < 1e-12f || !std::isfinite(area))
                    continue;
                faceNormal /= area;

                for (int i = 0; i < 3; ++i)
                {
                    // Prefer exported per-corner normals for smooth/hard-edge shading.
                    // Missing or unusable normals fall back to this triangle's flat normal.
                    glm::vec3 normal = faceNormal;
                    const int normalIndex = indices[i].normal_index;
                    if (normalIndex >= 0 &&
                        static_cast<size_t>(normalIndex) < attributes.normals.size() / 3)
                    {
                        const size_t n = static_cast<size_t>(normalIndex) * 3;
                        glm::vec3 supplied(attributes.normals[n], attributes.normals[n + 1],
                                           attributes.normals[n + 2]);
                        const float length = glm::length(supplied);
                        if (length > 1e-12f && std::isfinite(length))
                            normal = supplied / length;
                    }
                    vertices.push_back({p[i], normal});
                }
            }
        }
        if (vertices.size() == firstVertex)
            throw std::runtime_error("OBJ has no non-degenerate triangles: " + path.string());
        std::cout << "Loaded " << path << ": " << (vertices.size() - firstVertex) / 3
                  << " triangles\n";
    }
    if (vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("OBJ is too large for one draw call");

    glm::vec3 lo = vertices.front().position, hi = lo;
    // Fit arbitrary input units into the default camera's view without distortion.
    // Uniform scaling preserves the directions of the imported normals.
    for (const auto &v : vertices)
    {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }

    const glm::vec3 center = lo * 0.5f + hi * 0.5f;
    const glm::vec3 extent = hi - lo;
    const float span = std::max({extent.x, extent.y, extent.z});
    if (!std::isfinite(span) || span <= 0.0f)
        throw std::runtime_error("OBJ bounds cannot be normalized");

    for (auto &v : vertices)
        v.position = (v.position - center) * (2.0f / span);

    count_ = static_cast<GLsizei>(vertices.size());
    number_parts = count_ / 3;

    // Upload once: CPU import data can be discarded after glBufferData copies it.
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    // Stride skips a whole Vertex; offsetof selects each field within that vertex.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, normal)));
    glBindVertexArray(0);
    std::cout << "Assembly: " << paths.size() << " OBJ file(s), " << count_ / 3 << " triangles\n";
}

Mesh::~Mesh()
{
    // The owning scene must be destroyed while its OpenGL context is still current.
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void Mesh::Draw() const
{
    // Vertices are already expanded, so no element/index buffer is needed here.
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, count_);
}
