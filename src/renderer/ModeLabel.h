#pragma once

#include "Shader.h"
#include <array>
#include <string>

// Small screen-space bitmap label. Reuses the flat-color direction shader,
// so the viewer does not need an external font or GUI dependency.
class ModeLabel
{
  public:
    ModeLabel();
    ~ModeLabel();
    ModeLabel(const ModeLabel &) = delete;
    ModeLabel &operator=(const ModeLabel &) = delete;

    void Draw(const Shader &shader, const std::string &text,
              int width, int height, float pixelScale = 1.0f);

  private:
    struct Batch
    {
        GLint first = 0;
        GLsizei count = 0;
        glm::vec3 color{1.0f};
    };

    void Rebuild(const std::string &text, float pixelScale);

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::array<Batch, 4> batches_{};
    std::string text_;
    float pixelScale_ = 0.0f;
};
