// Draws Kubelka-Munk test images to screenshots/km/ for visual checks

#include "renderer/Image_Output.h"
#include "renderer/KubelkaMunk.h"
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
const glm::vec3 white(1.0f);
const glm::vec3 black(0.0f);

// One band per pigment, thickness growing from left (0) to right (maxThickness)
// Each band is painted over white paper with a black stripe across the middle,
// so transparent and opaque pigments look different
void writePigmentChart(const std::vector<NamedPigment> &pigments)
{
    const int width = 512;
    const int bandHeight = 40;
    const int pigmentCount = static_cast<int>(pigments.size());
    const int height = bandHeight * pigmentCount;
    const float maxThickness = 2.0f;

    std::vector<Pixel> pixels(width * height);
    for (int y = 0; y < height; y++)
    {
        // PPM rows go bottom-up, so put the first pigment at the top of the image
        int pigmentIndex = pigmentCount - 1 - y / bandHeight;
        int rowInBand = y % bandHeight;
        bool overBlack = rowInBand >= bandHeight / 3 && rowInBand < 2 * bandHeight / 3;
        for (int x = 0; x < width; x++)
        {
            float thickness = maxThickness * x / (width - 1);
            glm::vec3 color =
                composite({{pigments[pigmentIndex].pigment, thickness}}, overBlack ? black : white);
            pixels[y * width + x] = Pixel_Color(color);
        }
    }

    Dump_ppm(pixels.data(), width, height, "screenshots/km/pigments.ppm");
    for (int i = 0; i < pigmentCount; i++)
        std::cout << "Band " << i + 1 << " (top to bottom): " << pigments[i].name << '\n';
}

// A vertical band of the bottom pigment crossed by a horizontal band of the top pigment
// Where they overlap, the top layer is glazed over the bottom one
void writeGlazing(const Pigment &bottom, const Pigment &top)
{
    const int size = 256;
    const float thickness = 0.5f;

    std::vector<Pixel> pixels(size * size);
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            std::vector<Layer> layers;
            if (x >= 40 && x < 160)
                layers.push_back({bottom, thickness});
            if (y >= 96 && y < 216)
                layers.push_back({top, thickness});
            pixels[y * size + x] = Pixel_Color(composite(layers, white));
        }
    }

    Dump_ppm(pixels.data(), size, size, "screenshots/km/glazing.ppm");
}
} // namespace

int main()
{
    const std::vector<NamedPigment> &pigments = curtisPalette();
    std::filesystem::create_directories("screenshots/km");
    writePigmentChart(pigments);
    writeGlazing(pigments[10].pigment, pigments[8].pigment); // French Ultramarine under Hansa Yellow
    return 0;
}
