#pragma once
#include "KubelkaMunkCore.h"
#include <array>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Spectral version of the Kubelka-Munk color model
// Light is sampled at 10 wavelengths instead of only red, green, and blue

constexpr int spectralBands = 10;

using Spectrum = std::array<float, spectralBands>;

// Same as Pigment in KubelkaMunk.h, with K and S given per wavelength band
using SpectralPigment = kmcore::Pigment<Spectrum>;

// Same as Layer in KubelkaMunk.h, with a spectral pigment
using SpectralLayer = kmcore::Layer<Spectrum>;

// Same as pigmentFromName in KubelkaMunk.h, converted to a spectral pigment
// Throws std::invalid_argument if there is no pigment with that name
SpectralPigment spectralPigmentFromName(const std::string &name);

// Same as pigmentFromColors in KubelkaMunk.h, solved per wavelength band
// Band values are clamped so that 0 < onBlack < onWhite < 1 holds in every band
SpectralPigment spectralPigmentFromColors(const glm::vec3 &onWhite, const glm::vec3 &onBlack);

// Same as pigmentFromRGB in KubelkaMunk.h
SpectralPigment spectralPigmentFromRGB(const glm::vec3 &color, float opacity = 0.3f);

// Same as layerFromMix in KubelkaMunk.h, computed per wavelength band
// Throws std::invalid_argument if parts is empty
SpectralLayer spectralLayerFromMix(const std::vector<SpectralLayer> &parts);

// Same as colorFromLayers in KubelkaMunk.h, computed per wavelength band and converted to RGB at the end
// layers are ordered from bottom (painted first) to top (painted last)
// paperColor is the RGB color of the paper, 0 to 1 per channel
glm::vec3 colorFromSpectralLayers(const std::vector<SpectralLayer> &layers, const glm::vec3 &paperColor);

//===----------------------------------------------------------------------===//
// Conversions between RGB colors and spectra
//===----------------------------------------------------------------------===//

// Converts a reflectance spectrum to an RGB color (0 to 1 per channel)
// A spectrum of all 1 gives white (1, 1, 1)
glm::vec3 rgbFromSpectrum(const Spectrum &spectrum);

// Converts an RGB color (0 to 1 per channel) to a reflectance spectrum, using the method of Smits (1999)
Spectrum spectrumFromRGB(const glm::vec3 &color);

//===----------------------------------------------------------------------===//
// Lower-level steps used by colorFromSpectralLayers
//===----------------------------------------------------------------------===//

// Same as ReflectTransmit in KubelkaMunk.h, per wavelength band
using SpectralReflectTransmit = kmcore::ReflectTransmit<Spectrum>;

// Computes R and T for one layer of paint
SpectralReflectTransmit computeSpectralLayer(const SpectralLayer &layer);

// Combines two stacked layers into one effective layer; top is the layer light reaches first
SpectralReflectTransmit stackSpectralLayers(const SpectralReflectTransmit &top, const SpectralReflectTransmit &bottom);
