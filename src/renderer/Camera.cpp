#include "Camera.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

Pixel Pixel_Color(const glm::vec3 &color)
{
    // Store channels as 0xRRGGBBAA
    const auto c = glm::uvec3(glm::clamp(color, 0.0f, 1.0f) * 255.0f);
    return (c.r << 24) | (c.g << 16) | (c.b << 8) | 0xff;
}

glm::vec3 From_Pixel(Pixel color)
{
    return glm::vec3(color >> 24, (color >> 16) & 0xff, (color >> 8) & 0xff) / 255.0f;
}

Camera::Camera()
{
    Reset();
}

void Camera::Reset()
{
    const float yaw = glm::radians(30.0f), pitch = glm::radians(15.0f);
    Position_And_Aim_Camera(4.5f * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                             std::cos(pitch) * std::cos(yaw)),
                            glm::vec3(0), glm::vec3(0, 1, 0));
    Focus_Camera(1.0f, aspect_ratio_, glm::radians(45.0f));
}

void Camera::Position_And_Aim_Camera(const glm::vec3 &position_input,
                                     const glm::vec3 &look_at_point,
                                     const glm::vec3 &pseudo_up_vector)
{
    // Cross products construct right and corrected up vectors perpendicular to look.
    // Parallel look/up vectors cannot define a valid camera basis.
    const glm::vec3 direction = look_at_point - position_input;
    if (!(glm::length(direction) > 1e-6f) ||
        !(glm::length(glm::cross(direction, pseudo_up_vector)) > 1e-6f))
        throw std::runtime_error(
            "Camera needs distinct position/target and a non-parallel up vector");
    position = position_input;
    look_at_point_ = look_at_point;
    look_vector = glm::normalize(direction);
    horizontal_vector = glm::normalize(glm::cross(look_vector, pseudo_up_vector));
    vertical_vector = glm::normalize(glm::cross(horizontal_vector, look_vector));
    Update_Film();
}

void Camera::Focus_Camera(float focal_distance, float aspect_ratio, float field_of_view)
{
    if (!(focal_distance > 0) || !(aspect_ratio > 0) ||
        !(field_of_view > 0 && field_of_view < glm::radians(179.0f)))
        throw std::runtime_error("Invalid camera focus parameters");
    focal_distance_ = focal_distance;
    aspect_ratio_ = aspect_ratio;
    field_of_view_ = field_of_view;
    Update_Film();
}

void Camera::Update_Film()
{
    // Similar triangles: half the film width is focal_distance * tan(FOV / 2).
    // This film describes projection geometry, not photographic depth of field.
    film_position = position + look_vector * focal_distance_;
    const float width = 2.0f * focal_distance_ * std::tan(0.5f * field_of_view_);
    image_size = glm::vec2(width, width / aspect_ratio_);
    min = -0.5f * image_size;
    max = 0.5f * image_size;
    if (number_pixels.x > 0 && number_pixels.y > 0)
        pixel_size = image_size / glm::vec2(number_pixels);
}

void Camera::Set_Resolution(const glm::ivec2 &resolution)
{
    if (resolution.x <= 0 || resolution.y <= 0)
        throw std::runtime_error("Invalid camera resolution");
    if (resolution != number_pixels)
    {
        // Allocate screenshot storage only when the framebuffer dimensions change.
        colors.resize(static_cast<size_t>(resolution.x) * static_cast<size_t>(resolution.y));
        number_pixels = resolution;
    }
    aspect_ratio_ = static_cast<float>(resolution.x) / resolution.y;
    Update_Film();
}

glm::vec2 Camera::Cell_Center(const glm::ivec2 &index) const
{
    // The half-pixel offset samples pixel centers rather than shared pixel corners.
    return min + (glm::vec2(index) + glm::vec2(0.5f)) * pixel_size;
}

glm::vec3 Camera::World_Position(const glm::ivec2 &index) const
{
    const auto cell = Cell_Center(index);
    return film_position + horizontal_vector * cell.x + vertical_vector * cell.y;
}

void Camera::Set_Pixel(const glm::ivec2 &index, Pixel color)
{
    colors.at(static_cast<size_t>(index.y) * number_pixels.x + index.x) = color;
}

glm::mat4 Camera::View_Matrix() const
{
    return glm::lookAt(position, position + look_vector, vertical_vector);
}

glm::mat4 Camera::Projection_Matrix() const
{
    // Focus_Camera's FOV is horizontal, matching Raytracer's film coordinates.
    // Scale the film bounds onto the near plane while preserving its viewing angles.
    const float nearPlane = 0.1f;
    const float scale = nearPlane / focal_distance_;
    return glm::frustum(min.x * scale, max.x * scale, min.y * scale, max.y * scale, nearPlane,
                        100.0f);
}

void Camera::Orbit(float dx, float dy)
{
    // Convert the eye-to-target offset to spherical angles; preserve its radius.
    // Clamp pitch away from the poles so the look and world-up vectors stay distinct.
    const glm::vec3 offset = position - look_at_point_;
    const float distance = glm::length(offset);
    const float yaw = std::atan2(offset.x, offset.z) + glm::radians(dx * 0.3f);
    const float pitch = std::clamp(std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f)) +
                                       glm::radians(dy * 0.3f),
                                   glm::radians(-89.0f), glm::radians(89.0f));
    Position_And_Aim_Camera(look_at_point_ + distance * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                                                  std::sin(pitch),
                                                                  std::cos(pitch) * std::cos(yaw)),
                            look_at_point_, glm::vec3(0, 1, 0));
}

void Camera::Zoom(float steps)
{
    // Multiplicative distance changes give consistent wheel sensitivity at any zoom.
    const glm::vec3 offset = position - look_at_point_;
    const float distance = std::clamp(glm::length(offset) * std::exp(-steps * 0.1f), 2.0f, 20.0f);
    Position_And_Aim_Camera(look_at_point_ + glm::normalize(offset) * distance, look_at_point_,
                            glm::vec3(0, 1, 0));
}

void Camera::Read_Framebuffer()
{
    if (colors.empty())
        throw std::runtime_error("Set camera resolution before capturing an image");
    std::vector<unsigned char> rgba(colors.size() * 4);
    GLint previousReadBuffer, previousAlignment;
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);
    // Read the just-rendered image, not the previously presented front buffer.
    // Restore readback state so taking a screenshot does not affect later rendering.
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, number_pixels.x, number_pixels.y, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Framebuffer capture failed");
    // Pack explicit bytes instead of reinterpreting them as native-endian integers.
    for (size_t i = 0; i < colors.size(); ++i)
    {
        colors[i] = (Pixel(rgba[4 * i]) << 24) | (Pixel(rgba[4 * i + 1]) << 16) |
                    (Pixel(rgba[4 * i + 2]) << 8) | Pixel(rgba[4 * i + 3]);
    }
}
