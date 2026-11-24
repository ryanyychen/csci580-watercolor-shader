#include "renderer/Camera.h"
#include "renderer/Image_Output.h"
#include "renderer/TextureGenerator.h"
#include "renderer/PigmentLookup.h"
#include "renderer/SceneImporter.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Unlike assert(), these checks stay active in optimized/release builds.
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void checkSceneMaterialImport()
{
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory =
        std::filesystem::temp_directory_path() / ("watercolor-import-check-" + suffix);
    require(std::filesystem::create_directory(directory),
            "Could not create temporary scene-import directory");
    struct DirectoryCleanup
    {
        std::filesystem::path path;
        ~DirectoryCleanup()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{directory};

    const auto objPath = directory / "material.obj";
    const auto mtlPath = directory / "material.mtl";
    {
        std::ofstream obj(objPath);
        obj << "mtllib material.mtl\n"
               "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "v 2 0 0\n"
               "v 3 0 0\n"
               "v 2 1 0\n"
               "vt 0 0\n"
               "vt 1 0\n"
               "vt 0 1\n"
               "vn 0 0 1\n"
               "usemtl testMaterial\n"
               "f 1/1/1 2/2/1 3/3/1\n"
               "usemtl laterMaterial\n"
               "f 4/1/1 5/2/1 6/3/1\n";
        require(static_cast<bool>(obj), "Could not write temporary OBJ");
    }
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl testMaterial\n"
               "Ka 0.1 0.2 0.3\n"
               "Kd 0.8 0.1 0.2\n"
               "Ks 0.4 0.5 0.6\n"
               "Ns 24\n"
               "newmtl laterMaterial\n"
               "Ka 0 1 0\n"
               "Kd 0 1 0\n"
               "Ks 0 1 0\n"
               "Ns 4\n";
        require(static_cast<bool>(mtl), "Could not write temporary MTL");
    }

    const ImportedSceneData imported = loadSceneFiles({objPath});
    require(imported.material.has_value(), "Assigned OBJ material was not imported");
    require(imported.triangles.size() == 2, "OBJ triangles were not imported");
    require(glm::length(imported.material->ambient - glm::vec3(0.1f, 0.2f, 0.3f)) < 1e-6f,
            "OBJ ambient material color was not imported");
    require(glm::length(imported.material->diffuse - glm::vec3(0.8f, 0.1f, 0.2f)) < 1e-6f,
            "OBJ diffuse material color was not imported");
    require(glm::length(imported.material->specular - glm::vec3(0.4f, 0.5f, 0.6f)) < 1e-6f,
            "OBJ specular material color was not imported");
    require(std::abs(imported.material->shininess - 24.0f) < 1e-6f,
            "OBJ material shininess was not imported");

    const auto plainObjPath = directory / "plain.obj";
    {
        std::ofstream obj(plainObjPath);
        obj << "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "f 1 2 3\n";
        require(static_cast<bool>(obj), "Could not write temporary OBJ without a material");
    }
    const ImportedSceneData withoutMaterial = loadSceneFiles({plainObjPath});
    require(!withoutMaterial.material.has_value(),
            "OBJ without an assigned material should use the renderer fallback");
}

