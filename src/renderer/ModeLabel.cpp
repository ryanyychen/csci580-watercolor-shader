#include "ModeLabel.h"
#include <algorithm>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace
{
// Five columns by seven rows. Bits run from the leftmost column to the right.
// Only uppercase text and the punctuation used by the mode label are needed.
std::array<std::uint8_t, 7> glyph(char letter)
{
    switch (letter)
    {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 14};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case '-': return {0, 0, 0, 14, 0, 0, 0};
    default: return {};
    }
}

void rectangle(std::vector<glm::vec2> &vertices, float x, float y, float w, float h)
{
    vertices.insert(vertices.end(), {
        {x, y}, {x + w, y}, {x + w, y + h},
        {x, y}, {x + w, y + h}, {x, y + h}});
}

void lettering(std::vector<glm::vec2> &vertices, const std::string &text,
               float x, float y, float size)
{
    for (char letter : text)
    {
        const auto rows = glyph(letter);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if (rows[row] & (1 << (4 - column)))
                    rectangle(vertices, x + column * size, y + row * size, size, size);
        x += 6.0f * size;
    }
}
} // namespace

ModeLabel::ModeLabel()
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    // The direction shader accepts vec3; a two-component attribute supplies z = 0.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    glBindVertexArray(0);
}

ModeLabel::~ModeLabel()
{
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void ModeLabel::Rebuild(const std::string &text, float pixelScale)
{
    text_ = text;
    pixelScale_ = pixelScale;
    std::vector<glm::vec2> vertices;
    const std::string hint = "SPACE: NEXT SHADER";
    const float width = 24.0f + std::max(text.size() * 12.0f, hint.size() * 9.0f);
    const auto beginBatch = [&](size_t batch, const glm::vec3 &color) {
        batches_[batch].first = static_cast<GLint>(vertices.size());
        batches_[batch].color = color;
    };
    const auto endBatch = [&](size_t batch) {
        batches_[batch].count =
            static_cast<GLsizei>(vertices.size()) - batches_[batch].first;
    };

    beginBatch(0, glm::vec3(0.24f, 0.37f, 0.44f));
    rectangle(vertices, 12, 12, width, 58);
    endBatch(0);
    beginBatch(1, glm::vec3(0.045f, 0.060f, 0.075f));
    rectangle(vertices, 13, 13, width - 2, 56);
    endBatch(1);
    beginBatch(2, glm::vec3(0.95f, 0.97f, 0.98f));
    lettering(vertices, text, 24, 24, 2.0f);
    endBatch(2);
    beginBatch(3, glm::vec3(0.63f, 0.72f, 0.78f));
    lettering(vertices, hint, 24, 48, 1.5f);
    endBatch(3);

    for (auto &vertex : vertices)
        vertex *= pixelScale;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec2)),
                 vertices.data(), GL_DYNAMIC_DRAW);
}

void ModeLabel::Draw(const Shader &shader, const std::string &text,
                     int width, int height, float pixelScale)
{
    pixelScale = std::clamp(pixelScale, 1.0f, 3.0f);
    if (text != text_ || pixelScale != pixelScale_)
        Rebuild(text, pixelScale);

    // Draw after the scene/paper pass, preserving the caller's drawing state.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLint previousProgram = 0, previousVAO = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    shader.use();
    shader.set("uModel", glm::mat4(1.0f));
    shader.set("uView", glm::mat4(1.0f));
    shader.set("uProjection",
               glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
    glBindVertexArray(vao_);
    for (const Batch &batch : batches_)
    {
        shader.set("uLineColor", batch.color);
        glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
    glBindVertexArray(static_cast<GLuint>(previousVAO));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (depthEnabled)
        glEnable(GL_DEPTH_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);
}
