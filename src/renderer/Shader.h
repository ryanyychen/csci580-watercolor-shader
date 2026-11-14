#pragma once
#include <filesystem>
#include <glad/gl.h>
#include <glm/glm.hpp>

// Owns a linked vertex/fragment program, not the material or mesh using it.
// Construction, use, and destruction require a current OpenGL context.
class Shader
{
  public:
    Shader(const std::filesystem::path &vertex, const std::filesystem::path &fragment);
    ~Shader();
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    void use() const;
    // Bind with use() before setting uniforms; names must match the GLSL declarations.
    void set(const char *name, const glm::mat4 &value) const;
    void set(const char *name, const glm::vec3 &value) const;
    void set(const char *name, float value) const;
    void set(const char *name, int value) const;

  private:
    GLuint id_ = 0;
};
