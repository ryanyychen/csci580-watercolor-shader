#pragma once

#include <array>
#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

struct ImportedSceneTriangle
{
    std::array<unsigned int, 3> vertexIndices;
    std::array<glm::vec3, 3> normals;
    std::array<glm::vec2, 3> texcoords;
};

struct ImportedSceneData
{
    std::vector<glm::vec3> positions;
    std::vector<ImportedSceneTriangle> triangles;
    bool hasTextureCoordinates = true;
};

ImportedSceneData loadSceneFiles(const std::vector<std::filesystem::path> &paths);
