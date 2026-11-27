#pragma once
#include "KubelkaMunkCore.h"
#include <array>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Spectral Kubelka-Munk color model with measured pigment data (Okumura 2005)
// Light is sampled at 36 wavelengths from 380 to 730 nm in 10 nm steps, instead of only red, green, and blue

constexpr int measuredBands = 36;

using MeasuredSpectrum = std::array<float, measuredBands>;

// Same as Pigment in KubelkaMunk.h, with K and S given per wavelength band
using MeasuredPigment = kmcore::Pigment<MeasuredSpectrum>;

// Same as Layer in KubelkaMunk.h, with a measured pigment
// thickness is scaled so that it means about the same as for the Curtis pigments in KubelkaMunk.h
using MeasuredLayer = kmcore::Layer<MeasuredSpectrum>;

// A measured pigment with a human-readable name, used for palette entries
struct NamedMeasuredPigment
{
    std::string name;
    MeasuredPigment pigment;
};

// Returns the 27 acrylic paint pigments measured by Okumura (2005)
const std::vector<NamedMeasuredPigment> &measuredPalette();

// Returns the pigment with the given name from measuredPalette()
// Throws std::invalid_argument if there is none
MeasuredPigment measuredPigmentFromName(const std::string &name);

// Same as pigmentFromRGB in KubelkaMunk.h, but mixed from the measured pigments
// Picks one measured pigment, or two mixed in steps of 5%, and how much of it to use,
// so that one layer at thickness 1 over white paper is closest to color
// Closest means the smallest difference in CIELAB, which follows how different two colors look
// Colors that no mix can reach, such as pure red (1, 0, 0), get the closest mix
// The search over pigments and their mixes follows Pigmento (Tan et al. 2018)
MeasuredPigment measuredPigmentFromRGB(const glm::vec3 &color);

// Same as layerFromMix in KubelkaMunk.h, computed per wavelength band
// Throws std::invalid_argument if parts is empty
MeasuredLayer measuredLayerFromMix(const std::vector<MeasuredLayer> &parts);

// Same as colorFromLayers in KubelkaMunk.h, computed per wavelength band and converted to RGB at the end
// layers are ordered from bottom (painted first) to top (painted last)
// paperColor is the RGB color of the paper, 0 to 1 per channel
glm::vec3 colorFromMeasuredLayers(const std::vector<MeasuredLayer> &layers, const glm::vec3 &paperColor);

//===----------------------------------------------------------------------===//
// Conversions between RGB colors and spectra
//===----------------------------------------------------------------------===//

// Converts a reflectance spectrum to an RGB color with the standard CIE 1931 conversion
glm::vec3 rgbFromMeasuredSpectrum(const MeasuredSpectrum &spectrum);

// Converts a paper color to a smooth spectrum that converts back to exactly that color
// This simple spectrum is only meant for paper, which is close to white
MeasuredSpectrum paperSpectrumFromRGB(const glm::vec3 &paperColor);
