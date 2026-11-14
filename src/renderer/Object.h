#pragma once
#include <glm/glm.hpp>
#include "Texture.h"

class Shader;

// Material names follow ../Raytracer/phong_shader.h.
struct Material
{
    // Ambient/diffuse colors tint the surface; specular color tints the highlight.
    // Higher specular_power produces a narrower Phong highlight.
    glm::vec3 color_ambient{0.22f, 0.48f, 0.65f};
    glm::vec3 color_diffuse{0.22f, 0.48f, 0.65f};
    glm::vec3 color_specular{0.5f};
    float specular_power = 32.0f;

    Texture * texture = nullptr;
    float noise_scale = 1.0f;
};

// Adapted from ../Raytracer/object.h for rasterized objects.
class Object
{
  public:
    Shader *material_shader = nullptr; // Non-owning; shader must outlive the object.
    int number_parts = 1;
    Material material;
    // Converts normalized mesh coordinates to world space for this scene instance.
    glm::mat4 model{1.0f};
    virtual ~Object() = default;
    // Geometry submission only; Render_World binds the shader and uniforms first.
    virtual void Draw() const = 0;
};
