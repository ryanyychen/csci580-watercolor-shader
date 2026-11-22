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
        shader.set("uNoiseScale", object->material.noise_scale);
        shader.set("uLICBias", object->material.licBias);
        shader.set("uLICGain", object->material.licGain);
        shader.set("uUsePigmentLookup", object->material.pigmentLookup ? 1 : 0);
        shader.set("uPigmentMaxThickness", object->material.pigmentLookupMaxThickness);
        shader.set("uPaintThickness", object->material.watercolorThickness);
        shader.set("uPigmentNoiseStrength", object->material.watercolorNoiseStrength);
        shader.set("uUseMeshTextureCoordinates",
                   object->material.useMeshTextureCoordinates ? 1 : 0);
        shader.set("uUseTriplanarNoise",
                   object->material.useTriplanarNoise ? 1 : 0);
        const size_t directionIndex =
            static_cast<size_t>(object->material.principalDirection);
        Texture *licTexture = object->material.licTextures[directionIndex];
        const auto &triplanarLICTextures =
            object->material.triplanarLICTextures[directionIndex];
        shader.set("uUseLIC",
                   object->material.useLIC &&
                           (licTexture ||
                            object->material.useTriplanarNoise)
                       ? 1
                       : 0);
        shader.set("uLightCount", static_cast<int>(lights.size()));

        constexpr int noiseTextureUnit = 0;
        if (object->material.texture)
        {
            object->material.texture->Bind(GL_TEXTURE0 + noiseTextureUnit);
            shader.set("noiseTexture", noiseTextureUnit);
        }
        constexpr int licTextureUnit = 1;
        if (licTexture)
        {
            licTexture->Bind(GL_TEXTURE0 + licTextureUnit);
            shader.set("licTexture", licTextureUnit);
        }
        constexpr const char *triplanarLICUniforms[6] = {
            "licTextureXPositive", "licTextureXNegative",
            "licTextureYPositive", "licTextureYNegative",
            "licTextureZPositive", "licTextureZNegative"};
        for (int i = 0; i < 6; ++i)
        {
            Texture *texture = triplanarLICTextures[i];
            if (!texture)
                continue;
            const int textureUnit = 2 + i;
            texture->Bind(GL_TEXTURE0 + textureUnit);
            shader.set(triplanarLICUniforms[i], textureUnit);
        }

        // Units 0-7 are reserved for Perlin/LIC. A 1024x1 RGB texture works on
        // the existing desktop GL 3.3 path without a separate 1D-texture API.
        if (object->material.pigmentLookup)
        {
            constexpr int pigmentTextureUnit = 8;
            object->material.pigmentLookup->Bind(GL_TEXTURE0 + pigmentTextureUnit);
            shader.set("pigmentLookup", pigmentTextureUnit);
        }

        for (size_t i = 0; i < lights.size(); ++i)
        {
            // Uniform array names must match the arrays declared in phong.frag.
            const std::string suffix = "[" + std::to_string(i) + "]";
            shader.set(("uLightPosition" + suffix).c_str(), lights[i].position);
            shader.set(("uLightColor" + suffix).c_str(), lights[i].color * lights[i].intensity);
        }
        // The vertex shader now has transforms; the fragment shader has the material.
        object->Draw();
        if (show_vertex_attributes) {
            if (!object->direction_shader)
                throw std::runtime_error("Object needs a direction-overlay shader");
            const Shader &directionShader = *object->direction_shader;
            directionShader.use();
            directionShader.set("uModel", object->model);
            directionShader.set("uView", camera.View_Matrix());
            directionShader.set("uProjection", camera.Projection_Matrix());
            directionShader.set("uLineColor", glm::vec3(0.0f));
            if (show_normals)
                object->DrawVertexNormals();
            object->DrawCurvatureDirections();
        }
    }
}
