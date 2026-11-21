#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <glm/glm.hpp>
#include <stdexcept>
#include <vector>

// Kubelka-Munk formulas shared by the RGB version (KubelkaMunk.h) and the spectral version (KubelkaMunkSpectral.h)
// Channels is glm::vec3 for red, green, and blue, or Spectrum for wavelength bands
// Use KubelkaMunk.h or KubelkaMunkSpectral.h instead of including this file directly

namespace kmcore
{
inline constexpr int channelCount(const glm::vec3 &)
{
    return 3;
}

template <std::size_t N> constexpr int channelCount(const std::array<float, N> &)
{
    return static_cast<int>(N);
}

template <typename Channels> struct Pigment
{
    Channels K; // absorption coefficient: how strongly the pigment absorbs light; absorbed light is lost, lowering both R and T
    Channels S; // scattering coefficient: how strongly the pigment scatters light back up; this raises R and lowers T
};

template <typename Channels> struct Layer
{
    Pigment<Channels> pigment;
    float thickness; // 0 means no paint, larger values mean thicker paint
};

template <typename Channels> struct ReflectTransmit
{
    Channels R; // reflectance: fraction (0 to 1) of light the layer reflects back
    Channels T; // transmittance: fraction (0 to 1) of light that passes through the layer
};

template <typename Channels> Pigment<Channels> pigmentFromColors(const Channels &onWhite, const Channels &onBlack)
{
    // Computes K and S from Rw (color over white) and Rb (color over black), for each channel
    // a = (Rw + (Rb - Rw + 1) / Rb) / 2
    // b = sqrt(a^2 - 1)
    // S = arccoth((b^2 - (a - Rw)(a - 1)) / (b (1 - Rw))) / b
    // K = S (a - 1)
    Pigment<Channels> pigment;
    for (int ch = 0; ch < channelCount(onWhite); ch++)
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

template <typename Channels> ReflectTransmit<Channels> computeLayer(const Layer<Channels> &layer)
{
    // Kubelka-Munk solution for one layer, computed separately for each channel
    // a, b, and c are intermediate terms in the formula for computing R and T of a single layer
    ReflectTransmit<Channels> result;
    for (int ch = 0; ch < channelCount(layer.pigment.K); ch++)
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

template <typename Channels>
ReflectTransmit<Channels> stackLayers(const ReflectTransmit<Channels> &top, const ReflectTransmit<Channels> &bottom)
{
    // R = Rtop + Ttop^2 * Rbottom / (1 - Rtop * Rbottom)
    // T = Ttop * Tbottom / (1 - Rtop * Rbottom)
    ReflectTransmit<Channels> result;
    for (int ch = 0; ch < channelCount(top.R); ch++)
    {
        float bounce = 1.0f - top.R[ch] * bottom.R[ch];
        result.R[ch] = top.R[ch] + top.T[ch] * top.T[ch] * bottom.R[ch] / bounce;
        result.T[ch] = top.T[ch] * bottom.T[ch] / bounce;
    }
    return result;
}

template <typename Channels> Layer<Channels> layerFromMix(const std::vector<Layer<Channels>> &parts)
{
    // K and S are averages weighted by each part's thickness, and thickness is the sum of the parts
    if (parts.empty())
        throw std::invalid_argument("layerFromMix needs at least one part");
    Layer<Channels> mixed = {};
    for (const Layer<Channels> &part : parts)
    {
        for (int ch = 0; ch < channelCount(part.pigment.K); ch++)
        {
            mixed.pigment.K[ch] += part.pigment.K[ch] * part.thickness;
            mixed.pigment.S[ch] += part.pigment.S[ch] * part.thickness;
        }
        mixed.thickness += part.thickness;
    }

    // With no paint at all, keep the first pigment so that S is never 0
    if (mixed.thickness <= 0.0f)
        return {parts[0].pigment, 0.0f};
    for (int ch = 0; ch < channelCount(mixed.pigment.K); ch++)
    {
        mixed.pigment.K[ch] /= mixed.thickness;
        mixed.pigment.S[ch] /= mixed.thickness;
    }
    return mixed;
}

} // namespace kmcore
