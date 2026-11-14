#include "Render_World.h"
#include "Shader.h"
#include <stdexcept>
#include <string>

void Render_World::Render(int width, int height)
{
    if (lights.size() > 8)
        throw std::runtime_error("Phong supports at most eight lights");

    camera.Set_Resolution(glm::ivec2(width, height));

    // Start a new frame: match the viewport to the framebuffer and reset visibility.
    glViewport(0, 0, width, height);
    glClearColor(background_color.r, background_color.g, background_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto &object : objects)
    {
        if (!object || !object->material_shader)
            throw std::runtime_error("Object needs a material shader");

        const Shader &shader = *object->material_shader;

        // Every object's program receives the same camera and world lighting.
        // The model matrix and material come from the individual object.
        shader.use();
        shader.set("uModel", object->model);
        shader.set("uView", camera.View_Matrix());
        shader.set("uProjection", camera.Projection_Matrix());
        shader.set("uCameraPosition", camera.position);
        shader.set("uShowNormals", show_normals ? 1 : 0);
        // Ambient is a scene-wide approximation of indirect illumination.
        shader.set("uAmbientColor", ambient_color);
        shader.set("uAmbientIntensity", ambient_intensity);
        shader.set("uColorAmbient", object->material.color_ambient);
        shader.set("uColorDiffuse", object->material.color_diffuse);
        shader.set("uColorSpecular", object->material.color_specular);
        shader.set("uSpecularPower", object->material.specular_power);
        shader.set("uLightCount", static_cast<int>(lights.size()));

        for (size_t i = 0; i < lights.size(); ++i)
        {
            // Uniform array names must match the arrays declared in phong.frag.
            const std::string suffix = "[" + std::to_string(i) + "]";
            shader.set(("uLightPosition" + suffix).c_str(), lights[i].position);
            shader.set(("uLightColor" + suffix).c_str(), lights[i].color * lights[i].intensity);
        }
        // The vertex shader now has transforms; the fragment shader has the material.
        object->Draw();
    }
}
