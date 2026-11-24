#pragma once
#include "Camera.h"
#include "Object.h"
#include <memory>
#include <vector>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

struct Light
{
    // World-space point light, with constant intensity and no distance attenuation.
    glm::vec3 position{3.0f, 4.0f, 4.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 look_at_point_ = glm::vec3(0,1,0); // set a position for the lights to orbit around the model

    void rotateLight(float dx, float dy) {
      const glm::vec3 offset = position - look_at_point_;
      const float distance = glm::length(offset);
      const float yaw = std::atan2(offset.x, offset.z) + glm::radians(dx * 0.3f);
      const float pitch = std::clamp(std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f)) +
                                       glm::radians(dy * 0.3f),
                                   glm::radians(-89.0f), glm::radians(89.0f));
      position = look_at_point_ + distance * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                                                  std::sin(pitch),
                                                                  std::cos(pitch) * std::cos(yaw));
    }
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
    bool show_vertex_attributes = false; // show normal and curvature directions per vertex

    // Current shader supports up to eight constant-intensity point lights.
    void Render(int width, int height);
};
