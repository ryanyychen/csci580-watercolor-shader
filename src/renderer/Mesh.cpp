#include "Mesh.h"
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

    std::vector<Vertex> vertices; // expanded vertex data for rendering
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

        size_t baseVertex = vertices_.size();
        createIndexedMesh(reader, baseVertex);

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

    for (auto &v : vertices_)
        v.position = (v.position - center) * (2.0f / span);

    populateVertexNeighbors();
    populateVertexNormals();  

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

    setupNormalBuffers();
}

// create indexed mesh by populating vertices_ and triangles_ from obj reader 
// baseVertex specifies the index offset to account for multiple objects 
void Mesh::createIndexedMesh(
    tinyobj::ObjReader reader,
    size_t baseVertex)
{
    const auto& attributes = reader.GetAttrib();

    // 1. Append this OBJ's unique positions.
    const size_t numVertices = attributes.vertices.size() / 3;

    const size_t maxIndex =
        static_cast<size_t>(std::numeric_limits<unsigned int>::max());

    if (baseVertex > maxIndex || numVertices > maxIndex - baseVertex)
        throw std::overflow_error("Too many geometry vertices");

    for (size_t i = 0; i < numVertices; ++i)
    {
        const size_t index = i * 3;

        glm::vec3 position(
            attributes.vertices[index],
            attributes.vertices[index + 1],
            attributes.vertices[index + 2]
        );

        if (!std::isfinite(position.x) ||
            !std::isfinite(position.y) ||
            !std::isfinite(position.z))
        {
            throw std::runtime_error("OBJ has a non-finite position");
        }

        GeometryVertex vertex{};
        vertex.position = position;
        vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);

        vertices_.push_back(vertex);
    }

    // 2. Read triangle connectivity.
    const auto& shapes = reader.GetShapes();

    for (const auto& shape : shapes)
    {
        const auto& mesh = shape.mesh;
        size_t indexOffset = 0;

        for (unsigned char faceSize : mesh.num_face_vertices)
        {
            if (indexOffset > mesh.indices.size() ||
                faceSize > mesh.indices.size() - indexOffset)
            {
                throw std::runtime_error("Invalid OBJ face indices");
            }

            if (faceSize != 3)
            {
                throw std::runtime_error(
                    "OBJ triangulation failed; expected triangles"
                );
            }

            // 3. Retrieve and validate the local OBJ indices.
            int i0 = mesh.indices[indexOffset + 0].vertex_index;
            int i1 = mesh.indices[indexOffset + 1].vertex_index;
            int i2 = mesh.indices[indexOffset + 2].vertex_index;

            if (i0 < 0 || i1 < 0 || i2 < 0 ||
                static_cast<size_t>(i0) >= numVertices ||
                static_cast<size_t>(i1) >= numVertices ||
                static_cast<size_t>(i2) >= numVertices)
            {
                throw std::runtime_error("OBJ has an invalid position index");
            }

            // 4. Convert local indices to global geometry indices.
            unsigned int v0 = static_cast<unsigned int>(baseVertex + i0);
            unsigned int v1 = static_cast<unsigned int>(baseVertex + i1);
            unsigned int v2 = static_cast<unsigned int>(baseVertex + i2);

            // 5. Skip degenerate triangles.
            const glm::vec3& p0 = vertices_[v0].position;
            const glm::vec3& p1 = vertices_[v1].position;
            const glm::vec3& p2 = vertices_[v2].position;

            glm::vec3 crossProduct =
                glm::cross(p1 - p0, p2 - p0);

            float area = glm::length(crossProduct);

            if (area >= 1e-12f && std::isfinite(area))
            {
                triangles_.push_back({v0, v1, v2});
            }

            indexOffset += faceSize;
        }
    }
}

void Mesh::populateVertexNeighbors() {
    for (const auto& tri : triangles_) {
        unsigned int indices[3] = {tri.v0, tri.v1, tri.v2};

        for (int i = 0; i < 3; i++) {
            unsigned int vertexInd = indices[i];
            GeometryVertex& vertex = vertices_[vertexInd];

            // get the other two vertices
            for (int j = 0; j < 3; j++) {
                // skip the current index
                if (i == j) {
                    continue;
                }
                unsigned int neighborInd = indices[j];
                auto& neighbors = vertex.neighbors;
                if (std::find(neighbors.begin(), neighbors.end(), neighborInd) == neighbors.end()) {
                    neighbors.push_back(neighborInd);
                }
            }
        }
    }
}

void Mesh::populateVertexNormals() {
    for (const auto& tri : triangles_) {
        unsigned int ind0 = tri.v0;
        unsigned int ind1 = tri.v1;
        unsigned int ind2 = tri.v2;

        GeometryVertex& v0 = vertices_[ind0];
        GeometryVertex& v1 = vertices_[ind1];
        GeometryVertex& v2 = vertices_[ind2];

        glm::vec3 p0 = v0.position;
        glm::vec3 p1 = v1.position;
        glm::vec3 p2 = v2.position;

        glm::vec3 faceNormal = cross(p1 - p0, p2 - p0);

        v0.normal += faceNormal;
        v1.normal += faceNormal;
        v2.normal += faceNormal;
    }
    for (auto& v : vertices_) {
        v.normal = normalize(v.normal);
    }
}

std::vector<glm::vec3> Mesh::createNormalVisualization() const {
    std::vector<glm::vec3> normalData;
    const float lineLength = 0.1f;

    for (const auto& v : vertices_) {
        glm::vec3 start = v.position;
        glm::vec3 end = v.position + lineLength * v.normal;
        normalData.push_back(start);
        normalData.push_back(end);
    }

    return normalData;
}

void Mesh::setupNormalBuffers()
{
    std::vector<glm::vec3> normalData = createNormalVisualization();

    if (normalData.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
    {
        throw std::runtime_error("Too many normal visualization vertices");
    }
    normalCount_ = static_cast<GLsizei>(normalData.size());

    glGenVertexArrays(1, &normalVAO_);
    glGenBuffers(1, &normalVBO_);

    glBindVertexArray(normalVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, normalVBO_);
    glBufferData(
        GL_ARRAY_BUFFER,
        normalData.size() * sizeof(glm::vec3),
        normalData.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec3),
        nullptr
    );

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    // The owning scene must be destroyed while its OpenGL context is still current.
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &normalVBO_);
    glDeleteVertexArrays(1, &normalVAO_);
}

void Mesh::DrawVertexNormals() const
{
    glBindVertexArray(normalVAO_);
    glDrawArrays(GL_LINES, 0, normalCount_);
    glBindVertexArray(0);
}

void Mesh::Draw() const
{
    // Vertices are already expanded, so no element/index buffer is needed here.
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, count_);
}
