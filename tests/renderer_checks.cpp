#include "renderer/Camera.h"
#include "renderer/Image_Output.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
        std::cout << "Camera projection, orbit/zoom, and PPM orientation/channels passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
