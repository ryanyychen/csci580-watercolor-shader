#include "SceneImporter.h"
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

std::optional<ImportedMaterial> assignedMaterial(
    const tinyobj::ObjReader &reader, const std::filesystem::path &path)
{
    const auto &materials = reader.GetMaterials();
    std::optional<int> assignedId;

    for (const auto &shape : reader.GetShapes())
    {
        if (shape.mesh.material_ids.size() != shape.mesh.num_face_vertices.size())
            throw std::runtime_error("OBJ material assignments do not match its faces: " +
                                     path.string());

        for (const int materialId : shape.mesh.material_ids)
        {
            if (materialId < 0)
                continue;
            if (static_cast<size_t>(materialId) >= materials.size())
                throw std::runtime_error("OBJ has an invalid material assignment: " +
                                         path.string());
            if (!assignedId)
                assignedId = materialId;
        }
    }

    if (!assignedId)
        return std::nullopt;

    const tinyobj::material_t &source = materials[static_cast<size_t>(*assignedId)];
    ImportedMaterial material{
        glm::vec3(source.ambient[0], source.ambient[1], source.ambient[2]),
        glm::vec3(source.diffuse[0], source.diffuse[1], source.diffuse[2]),
        glm::vec3(source.specular[0], source.specular[1], source.specular[2]),
        static_cast<float>(source.shininess)};
    const bool hasInvalidMaterialValue =
        !std::isfinite(material.ambient.x) || !std::isfinite(material.ambient.y) ||
        !std::isfinite(material.ambient.z) || !std::isfinite(material.diffuse.x) ||
        !std::isfinite(material.diffuse.y) || !std::isfinite(material.diffuse.z) ||
        !std::isfinite(material.specular.x) || !std::isfinite(material.specular.y) ||
        !std::isfinite(material.specular.z) || !std::isfinite(material.shininess) ||
        material.shininess < 0.0f;
    if (hasInvalidMaterialValue)
    {
        throw std::runtime_error("OBJ material contains invalid values: " + path.string());
    }

    return material;
}

