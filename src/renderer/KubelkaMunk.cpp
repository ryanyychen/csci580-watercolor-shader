#include "KubelkaMunk.h"
#include <cmath>

const std::vector<NamedPigment> &curtisPalette()
{
    // K and S values from Curtis et al. (1997)
    static const std::vector<NamedPigment> palette = {
        {"Quinacridone Rose", {{0.22f, 1.47f, 0.57f}, {0.05f, 0.003f, 0.03f}}},
        {"Indian Red", {{0.46f, 1.07f, 1.50f}, {1.28f, 0.38f, 0.21f}}},
        {"Cadmium Yellow", {{0.10f, 0.36f, 3.45f}, {0.97f, 0.65f, 0.007f}}},
        {"Hookers Green", {{1.62f, 0.61f, 1.64f}, {0.01f, 0.012f, 0.003f}}},
        {"Cerulean Blue", {{1.52f, 0.32f, 0.25f}, {0.06f, 0.26f, 0.40f}}},
        {"Burnt Umber", {{0.74f, 1.54f, 2.10f}, {0.09f, 0.09f, 0.004f}}},
        {"Cadmium Red", {{0.14f, 1.08f, 1.68f}, {0.77f, 0.015f, 0.018f}}},
        {"Brilliant Orange", {{0.13f, 0.81f, 3.45f}, {0.005f, 0.009f, 0.007f}}},
        {"Hansa Yellow", {{0.06f, 0.21f, 1.78f}, {0.50f, 0.88f, 0.009f}}},
        {"Phthalo Green", {{1.55f, 0.47f, 0.63f}, {0.01f, 0.05f, 0.035f}}},
        {"French Ultramarine", {{0.86f, 0.86f, 0.06f}, {0.005f, 0.005f, 0.09f}}},
        {"Interference Lilac", {{0.08f, 0.11f, 0.07f}, {1.25f, 0.42f, 1.43f}}},
    };
    return palette;
}

ReflectTransmit computeLayer(const Layer &layer)
{
    // Kubelka-Munk solution for one layer, computed separately for each color channel.
    // a, b, and c are intermediate terms in the formula for computing R and T of a single layer.
    ReflectTransmit result;
    
    for (int ch = 0; ch < 3; ch++)
    {
        float K = layer.pigment.K[ch];
        float S = layer.pigment.S[ch];
        float a = (S + K) / S;
        float b = std::sqrt(a * a - 1.0f);
        float bSx = b * S * layer.thickness;
        float c = a * std::sinh(bSx) + b * std::cosh(bSx);
        result.R[ch] = std::sinh(bSx) / c;
        result.T[ch] = b / c;
    }
    return result;
}

ReflectTransmit stackLayers(const ReflectTransmit &top, const ReflectTransmit &bottom)
{
    // R = Rtop + Ttop² × Rbottom / (1 − Rtop × Rbottom)
    // T = Ttop × Tbottom / (1 − Rtop × Rbottom)

    glm::vec3 bounce = 1.0f - top.R * bottom.R;
    ReflectTransmit result;
    result.R = top.R + top.T * top.T * bottom.R / bounce;
    result.T = top.T * bottom.T / bounce;
    return result;
}

glm::vec3 composite(const std::vector<Layer> &layers, const glm::vec3 &paperColor)
{
    // Start with bare paper: it reflects its own color and lets no light through
    ReflectTransmit stack;
    stack.R = paperColor;
    stack.T = glm::vec3(0.0f);

    // Put each layer of paint on top of everything below it
    for (const Layer &layer : layers)
        stack = stackLayers(computeLayer(layer), stack);

    // The final color is the reflectance of the whole stack
    return stack.R;
}
