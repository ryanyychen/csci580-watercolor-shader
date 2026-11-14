#include "Image_Output.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void Dump_ppm(const Pixel *data, int width, int height, const std::filesystem::path &filename)
{
    if (!data || width <= 0 || height <= 0)
        throw std::runtime_error("Invalid PPM image");
    if (!filename.parent_path().empty())
        std::filesystem::create_directories(filename.parent_path());
    std::ofstream file(filename, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open image for writing: " + filename.string());

    // P3 stores decimal RGB values; alpha is discarded by this output format.
    file << "P3\n" << width << " " << height << "\n255\n";
    // Course pixel buffers and OpenGL start at the bottom; PPM starts at the top.
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const Pixel pixel = data[static_cast<size_t>(height - y - 1) * width + x];
            file << ((pixel >> 24) & 0xff) << " " << ((pixel >> 16) & 0xff) << " "
                 << ((pixel >> 8) & 0xff) << "\n";
        }
    }
    // Closing can reveal buffered write failures, such as a full disk.
    file.close();
    if (!file)
        throw std::runtime_error("Failed while writing image: " + filename.string());
    std::cout << "Dumping output to " << filename << '\n';
}

void saveScreenshot(Camera &camera, const std::filesystem::path &filename)
{
    // Called after drawing and before swapping, as in the terrain readback path.
    camera.Read_Framebuffer();
    Dump_ppm(camera.colors.data(), camera.number_pixels.x, camera.number_pixels.y, filename);
}
