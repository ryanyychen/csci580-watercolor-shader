#pragma once
#include "Camera.h"
#include "Object.h"
#include <memory>
#include <vector>

struct Light
{
    // World-space point light, with constant intensity and no distance attenuation.
    glm::vec3 position{3.0f, 4.0f, 4.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

// Mirrors ../Raytracer/render_world.h, with automatic object ownership.
class Render_World
{
  public:
    Camera camera;
    std::vector<std::unique_ptr<Object>> objects;
    // Light count must match the maximum supported by the fragment shader.
    std::vector<Light> lights{Light{}};
    glm::vec3 ambient_color{1.0f};
    float ambient_intensity = 0.12f;
    glm::vec3 background_color{0.93f, 0.92f, 0.89f};
    // Diagnostic normal colors bypass lighting in the fragment shader.
    bool show_normals = false;

    // Current shader supports up to eight constant-intensity point lights.
    void Render(int width, int height);
};
