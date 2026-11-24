#include "SceneImporter.h"
#include <tiny_obj_loader.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{

glm::vec3 positionAt(const tinyobj::attrib_t &attributes, int index)
{
    if (index < 0 || static_cast<size_t>(index) >= attributes.vertices.size() / 3)
        throw std::runtime_error("OBJ has an invalid position index");
    const size_t i = static_cast<size_t>(index) * 3;
    glm::vec3 position(attributes.vertices[i], attributes.vertices[i + 1],
                       attributes.vertices[i + 2]);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z))
        throw std::runtime_error("OBJ has a non-finite position");
    return position;
}

} // namespace

ImportedSceneData loadSceneFiles(const std::vector<std::filesystem::path> &paths)
{
    if (paths.empty())
        throw std::runtime_error("Expected at least one OBJ file");

    ImportedSceneData imported;
    for (const auto &path : paths)
    {
        const size_t firstTriangle = imported.triangles.size();
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;

        config.triangulate = true;
        config.mtl_search_path = path.parent_path().string();

        if (!reader.ParseFromFile(path.string(), config))
            throw std::runtime_error("Cannot load OBJ " + path.string() + ": " + reader.Error());
        if (!reader.Warning().empty())
            std::cerr << reader.Warning();

        const auto &attributes = reader.GetAttrib();
        const size_t baseVertex = imported.positions.size();
        const size_t numVertices = attributes.vertices.size() / 3;
        const size_t maxIndex = static_cast<size_t>(std::numeric_limits<unsigned int>::max());
        if (baseVertex > maxIndex || numVertices > maxIndex - baseVertex)
            throw std::overflow_error("Too many geometry vertices");

        imported.positions.reserve(baseVertex + numVertices);
        for (size_t i = 0; i < numVertices; ++i)
            imported.positions.push_back(positionAt(attributes, static_cast<int>(i)));

        for (const auto &shape : reader.GetShapes())
        {
            size_t indexOffset = 0;
            for (const unsigned char faceSize : shape.mesh.num_face_vertices)
            {
                if (indexOffset > shape.mesh.indices.size() ||
                    faceSize > shape.mesh.indices.size() - indexOffset)
                    throw std::runtime_error("Invalid OBJ face indices");
                if (faceSize != 3)
                    throw std::runtime_error(
                        "OBJ triangulation failed; expected triangles");

                const auto *indices = shape.mesh.indices.data() + indexOffset;
                int localIndices[3] = {
                    indices[0].vertex_index,
                    indices[1].vertex_index,
                    indices[2].vertex_index};
                for (const int index : localIndices)
                    if (index < 0 || static_cast<size_t>(index) >= numVertices)
                        throw std::runtime_error("OBJ has an invalid position index");

                glm::vec3 positions[3] = {
                    positionAt(attributes, localIndices[0]),
                    positionAt(attributes, localIndices[1]),
                    positionAt(attributes, localIndices[2])};
                glm::vec3 faceNormal =
                    glm::cross(positions[1] - positions[0], positions[2] - positions[0]);
                const float area = glm::length(faceNormal);
                indexOffset += faceSize;
                if (area < 1e-12f || !std::isfinite(area))
                    continue;
                faceNormal /= area;

                ImportedSceneTriangle triangle{};
                for (int i = 0; i < 3; ++i)
                {
                    triangle.vertexIndices[i] =
                        static_cast<unsigned int>(baseVertex + localIndices[i]);

                    glm::vec3 normal = faceNormal;
                    const int normalIndex = indices[i].normal_index;
                    if (normalIndex >= 0 &&
                        static_cast<size_t>(normalIndex) < attributes.normals.size() / 3)
                    {
                        const size_t n = static_cast<size_t>(normalIndex) * 3;
                        const glm::vec3 supplied(attributes.normals[n],
                                                 attributes.normals[n + 1],
                                                 attributes.normals[n + 2]);
                        const float length = glm::length(supplied);
                        if (length > 1e-12f && std::isfinite(length))
                            normal = supplied / length;
                    }
                    triangle.normals[i] = normal;

                    const int texcoordIndex = indices[i].texcoord_index;
                    if (texcoordIndex >= 0 &&
                        static_cast<size_t>(texcoordIndex) < attributes.texcoords.size() / 2)
                    {
                        const size_t t = static_cast<size_t>(texcoordIndex) * 2;
                        triangle.texcoords[i] =
                            glm::vec2(attributes.texcoords[t], attributes.texcoords[t + 1]);
                        if (!std::isfinite(triangle.texcoords[i].x) ||
                            !std::isfinite(triangle.texcoords[i].y))
                            imported.hasTextureCoordinates = false;
                    }
                    else
                    {
                        imported.hasTextureCoordinates = false;
                    }
                }
                imported.triangles.push_back(triangle);
            }
        }

        if (imported.triangles.size() == firstTriangle)
            throw std::runtime_error("OBJ has no non-degenerate triangles: " + path.string());
        std::cout << "Loaded " << path << ": "
                  << imported.triangles.size() - firstTriangle << " triangles\n";
    }

    return imported;
}