void checkFbxMaterialImport()
{
    std::filesystem::path directory = std::filesystem::current_path();
    std::filesystem::path fbxPath;
    while (!directory.empty())
    {
        const auto candidate = directory / "assets/models/sampleScene.fbx";
        if (std::filesystem::exists(candidate))
        {
            fbxPath = candidate;
            break;
        }
        const auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    require(!fbxPath.empty(), "Could not locate sampleScene.fbx");

    const ImportedSceneData imported = loadSceneFiles({fbxPath});
    require(!imported.triangles.empty(), "FBX triangles were not imported");
    require(imported.material.has_value(), "FBX material was not imported");
    require(glm::length(imported.material->diffuse - glm::vec3(0.8f, 0.0f, 0.0f)) < 1e-5f,
            "The first FBX material was not selected");
}

int main()
{
    try
    {
        checkSceneMaterialImport();
        checkFbxMaterialImport();
        Camera camera;
        camera.Position_And_Aim_Camera(glm::vec3(0, 0, 5), glm::vec3(0), glm::vec3(0, 1, 0));
        camera.Focus_Camera(1.0f, 2.0f, glm::half_pi<float>());
        camera.Set_Resolution(glm::ivec2(4, 2));
        // Film pixel centers must project to the corresponding raster centers.
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 4; ++x)
            {
                glm::vec4 clip = camera.Projection_Matrix() * camera.View_Matrix() *
                                 glm::vec4(camera.World_Position(glm::ivec2(x, y)), 1.0f);
                require(std::abs(clip.x / clip.w - (2.0f * (x + 0.5f) / 4 - 1)) < 1e-5f,
                        "Film/raster x mismatch");
                require(std::abs(clip.y / clip.w - (2.0f * (y + 0.5f) / 2 - 1)) < 1e-5f,
                        "Film/raster y mismatch");
            }
        // Moving the camera should retain its aim and an orthogonal image-plane basis.
        camera.Orbit(40, 15);
        camera.Zoom(3);
        require(glm::length(glm::normalize(-camera.position) - camera.look_vector) < 1e-5f,
                "Orbit/zoom lost target");
        require(std::abs(glm::dot(camera.look_vector, camera.vertical_vector)) < 1e-5f,
                "Camera basis not orthogonal");
        camera.Set_Resolution(glm::ivec2(2, 2));
        camera.Set_Pixel(glm::ivec2(0, 0), Pixel_Color(glm::vec3(1, 0, 0))); // bottom left
        camera.Set_Pixel(glm::ivec2(1, 0), Pixel_Color(glm::vec3(0, 1, 0)));
        camera.Set_Pixel(glm::ivec2(0, 1), Pixel_Color(glm::vec3(0, 0, 1))); // top left
        camera.Set_Pixel(glm::ivec2(1, 1), Pixel_Color(glm::vec3(1, 1, 1)));
        const auto filename = std::filesystem::current_path() / "renderer-check.ppm";
        Dump_ppm(camera.colors.data(), 2, 2, filename);
        std::ifstream file(filename);
        std::string magic;
        int width, height, maximum;
        file >> magic >> width >> height >> maximum;
        require(magic == "P3" && width == 2 && height == 2 && maximum == 255, "Invalid PPM header");
        // PPM starts at the top: blue/white must precede the bottom red/green row.
        const int expected[] = {0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 0};
        for (const int value : expected)
        {
            int actual = -1;
            require(static_cast<bool>(file >> actual) && actual == value,
                    "PPM row order or channel mismatch");
        }
        std::string extra;
        require(!(file >> extra), "Unexpected PPM pixels");
        file.close();
        std::filesystem::remove(filename);
        resizeGradientGrid(64);
        float **perlin = createPerlinTexture(8);
        require(perlin != nullptr, "Perlin texture allocation failed after resizing gradient grid");
        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                require(std::isfinite(perlin[y][x]) &&
                            perlin[y][x] >= 0.0f && perlin[y][x] <= 1.0f,
                        "Perlin texture is invalid after resizing gradient grid");
            }
        }
        float **customPerlin = createPerlinTexture(8, 2, 0.25f, 1.5f);
        require(customPerlin != nullptr, "Custom Perlin texture allocation failed");
        bool customParametersChangedOutput = false;
        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                customParametersChangedOutput =
                    customParametersChangedOutput ||
                    std::abs(customPerlin[y][x] - perlin[y][x]) > 1e-6f;
            }
            std::free(perlin[y]);
            std::free(customPerlin[y]);
        }
        std::free(perlin);
        std::free(customPerlin);
        require(customParametersChangedOutput, "Custom Perlin parameters did not affect the output");

        bool undersizedGridRejected = false;
        try
        {
            resizeGradientGrid(48);
        }
        catch (const std::invalid_argument &)
        {
            undersizedGridRejected = true;
        }
        require(undersizedGridRejected, "Gradient grid accepted a size too small for Perlin noise");
        bool insufficientOctaveGridRejected = false;
        try
        {
            createPerlinTexture(8, 4, 0.5f, 3.0f);
        }
        catch (const std::invalid_argument &)
        {
            insufficientOctaveGridRejected = true;
        }
        require(insufficientOctaveGridRejected,
                "Perlin texture accepted octave parameters that exceed the gradient grid");

        const int textureSize = 33;
        std::vector<float> stripeNoise(static_cast<size_t>(textureSize) * textureSize);
        std::vector<glm::vec2> horizontalField(stripeNoise.size(), glm::vec2(1.0f, 0.0f));
        for (int y = 0; y < textureSize; ++y)
            for (int x = 0; x < textureSize; ++x)
                stripeNoise[static_cast<size_t>(y) * textureSize + x] =
                    0.5f + 0.5f * std::sin(2.0f * glm::pi<float>() * x / 8.0f);

        const auto lic = createLineIntegralConvolutionTexture(
            textureSize, stripeNoise, horizontalField);
        float sourceVariation = 0.0f;
        float licVariation = 0.0f;
        for (int x = 8; x < 24; ++x)
        {
            const size_t index = static_cast<size_t>(textureSize / 2) * textureSize + x;
            sourceVariation += std::abs(
                stripeNoise[index] - stripeNoise[index - 1]);
            licVariation += std::abs(lic[index] - lic[index - 1]);
        }
        require(licVariation < sourceVariation * 0.25f,
                "LIC did not smooth noise along the vector field");

        std::vector<std::vector<float>> sourceRows(
            textureSize, std::vector<float>(textureSize));
        std::vector<float *> sourceRowPointers(textureSize);
        for (int y = 0; y < textureSize; ++y)
        {
            sourceRowPointers[y] = sourceRows[y].data();
            for (int x = 0; x < textureSize; ++x)
                sourceRows[y][x] = stripeNoise[static_cast<size_t>(y) * textureSize + x];
        }
        float **licRows = createLineIntegralConvolutionTexture(
            textureSize, sourceRowPointers.data(), horizontalField);
        require(licRows != nullptr, "LIC row texture allocation failed");
        for (int y = 0; y < textureSize; ++y)
            for (int x = 0; x < textureSize; ++x)
                require(std::abs(licRows[y][x] -
                                 lic[static_cast<size_t>(y) * textureSize + x]) < 1e-6f,
                        "LIC row texture differs from vector output");
        for (int y = 0; y < textureSize; ++y)
            std::free(licRows[y]);
        std::free(licRows);

        std::fill(horizontalField.begin(), horizontalField.end(), glm::vec2(0.0f));
        const auto unchanged = createLineIntegralConvolutionTexture(
            textureSize, stripeNoise, horizontalField);
        require(unchanged == stripeNoise, "LIC changed noise outside the direction field");

        const glm::vec3 paperColor(0.96f, 0.94f, 0.88f);
        const MeasuredPigment blue = measuredPigmentFromName("Ultramarine Blue");
        const MeasuredPigment yellow = measuredPigmentFromName("Hansa Yellow Opaque");
        const MeasuredPigment mixture = measuredLayerFromMix({{blue, 0.7f}, {yellow, 0.3f}}).pigment;
        constexpr int lookupSize = 1024;
        constexpr float maximumThickness = 3.0f;
        for (const MeasuredPigment &pigment : {blue, mixture})
        {
            const auto lookup = createMeasuredPigmentLookup(
                pigment, paperColor, lookupSize, maximumThickness);
            require(lookup.size() == lookupSize, "Wrong pigment lookup size");
            require(glm::length(lookup.front() - paperColor) < 1e-5f,
                    "Zero-thickness lookup sample must return paper");
            const auto thick = colorFromMeasuredLayers({{pigment, maximumThickness}}, paperColor);
            require(glm::length(lookup.back() - thick) < 1e-6f,
                    "Final lookup sample does not match spectral KM");

            // Emulate the shader's texel-center sampling and linear interpolation.
            // Non-grid thicknesses must remain close to the direct 36-band result.
            for (float fraction : {0.0001f, 0.073f, 0.437f, 0.913f})
            {
                const float sample = fraction * (lookupSize - 1);
                const auto left = static_cast<size_t>(sample);
                const auto interpolated = glm::mix(lookup[left], lookup[left + 1], sample - left);
                const auto direct = colorFromMeasuredLayers(
                    {{pigment, fraction * maximumThickness}}, paperColor);
                require(glm::length(interpolated - direct) < 1e-3f,
                        "Pigment lookup interpolation differs from spectral KM");
            }
            for (const auto &color : lookup)
                for (int channel = 0; channel < 3; ++channel)
                    require(std::isfinite(color[channel]), "Non-finite pigment lookup color");
        }
        bool invalidLookupRejected = false;
        try
        {
            createMeasuredPigmentLookup(blue, paperColor, 1, maximumThickness);
        }
        catch (const std::invalid_argument &)
        {
            invalidLookupRejected = true;
        }
        require(invalidLookupRejected, "Pigment lookup accepted fewer than two samples");

        std::cout << "Scene import, camera, PPM, LIC, and spectral pigment lookup checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
