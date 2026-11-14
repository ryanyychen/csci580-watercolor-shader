#pragma once
#include <filesystem>
#include <vector>
#include <glad/gl.h>
#include "Object.h"

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
    void Draw() const override;

  private:
    // VAO describes attribute layout; VBO owns interleaved position/normal data.
    // Copying is disabled because two Mesh instances must not own the same handles.
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLsizei count_ = 0;
};
