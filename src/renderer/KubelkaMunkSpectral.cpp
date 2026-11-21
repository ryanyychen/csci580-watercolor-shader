#include "KubelkaMunkSpectral.h"
#include <cmath>
#include <stdexcept>

namespace
{

// Each row turns the 10 band values into one RGB channel
// Built from the CIE 1931 2-degree colour-matching functions under an equal-energy light, converted to linear sRGB
// A fitted 3x3 correction is folded in so that spectrumFromRGB followed by rgbFromSpectrum returns the input color
const float spectrumToRGBMatrix[3][spectralBands] = {
    {0.001485f, 0.033775f, 0.006818f, -0.183958f, -0.234957f, 0.272668f, 0.697786f, 0.347780f, 0.053937f, 0.004617f},
    {-0.001393f, -0.034645f, -0.028647f, 0.191227f, 0.488554f, 0.374388f, 0.047664f, -0.029741f, -0.006234f, -0.000556f},
    {0.011756f, 0.325037f, 0.670115f, 0.189462f, -0.081215f, -0.090445f, -0.025098f, -0.000232f, 0.000337f, 0.000034f},
};

// Basis spectra from Smits (1999)
const Spectrum white = {1.0000f, 1.0000f, 0.9999f, 0.9993f, 0.9992f, 0.9998f, 1.0000f, 1.0000f, 1.0000f, 1.0000f};
const Spectrum cyan = {0.9710f, 0.9426f, 1.0007f, 1.0007f, 1.0007f, 1.0007f, 0.1564f, 0.0000f, 0.0000f, 0.0000f};
const Spectrum magenta = {1.0000f, 1.0000f, 0.9685f, 0.2229f, 0.0000f, 0.0458f, 0.8369f, 1.0000f, 1.0000f, 0.9959f};
const Spectrum yellow = {0.0001f, 0.0000f, 0.1088f, 0.6651f, 1.0000f, 1.0000f, 0.9996f, 0.9586f, 0.9685f, 0.9840f};
const Spectrum red = {0.1012f, 0.0515f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.8325f, 1.0149f, 1.0149f, 1.0149f};
const Spectrum green = {0.0000f, 0.0000f, 0.0273f, 0.7937f, 1.0000f, 0.9418f, 0.1719f, 0.0000f, 0.0000f, 0.0025f};
const Spectrum blue = {1.0000f, 1.0000f, 0.8916f, 0.3323f, 0.0000f, 0.0000f, 0.0003f, 0.0369f, 0.0483f, 0.0496f};

// Returns a * x + b * y + c * z for three spectra
Spectrum combine(float a, const Spectrum &x, float b, const Spectrum &y, float c, const Spectrum &z)
{
    Spectrum result;
    for (int band = 0; band < spectralBands; band++)
        result[band] = a * x[band] + b * y[band] + c * z[band];
    return result;
}

} // namespace

Spectrum spectrumFromRGB(const glm::vec3 &color)
{
    // The smallest channel becomes white, the gap to the middle channel becomes a secondary color
    // (cyan, magenta, or yellow), and the gap to the largest channel becomes a primary color
    float r = color.r;
    float g = color.g;
    float b = color.b;
    if (r <= g && r <= b)
    {
        if (g <= b)
            return combine(r, white, g - r, cyan, b - g, blue);
        return combine(r, white, b - r, cyan, g - b, green);
    }
    if (g <= r && g <= b)
    {
        if (r <= b)
            return combine(g, white, r - g, magenta, b - r, blue);
        return combine(g, white, b - g, magenta, r - b, red);
    }
    if (r <= g)
        return combine(b, white, r - b, yellow, g - r, green);
    return combine(b, white, g - b, yellow, r - g, red);
}

glm::vec3 rgbFromSpectrum(const Spectrum &spectrum)
{
    glm::vec3 rgb(0.0f);
    for (int ch = 0; ch < 3; ch++)
        for (int band = 0; band < spectralBands; band++)
            rgb[ch] += spectrumToRGBMatrix[ch][band] * spectrum[band];
    return rgb;
}

// The formulas below are the same as in KubelkaMunk.cpp, applied to each wavelength band instead of each RGB channel

SpectralReflectTransmit computeSpectralLayer(const SpectralLayer &layer)
{
    SpectralReflectTransmit result;
    for (int band = 0; band < spectralBands; band++)
    {
        float K = layer.pigment.K[band];
        float S = layer.pigment.S[band];
        float a = (S + K) / S;
        float b = std::sqrt(a * a - 1.0f);
        float bSx = b * S * layer.thickness;
        float c = a * std::sinh(bSx) + b * std::cosh(bSx);
        // R = sinh(bSx) / c, rewritten with tanh because sinh and cosh overflow for thick layers
        float t = std::tanh(bSx);
        result.R[band] = t / (a * t + b);
        result.T[band] = b / c;
    }
    return result;
}

SpectralReflectTransmit stackSpectralLayers(const SpectralReflectTransmit &top, const SpectralReflectTransmit &bottom)
{
    SpectralReflectTransmit result;
    for (int band = 0; band < spectralBands; band++)
    {
        float bounce = 1.0f - top.R[band] * bottom.R[band];
        result.R[band] = top.R[band] + top.T[band] * top.T[band] * bottom.R[band] / bounce;
        result.T[band] = top.T[band] * bottom.T[band] / bounce;
    }
    return result;
}

SpectralLayer spectralLayerFromMix(const std::vector<SpectralLayer> &parts)
{
    if (parts.empty())
        throw std::invalid_argument("spectralLayerFromMix needs at least one part");
    SpectralLayer mixed = {};
    for (const SpectralLayer &part : parts)
    {
        for (int band = 0; band < spectralBands; band++)
        {
            mixed.pigment.K[band] += part.pigment.K[band] * part.thickness;
            mixed.pigment.S[band] += part.pigment.S[band] * part.thickness;
        }
        mixed.thickness += part.thickness;
    }

    // With no paint at all, keep the first pigment so that S is never 0
    if (mixed.thickness <= 0.0f)
        return {parts[0].pigment, 0.0f};
    for (int band = 0; band < spectralBands; band++)
    {
        mixed.pigment.K[band] /= mixed.thickness;
        mixed.pigment.S[band] /= mixed.thickness;
    }
    return mixed;
}

glm::vec3 colorFromSpectralLayers(const std::vector<SpectralLayer> &layers, const glm::vec3 &paperColor)
{
    // Start with bare paper: it reflects its own color and lets no light through
    SpectralReflectTransmit stack;
    stack.R = spectrumFromRGB(paperColor);
    stack.T.fill(0.0f);

    for (const SpectralLayer &layer : layers)
        stack = stackSpectralLayers(computeSpectralLayer(layer), stack);

    return rgbFromSpectrum(stack.R);
}
