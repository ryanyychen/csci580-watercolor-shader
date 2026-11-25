#pragma once

#include <array>
#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

struct ImportedMaterial
{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
};

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
    std::optional<ImportedMaterial> material;
    bool hasTextureCoordinates = true;
};

ImportedSceneData loadSceneFiles(const std::vector<std::filesystem::path> &paths); // squash all mesh data into one sceneData object
std::vector<ImportedSceneData> loadSceneMeshes(const std::vector<std::filesystem::path> &paths);
