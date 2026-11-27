#include "WatercolorSim.h"
#include "Image_Output.h"
#include "KubelkaMunk.h"
#include "TextureGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
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
    // same paper texture as the overlay in the stylistic method
    float **paper = createPaperTexture(width, height);
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
            paperHeight[j * width + i] = paper[j][i];
    }

    for (int y = 0; y < height; y++)
        free(paper[y]);
    free(paper);
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

std::vector<Pixel> WatercolorSim::imagePixels() const
{
    std::vector<Pixel> pixels(width * height);
    glm::vec3 paperColor(0.96f, 0.94f, 0.88f);
    Pigment pigment = pigmentFromName("French Ultramarine");

    for (int k = 0; k < width * height; k++)
    {
        glm::vec3 paper = paperColor * (0.88f + 0.12f * paperHeight[k]);
        if (wetMask[k] == 1)
            paper *= 0.95f;
        // one layer of pigment over the paper, thickness = floating + settled pigment
        float thickness = pigmentWater[k] + pigmentPaper[k];
        glm::vec3 color = colorFromLayers({{pigment, thickness}}, paper);
        pixels[k] = Pixel_Color(color);
    }

    return pixels;
}

void WatercolorSim::saveImage(const std::string &filename)
{
    const auto pixels = imagePixels();
    Dump_ppm(pixels.data(), width, height, filename);
}

void WatercolorSim::saveDebugImages(const std::string &folder)
{
    // paper height and wet mask in grayscale
    std::vector<Pixel> paper(width * height);
    std::vector<Pixel> wet(width * height);
    for (int k = 0; k < width * height; k++)
    {
        paper[k] = Pixel_Color(glm::vec3(paperHeight[k]));
        wet[k] = Pixel_Color(glm::vec3(static_cast<float>(wetMask[k])));
    }

    Dump_ppm(paper.data(), width, height, folder + "/paper.ppm");
    Dump_ppm(wet.data(), width, height, folder + "/wet.ppm");
}

float WatercolorSim::totalPigment()
{
    float total = 0.0f;
    for (int k = 0; k < width * height; k++)
        total += pigmentWater[k] + pigmentPaper[k];
    return total;
}

float WatercolorSim::uAt(int i, int j)
{
    if (i < 0 || i > width || j < 0 || j >= height)
        return 0.0f;
    return u[j * (width + 1) + i];
}

float WatercolorSim::vAt(int i, int j)
{
    if (i < 0 || i >= width || j < 0 || j > height)
        return 0.0f;
    return v[j * width + i];
}

float WatercolorSim::maxSpeed()
{
    float fastest = 0.0f;
    for (float value : u)
        fastest = std::max(fastest, std::fabs(value));
    for (float value : v)
        fastest = std::max(fastest, std::fabs(value));
    return fastest;
}

void WatercolorSim::enforceBoundaries()
{
    // water can only cross an edge if the cells on both sides are wet
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i <= width; i++)
        {
            if (i == 0 || i == width || wetMask[j * width + i - 1] == 0 ||
                wetMask[j * width + i] == 0)
                u[j * (width + 1) + i] = 0.0f;
        }
    }
    for (int j = 0; j <= height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            if (j == 0 || j == height || wetMask[(j - 1) * width + i] == 0 ||
                wetMask[j * width + i] == 0)
                v[j * width + i] = 0.0f;
        }
    }
}

void WatercolorSim::step()
{
    updateVelocities();
    relaxDivergence();
    flowOutward();
    movePigment();
    transferPigment();
}

