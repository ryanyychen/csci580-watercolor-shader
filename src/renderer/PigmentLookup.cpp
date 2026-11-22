#include "PigmentLookup.h"
#include <cmath>
#include <stdexcept>

std::vector<glm::vec3> createMeasuredPigmentLookup(
    const MeasuredPigment &pigment, const glm::vec3 &paperColor,
    int sampleCount, float maxThickness)
{
    if (sampleCount < 2 || !std::isfinite(maxThickness) || maxThickness <= 0.0f)
        throw std::invalid_argument("Pigment lookup needs at least two samples and positive thickness");
    for (int channel = 0; channel < 3; ++channel)
        if (!std::isfinite(paperColor[channel]) || paperColor[channel] < 0.0f ||
            paperColor[channel] > 1.0f)
            throw std::invalid_argument("Pigment lookup paper must be finite RGB in [0, 1]");

    std::vector<glm::vec3> colors;
    colors.reserve(static_cast<size_t>(sampleCount));
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const float thickness =
            maxThickness * static_cast<float>(sample) / static_cast<float>(sampleCount - 1);
        // Reuse the measured spectral implementation, including its thickness scale
        // and CIE conversion. Do not replace this with an RGB color interpolation.
        const glm::vec3 color = colorFromMeasuredLayers({{pigment, thickness}}, paperColor);
        for (int channel = 0; channel < 3; ++channel)
            if (!std::isfinite(color[channel]))
                throw std::runtime_error("Pigment lookup contains non-finite reflectance");
        // Keep linear, potentially out-of-gamut RGB until the final display pass.
        colors.push_back(color);
    }
    return colors;
}