void loadObjFile(ImportedSceneData& imported, const std::filesystem::path& path) {
    const size_t firstTriangle = imported.triangles.size();
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;

        config.triangulate = true;
        config.mtl_search_path = path.parent_path().string();

        if (!reader.ParseFromFile(path.string(), config))
            throw std::runtime_error("Cannot load OBJ " + path.string() + ": " + reader.Error());
        if (!reader.Warning().empty())
            std::cerr << reader.Warning();

        const auto pathMaterial = assignedMaterial(reader, path);
        if (!imported.material && pathMaterial)
            imported.material = pathMaterial;

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

bool isFinite(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::optional<ImportedMaterial> ProcessMaterial(const aiScene &scene, const aiMesh &mesh)
{
    if (scene.mNumMaterials == 0)
        return std::nullopt;
    if (mesh.mMaterialIndex >= scene.mNumMaterials ||
        !scene.mMaterials[mesh.mMaterialIndex])
        throw std::runtime_error("FBX mesh has an invalid material index");

    const aiMaterial &source = *scene.mMaterials[mesh.mMaterialIndex];
    // Keep the renderer's defaults for material properties absent from the FBX.
    ImportedMaterial material{
        glm::vec3(0.45f, 0.16f, 0.06f),
        glm::vec3(0.45f, 0.16f, 0.06f),
        glm::vec3(0.60f),
        48.0f};
    bool hasMaterialProperties = false;

    aiColor3D color;
    if (source.Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
    {
        material.ambient = glm::vec3(color.r, color.g, color.b);
        hasMaterialProperties = true;
    }
    if (source.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    {
        material.diffuse = glm::vec3(color.r, color.g, color.b);
        hasMaterialProperties = true;
    }
    if (source.Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    {
        material.specular = glm::vec3(color.r, color.g, color.b);
        hasMaterialProperties = true;
    }

    ai_real shininess = 0.0f;
    if (source.Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
    {
        material.shininess = static_cast<float>(shininess);
        hasMaterialProperties = true;
    }

    if (!hasMaterialProperties)
        return std::nullopt;

    if (!isFinite(material.ambient) || !isFinite(material.diffuse) ||
        !isFinite(material.specular) || !std::isfinite(material.shininess) ||
        material.shininess < 0.0f)
        throw std::runtime_error("FBX material contains invalid values");

    return material;
}

ImportedSceneData ProcessMesh(const aiMesh &mesh, const aiScene &scene,
                              const aiMatrix4x4 &transform)
{
    ImportedSceneData data;
    data.hasTextureCoordinates = mesh.HasTextureCoords(0);
    data.positions.reserve(mesh.mNumVertices);

    aiMatrix3x3 normalTransform(transform);
    if (mesh.HasNormals())
        normalTransform.Inverse().Transpose();

    data.material = ProcessMaterial(scene, mesh);

    for (unsigned int i = 0; i < mesh.mNumVertices; ++i)
    {
        const aiVector3D transformed = transform * mesh.mVertices[i];
        const glm::vec3 position(transformed.x, transformed.y, transformed.z);
        if (!isFinite(position))
            throw std::runtime_error("FBX contains a non-finite position");
        data.positions.push_back(position);
    }

    for (unsigned int i = 0; i < mesh.mNumFaces; ++i)
    {
        const aiFace &face = mesh.mFaces[i];
        if (face.mNumIndices != 3)
            throw std::runtime_error("Assimp triangulation failed; expected triangles");

        glm::vec3 facePositions[3];
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            const unsigned int vertexIndex = face.mIndices[corner];
            if (vertexIndex >= mesh.mNumVertices)
                throw std::runtime_error("FBX has an invalid vertex index");
            facePositions[corner] = data.positions[vertexIndex];
        }

        glm::vec3 faceNormal =
            glm::cross(facePositions[1] - facePositions[0],
                       facePositions[2] - facePositions[0]);
        const float area = glm::length(faceNormal);
        if (!std::isfinite(area) || area < 1e-12f)
            continue;
        faceNormal /= area;

        ImportedSceneTriangle triangle{};
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            const unsigned int vertexIndex = face.mIndices[corner];
            triangle.vertexIndices[corner] = vertexIndex;

            glm::vec3 normal = faceNormal;
            if (mesh.HasNormals())
            {
                const aiVector3D transformed =
                    normalTransform * mesh.mNormals[vertexIndex];
                const glm::vec3 supplied(transformed.x, transformed.y, transformed.z);
                const float length = glm::length(supplied);
                if (isFinite(supplied) && std::isfinite(length) && length > 1e-12f)
                    normal = supplied / length;
            }
            triangle.normals[corner] = normal;

            if (data.hasTextureCoordinates)
            {
                const aiVector3D &uv = mesh.mTextureCoords[0][vertexIndex];
                triangle.texcoords[corner] = glm::vec2(uv.x, uv.y);
                if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
                {
                    triangle.texcoords[corner] = glm::vec2(0.0f);
                    data.hasTextureCoordinates = false;
                }
            }
        }
        data.triangles.push_back(triangle);
    }

    return data;
}

void ProcessNode(const aiNode &node, const aiScene &scene,
                 const aiMatrix4x4 &parentTransform,
                 std::vector<ImportedSceneData> &meshes)
{
    const aiMatrix4x4 transform = parentTransform * node.mTransformation;

    for (unsigned int i = 0; i < node.mNumMeshes; ++i)
    {
        const unsigned int meshIndex = node.mMeshes[i];
        if (meshIndex >= scene.mNumMeshes || !scene.mMeshes[meshIndex])
            throw std::runtime_error("FBX node has an invalid mesh index");
        meshes.push_back(ProcessMesh(*scene.mMeshes[meshIndex], scene, transform));
    }

    for (unsigned int i = 0; i < node.mNumChildren; ++i)
    {
        if (!node.mChildren[i])
            throw std::runtime_error("FBX node has a null child");
        ProcessNode(*node.mChildren[i], scene, transform, meshes);
    }
}

void squashMeshes(ImportedSceneData &imported,
                  const std::vector<ImportedSceneData> &meshes)
{
    const size_t maxIndex = std::numeric_limits<unsigned int>::max();
    for (const ImportedSceneData &mesh : meshes)
    {
        const size_t baseVertex = imported.positions.size();
        if (baseVertex > maxIndex || mesh.positions.size() > maxIndex - baseVertex)
            throw std::overflow_error("Too many FBX vertices");

        imported.positions.insert(imported.positions.end(),
                                  mesh.positions.begin(), mesh.positions.end());
        imported.hasTextureCoordinates =
            imported.hasTextureCoordinates && mesh.hasTextureCoordinates;
        if (!imported.material && mesh.material)
            imported.material = mesh.material;

        imported.triangles.reserve(imported.triangles.size() + mesh.triangles.size());
        for (const ImportedSceneTriangle &source : mesh.triangles)
        {
            ImportedSceneTriangle triangle = source;
            for (unsigned int &vertexIndex : triangle.vertexIndices)
            {
                if (vertexIndex >= mesh.positions.size())
                    throw std::runtime_error("FBX triangle has an invalid vertex index");
                vertexIndex += static_cast<unsigned int>(baseVertex);
            }
            imported.triangles.push_back(triangle);
        }
    }
}

std::vector<ImportedSceneData> loadFbxFile(const std::filesystem::path &path)
{
    std::vector<ImportedSceneData> meshes;
    Assimp::Importer importer;

    const unsigned int flags =
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices;
    const aiScene* scene = importer.ReadFile(path.string().c_str(), flags);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode)
    {
        throw std::runtime_error("Assimp cannot load " + path.string() + ": " +
                                 importer.GetErrorString());
    }

    ProcessNode(*scene->mRootNode, *scene, aiMatrix4x4(), meshes);

    return meshes;
}

} // namespace

// load all scene files and squash them into one mesh
ImportedSceneData loadSceneFiles(const std::vector<std::filesystem::path> &paths)
{
    if (paths.empty())
        throw std::runtime_error("Expected at least one file");

    ImportedSceneData imported;
    for (const auto &path : paths)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
        if (extension == ".obj")
        {
            loadObjFile(imported, path);
        }
        else if (extension == ".fbx")
        {
            const size_t firstTriangle = imported.triangles.size();
            auto meshes = loadFbxFile(path);
            squashMeshes(imported, meshes);
            if (imported.triangles.size() == firstTriangle)
                throw std::runtime_error("FBX has no non-degenerate triangles: " + path.string());
        }
        else {
            throw std::runtime_error("Unsupported scene file: " + path.string());
        }
    }

    return imported;
}

// Keep mesh data separate per object
std::vector<ImportedSceneData> loadSceneMeshes(const std::vector<std::filesystem::path> &paths)
{
    if (paths.empty())
        throw std::runtime_error("Expected at least one file");
    std::vector<ImportedSceneData> meshes;
    for (const auto &path : paths) {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
        if (extension == ".obj") {
            ImportedSceneData meshData;
            loadObjFile(meshData, path);
            meshes.push_back(meshData);
        } else if (extension == ".fbx") {
            std::vector<ImportedSceneData> fbxData = loadFbxFile(path);
            meshes.insert(meshes.end(), fbxData.begin(), fbxData.end());
        } else {
            throw std::runtime_error("Unsupported scene file: " + path.string());
        }
    }
    return meshes;
}