void WatercolorSim::updateVelocities()
{
    const float mu = 0.1f;     // viscosity
    const float kappa = 0.01f; // viscous drag

    // water runs downhill on the paper
    for (int j = 0; j < height; j++)
    {
        for (int i = 1; i < width; i++)
            u[j * (width + 1) + i] -= paperHeight[j * width + i] - paperHeight[j * width + i - 1];
    }
    for (int j = 1; j < height; j++)
    {
        for (int i = 0; i < width; i++)
            v[j * width + i] -= paperHeight[j * width + i] - paperHeight[(j - 1) * width + i];
    }
    enforceBoundaries();

    // split the step so water never moves more than one cell at a time
    int substeps = std::max(1, static_cast<int>(std::ceil(maxSpeed())));
    float dt = 1.0f / substeps;

    for (int s = 0; s < substeps; s++)
    {
        std::vector<float> newU = u;
        std::vector<float> newV = v;

        // u on the edge between cell (i - 1, j) and cell (i, j)
        for (int j = 0; j < height; j++)
        {
            for (int i = 1; i < width; i++)
            {
                int left = j * width + i - 1;
                int right = j * width + i;
                if (wetMask[left] == 0 || wetMask[right] == 0)
                    continue;

                float uLeft = (uAt(i - 1, j) + uAt(i, j)) / 2.0f;
                float uRight = (uAt(i, j) + uAt(i + 1, j)) / 2.0f;
                float uvBottom = ((uAt(i, j - 1) + uAt(i, j)) / 2.0f) *
                                 ((vAt(i - 1, j) + vAt(i, j)) / 2.0f);
                float uvTop = ((uAt(i, j) + uAt(i, j + 1)) / 2.0f) *
                              ((vAt(i - 1, j + 1) + vAt(i, j + 1)) / 2.0f);
                float a = uLeft * uLeft - uRight * uRight + uvBottom - uvTop;
                float b = uAt(i + 1, j) + uAt(i - 1, j) + uAt(i, j + 1) + uAt(i, j - 1) -
                          4.0f * uAt(i, j);

                float uHere = uAt(i, j);
                newU[j * (width + 1) + i] =
                    uHere + dt * (a + mu * b + pressure[left] - pressure[right] - kappa * uHere);
            }
        }

        // v on the edge between cell (i, j - 1) and cell (i, j)
        for (int j = 1; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                int below = (j - 1) * width + i;
                int above = j * width + i;
                if (wetMask[below] == 0 || wetMask[above] == 0)
                    continue;

                float vBelow = (vAt(i, j - 1) + vAt(i, j)) / 2.0f;
                float vAbove = (vAt(i, j) + vAt(i, j + 1)) / 2.0f;
                float uvLeft = ((vAt(i - 1, j) + vAt(i, j)) / 2.0f) *
                               ((uAt(i, j - 1) + uAt(i, j)) / 2.0f);
                float uvRight = ((vAt(i, j) + vAt(i + 1, j)) / 2.0f) *
                                ((uAt(i + 1, j - 1) + uAt(i + 1, j)) / 2.0f);
                float a = vBelow * vBelow - vAbove * vAbove + uvLeft - uvRight;
                float b = vAt(i + 1, j) + vAt(i - 1, j) + vAt(i, j + 1) + vAt(i, j - 1) -
                          4.0f * vAt(i, j);

                float vHere = vAt(i, j);
                newV[j * width + i] =
                    vHere + dt * (a + mu * b + pressure[below] - pressure[above] - kappa * vHere);
            }
        }

        u = newU;
        v = newV;
        enforceBoundaries();
    }
}

void WatercolorSim::relaxDivergence()
{
    const int maxIterations = 50; // N
    const float tolerance = 0.01f; // tau
    const float xi = 0.1f;

    for (int t = 0; t < maxIterations; t++)
    {
        std::vector<float> newU = u;
        std::vector<float> newV = v;
        float biggest = 0.0f;

        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                int k = j * width + i;
                if (wetMask[k] == 0)
                    continue;

                // more water leaving than coming in: slow the outflow and lower the pressure
                float outflow = uAt(i + 1, j) - uAt(i, j) + vAt(i, j + 1) - vAt(i, j);
                float delta = -xi * outflow;
                pressure[k] += delta;
                newU[j * (width + 1) + i + 1] += delta;
                newU[j * (width + 1) + i] -= delta;
                newV[(j + 1) * width + i] += delta;
                newV[j * width + i] -= delta;
                biggest = std::max(biggest, std::fabs(delta));
            }
        }

        u = newU;
        v = newV;
        enforceBoundaries();
        if (biggest <= tolerance)
            break;
    }
}

