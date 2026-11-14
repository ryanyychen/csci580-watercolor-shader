#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// Adapted from ../Raytracer/camera.h; GLM replaces the course vec types.
using Pixel = std::uint32_t;
Pixel Pixel_Color(const glm::vec3 &color);
glm::vec3 From_Pixel(Pixel color);

class Camera
{
  public:
    // World-space eye position and an orthonormal basis for the image plane.
    glm::vec3 position;
    glm::vec3 film_position;
    glm::vec3 look_vector;
    glm::vec3 vertical_vector;
    glm::vec3 horizontal_vector;
    // Film bounds/sizes are in film units; number_pixels is the raster resolution.
    glm::vec2 min{}, max{};
    glm::vec2 image_size{};
    glm::vec2 pixel_size{};
    glm::ivec2 number_pixels{};
    // Packed RGBA, bottom-row-first, matching the course camera.
    // vector owns storage so copying/resetting a camera is safe.
    std::vector<Pixel> colors;

    Camera();
    // Aim first, then choose focal distance, aspect ratio, and horizontal FOV in radians.
    void Position_And_Aim_Camera(const glm::vec3 &position_input, const glm::vec3 &look_at_point,
                                 const glm::vec3 &pseudo_up_vector);
    void Focus_Camera(float focal_distance, float aspect_ratio, float field_of_view);
    void Set_Resolution(const glm::ivec2 &number_pixels_input);
    // Map a bottom-left-based pixel index to its center on the film plane.
    glm::vec2 Cell_Center(const glm::ivec2 &index) const;
    glm::vec3 World_Position(const glm::ivec2 &pixel_index) const;
    void Set_Pixel(const glm::ivec2 &pixel_index, Pixel color);
    glm::mat4 View_Matrix() const;
    glm::mat4 Projection_Matrix() const;
    void Orbit(float dx, float dy);
    void Zoom(float steps);
    void Reset();
    // Capture the current default back buffer after Render() and before swapping.
    void Read_Framebuffer();

  private:
    // Retain a target for orbiting, independent of the film's focal distance.
    glm::vec3 look_at_point_{};
    float focal_distance_ = 1.0f;
    float aspect_ratio_ = 1.0f;
    float field_of_view_ = 0.785398163f; // radians, as in the course camera
    void Update_Film();
};
