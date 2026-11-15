#include "KubelkaMunk.h"
#include <cmath>
#include <stdexcept>

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

Pigment pigmentFromName(const std::string &name)
{
    for (const NamedPigment &entry : curtisPalette())
        if (entry.name == name)
            return entry.pigment;
    throw std::invalid_argument("Unknown pigment: " + name);
}

Pigment pigmentFromColors(const glm::vec3 &onWhite, const glm::vec3 &onBlack)
{
    // Computes K and S from Rw (color over white) and Rb (color over black), for each color channel
    // a = (Rw + (Rb - Rw + 1) / Rb) / 2
    // b = sqrt(a^2 - 1)
    // S = arccoth((b^2 - (a - Rw)(a - 1)) / (b (1 - Rw))) / b
    // K = S (a - 1)
    Pigment pigment;
    for (int ch = 0; ch < 3; ch++)
    {
        float Rw = onWhite[ch];
        float Rb = onBlack[ch];
        if (!(0.0f < Rb && Rb < Rw && Rw < 1.0f))
            throw std::invalid_argument("pigmentFromColors needs 0 < onBlack < onWhite < 1");
        float a = 0.5f * (Rw + (Rb - Rw + 1.0f) / Rb);
        float b = std::sqrt(a * a - 1.0f);
        float x = (b * b - (a - Rw) * (a - 1.0f)) / (b * (1.0f - Rw));
        float arccothX = 0.5f * std::log((x + 1.0f) / (x - 1.0f));
        pigment.S[ch] = arccothX / b;
        pigment.K[ch] = pigment.S[ch] * (a - 1.0f);
    }
    return pigment;
}

Pigment pigmentFromRGB(const glm::vec3 &color, float opacity)
{
    // onWhite = color, onBlack = color * opacity
    // Values are clamped to 0.01 to 0.99 so that 0 < onBlack < onWhite < 1 always holds
    glm::vec3 onWhite = glm::clamp(color, 0.01f, 0.99f);
    glm::vec3 onBlack = onWhite * glm::clamp(opacity, 0.01f, 0.99f);
    return pigmentFromColors(onWhite, onBlack);
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
        // R = sinh(bSx) / c, rewritten with tanh because sinh and cosh overflow for thick layers
        float t = std::tanh(bSx);
        result.R[ch] = t / (a * t + b);
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

Layer layerFromMix(const std::vector<Layer> &parts)
{
    // K and S are averages weighted by each part's thickness, and thickness is the sum of the parts
    if (parts.empty())
        throw std::invalid_argument("layerFromMix needs at least one part");
    Layer mixed = {{glm::vec3(0.0f), glm::vec3(0.0f)}, 0.0f};
    for (const Layer &part : parts)
    {
        mixed.pigment.K += part.pigment.K * part.thickness;
        mixed.pigment.S += part.pigment.S * part.thickness;
        mixed.thickness += part.thickness;
    }

    if (mixed.thickness <= 0.0f)
        return {parts[0].pigment, 0.0f};

    mixed.pigment.K /= mixed.thickness;
    mixed.pigment.S /= mixed.thickness;
    return mixed;
}

glm::vec3 colorFromLayers(const std::vector<Layer> &layers, const glm::vec3 &paperColor)
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