std::vector<float> WatercolorSim::blurWetMask()
{
    // gaussian blur, about K = 10 cells wide, done on rows then on columns
    const int radius = 5;
    const float sigma = 2.5f;
    float weights[2 * radius + 1];
    float total = 0.0f;
    for (int d = -radius; d <= radius; d++)
    {
        weights[d + radius] = std::exp(-(d * d) / (2.0f * sigma * sigma));
        total += weights[d + radius];
    }
    for (int d = 0; d < 2 * radius + 1; d++)
        weights[d] /= total;

    std::vector<float> rows(width * height, 0.0f);
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            float sum = 0.0f;
            for (int d = -radius; d <= radius; d++)
            {
                int ni = i + d;
                if (ni >= 0 && ni < width)
                    sum += weights[d + radius] * wetMask[j * width + ni];
            }
            rows[j * width + i] = sum;
        }
    }

    std::vector<float> blurred(width * height, 0.0f);
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            float sum = 0.0f;
            for (int d = -radius; d <= radius; d++)
            {
                int nj = j + d;
                if (nj >= 0 && nj < height)
                    sum += weights[d + radius] * rows[nj * width + i];
            }
            blurred[j * width + i] = sum;
        }
    }
    return blurred;
}

void WatercolorSim::flowOutward()
{
    // lower the pressure near the edge of the wet area so water flows outward (edge darkening)
    const float eta = 0.02f; // the paper uses 0.01 to 0.05
    std::vector<float> blurred = blurWetMask();
    for (int k = 0; k < width * height; k++)
        pressure[k] -= eta * (1.0f - blurred[k]) * wetMask[k];
}

void WatercolorSim::movePigment()
{
    int substeps = std::max(1, static_cast<int>(std::ceil(maxSpeed())));
    float dt = 1.0f / substeps;

    for (int s = 0; s < substeps; s++)
    {
        std::vector<float> newG = pigmentWater;
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                int k = j * width + i;
                if (wetMask[k] == 0)
                    continue;

                // send pigment to each neighbor that water is flowing toward
                float g = pigmentWater[k];
                float toRight = dt * std::max(0.0f, uAt(i + 1, j)) * g;
                float toLeft = dt * std::max(0.0f, -uAt(i, j)) * g;
                float toTop = dt * std::max(0.0f, vAt(i, j + 1)) * g;
                float toBottom = dt * std::max(0.0f, -vAt(i, j)) * g;

                newG[k] -= toRight + toLeft + toTop + toBottom;
                if (i + 1 < width)
                    newG[k + 1] += toRight;
                if (i > 0)
                    newG[k - 1] += toLeft;
                if (j + 1 < height)
                    newG[k + width] += toTop;
                if (j > 0)
                    newG[k - width] += toBottom;
            }
        }
        pigmentWater = newG;
    }
}

void WatercolorSim::transferPigment()
{
    // pigment settles from the water onto the paper, and is lifted back into the water
    // values for French Ultramarine from Curtis et al. (1997)
    const float density = 0.01f;     // rho, how fast pigment settles and is lifted
    const float staining = 3.1f;     // omega, how much settled pigment resists being lifted
    const float granulation = 0.91f; // gamma, how much more pigment settles in the low parts of the paper

    for (int k = 0; k < width * height; k++)
    {
        if (wetMask[k] == 0)
            continue;

        float h = paperHeight[k];
        float down = pigmentWater[k] * (1.0f - h * granulation) * density;
        float up = pigmentPaper[k] * (1.0f + (h - 1.0f) * granulation) * density / staining;
        // neither layer can hold more than 1
        if (pigmentPaper[k] + down > 1.0f)
            down = std::max(0.0f, 1.0f - pigmentPaper[k]);
        if (pigmentWater[k] + up > 1.0f)
            up = std::max(0.0f, 1.0f - pigmentWater[k]);

        pigmentPaper[k] += down - up;
        pigmentWater[k] += up - down;
    }
}

void runWatercolorSimDemo()
{
    WatercolorSim sim(256, 256);
    sim.makePaper();
    sim.paintCircle(128.0f, 128.0f, 60.0f, 1.0f, 0.5f);

    std::cout << "Total pigment: " << sim.totalPigment() << '\n';
    sim.saveDebugImages("screenshots/sim");
    sim.saveImage("screenshots/sim/step000.ppm");

    for (int step = 1; step <= 100; step++)
    {
        sim.step();
        if (step % 20 == 0)
        {
            char filename[64];
            snprintf(filename, sizeof(filename), "screenshots/sim/step%03d.ppm", step);
            sim.saveImage(filename);
            std::cout << "Step " << step << ", total pigment: " << sim.totalPigment() << '\n';
        }
    }
}
