#pragma once
#include <filesystem>
#include <tiny_obj_loader.h>
#include <vector>
#include <glad/gl.h>
#include "Object.h"

struct PrincipalCurvature
{
    float k1 = 0.0f;
    float k2 = 0.0f;

    glm::vec3 dir1{0.0f};
    glm::vec3 dir2{0.0f};
};

struct GeometryVertex
{
    glm::vec3 position;
    glm::vec3 normal;

    // indices to connecting vertices
    std::vector<unsigned int> neighbors;

    PrincipalCurvature curvature;
};

struct Triangle
{
    unsigned int v0;
    unsigned int v1;
    unsigned int v2;
};

// One or more OBJ files become one draw call. Material groups and UVs are deferred.
class Mesh : public Object
{
  public:
    // Requires a current OpenGL context; imports, normalizes, and uploads the mesh.
    explicit Mesh(const std::filesystem::path &path);
    // Parts must share an exported coordinate system; normalize the assembly once.
    explicit Mesh(const std::vector<std::filesystem::path> &paths);
    ~Mesh() override;
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    void DrawVertexNormals() const override;
    void DrawCurvatureDirections() const override;
    void Draw() const override;

  private:
    std::vector<GeometryVertex> vertices_; // list of unique vertices
    std::vector<Triangle> triangles_; // indexed triangle mesh

    std::vector<PrincipalCurvature> curvatureData_;

    void createIndexedMesh(const tinyobj::ObjReader reader, size_t baseVertex);
    void populateVertexNeighbors();
    void populateVertexNormals();
    void populateVertexCurvatures();
    std::vector<glm::vec3> createNormalVisualization() const; // return a list of normal start/end points to send to gpu for rendering
    std::vector<glm::vec3> createCurveDirectionVisuals() const;
    void setupNormalBuffers();
    void setupCurveBuffers();

    // VAO describes attribute layout; VBO owns interleaved position/normal data.
    // Copying is disabled because two Mesh instances must not own the same handles.
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLsizei count_ = 0;

    // for normal visualization
    GLuint normalVAO_ = 0;
    GLuint normalVBO_ = 0;
    GLsizei normalCount_ = 0;

    // curvature visualization
    GLuint curveVAO_ = 0;
    GLuint curveVBO_ = 0;
    GLsizei curveCount_ = 0;
};
