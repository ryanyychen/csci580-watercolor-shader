// Draws Kubelka-Munk pigment swatches to screenshots/km/swatches.ppm for visual checks.

#include "renderer/Image_Output.h"
#include "renderer/KubelkaMunk.h"
#include <filesystem>
#include <iostream>
#include <vector>

int main()
{
    const std::vector<Pigment> &pigments = curtisPalette();
    const int width = 512;
    const int bandHeight = 40;
    const int height = bandHeight * static_cast<int>(pigments.size());
    const float maxThickness = 2.0f;
    // Paper reflects its own color and lets no light through
    const ReflectTransmit whitePaper = {glm::vec3(1.0f), glm::vec3(0.0f)};
    const ReflectTransmit blackPaper = {glm::vec3(0.0f), glm::vec3(0.0f)};

    // One band per pigment, thickness growing from left (0) to right (maxThickness)
    // Each band is painted over white paper with a black stripe across the middle, so transparent and opaque pigments look different
    std::vector<Pixel> pixels(width * height);
    for (int y = 0; y < height; y++)
    {
        // PPM rows go bottom-up, so put the first pigment at the top of the image
        int pigmentIndex = static_cast<int>(pigments.size()) - 1 - y / bandHeight;
        int rowInBand = y % bandHeight;
        bool overBlack = rowInBand >= bandHeight / 3 && rowInBand < 2 * bandHeight / 3;
        const ReflectTransmit &paper = overBlack ? blackPaper : whitePaper;
        for (int x = 0; x < width; x++)
        {
            float thickness = maxThickness * x / (width - 1);
            ReflectTransmit paint = computeLayer(pigments[pigmentIndex], thickness);
            ReflectTransmit painted = stackLayers(paint, paper);
            pixels[y * width + x] = Pixel_Color(painted.R);
        }
    }

    std::filesystem::create_directories("screenshots/km");
    Dump_ppm(pixels.data(), width, height, "screenshots/km/swatches.ppm");
    for (std::size_t i = 0; i < pigments.size(); i++)
        std::cout << "Band " << i + 1 << " (top to bottom): " << pigments[i].name << '\n';
    return 0;
}
