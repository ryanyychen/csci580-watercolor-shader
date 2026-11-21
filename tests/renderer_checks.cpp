#include "renderer/Camera.h"
#include "renderer/Image_Output.h"
#include "renderer/TextureGenerator.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
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

int main()
{
    try
    {
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

        std::cout << "Camera, PPM, and LIC checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
