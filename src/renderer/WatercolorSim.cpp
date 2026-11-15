#include "WatercolorSim.h"
#include "Image_Output.h"
#include "TextureGenerator.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <glm/glm.hpp>

WatercolorSim::WatercolorSim(int w, int h)
{
    width = w;
    height = h;

    int size = width * height;
    paperHeight.assign(size, 0.0f);
    wetMask.assign(size, 0);
    pressure.assign(size, 0.0f);
    pigmentWater.assign(size, 0.0f);
    pigmentPaper.assign(size, 0.0f);
    u.assign((width + 1) * height, 0.0f);
    v.assign(width * (height + 1), 0.0f);
}

void WatercolorSim::makePaper()
{
    int noiseSize = std::max(width, height);
    generateGradientGrid();
    float **noise = createPerlinTexture(noiseSize);

    // perlin noise for the large bumps, white noise for the fiber grain
    std::vector<float> grain(width * height);
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            float random = static_cast<float>(rand()) / RAND_MAX;
            grain[j * width + i] = 0.6f * noise[j][i] + 0.4f * random;
        }
    }

    // 3x3 average blur to soften the grain
    float minValue = 1000.0f;
    float maxValue = -1000.0f;
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            float sum = 0.0f;
            int count = 0;
            for (int dj = -1; dj <= 1; dj++)
            {
                for (int di = -1; di <= 1; di++)
                {
                    int ni = i + di;
                    int nj = j + dj;
                    if (ni < 0 || ni >= width || nj < 0 || nj >= height)
                        continue;
                    sum += grain[nj * width + ni];
                    count++;
                }
            }
            float h = sum / count;
            paperHeight[j * width + i] = h;
            minValue = std::min(minValue, h);
            maxValue = std::max(maxValue, h);
        }
    }

    // rescale to [0,1], the paper needs 0 < h < 1
    for (float &h : paperHeight)
        h = (h - minValue) / (maxValue - minValue);

    for (int y = 0; y < noiseSize; y++)
        free(noise[y]);
    free(noise);
}

void WatercolorSim::paintCircle(float centerX, float centerY, float radius, float water,
                                float pigment)
{
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            float dx = i - centerX;
            float dy = j - centerY;
            if (dx * dx + dy * dy > radius * radius)
                continue;

            int k = j * width + i;
            wetMask[k] = 1;
            pressure[k] += water;
            pigmentWater[k] = std::min(1.0f, pigmentWater[k] + pigment);
        }
    }
}

void WatercolorSim::saveImage(const std::string &filename)
{
    std::vector<Pixel> pixels(width * height);
    glm::vec3 paperColor(0.96f, 0.94f, 0.88f);
    glm::vec3 pigmentColor(0.15f, 0.30f, 0.65f);

    for (int k = 0; k < width * height; k++)
    {
        glm::vec3 color = paperColor * (0.88f + 0.12f * paperHeight[k]);
        if (wetMask[k] == 1)
            color *= 0.95f;
        float amount = std::min(1.0f, pigmentWater[k] + pigmentPaper[k]);
        color *= glm::mix(glm::vec3(1.0f), pigmentColor, amount);
        pixels[k] = Pixel_Color(color);
    }

    Dump_ppm(pixels.data(), width, height, filename);
}

float WatercolorSim::totalPigment()
{
    float total = 0.0f;
    for (int k = 0; k < width * height; k++)
        total += pigmentWater[k] + pigmentPaper[k];
    return total;
}

void runWatercolorSimDemo()
{
    WatercolorSim sim(256, 256);
    sim.makePaper();
    sim.paintCircle(128.0f, 128.0f, 60.0f, 1.0f, 0.5f);

    std::cout << "Total pigment: " << sim.totalPigment() << '\n';
    sim.saveImage("screenshots/sim/step000.ppm");
}
