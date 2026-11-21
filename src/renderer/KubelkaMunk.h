#pragma once
#include "KubelkaMunkCore.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Kubelka-Munk color model
// K, S, R, and T are defined per wavelength of light;
// Here they are approximated with red, green, and blue, so each is a vec3 with one value per color channel

// A pigment: K (absorption coefficient) and S (scattering coefficient)
using Pigment = kmcore::Pigment<glm::vec3>;

// One layer of paint: which pigment, and how thick (thickness)
using Layer = kmcore::Layer<glm::vec3>;

// A pigment with a human-readable name, used for palette entries
struct NamedPigment
{
    std::string name;
    Pigment pigment;
};

// Returns the 12 pigments defined by Curtis et al. (1997)
const std::vector<NamedPigment> &curtisPalette();

// Returns the pigment with the given name from curtisPalette()
// Throws std::invalid_argument if there is none
Pigment pigmentFromName(const std::string &name);

// Builds a pigment from its RGB color (0 to 1) over white and over black paper, as in Curtis et al. (1997)
// Throws std::invalid_argument unless 0 < onBlack < onWhite < 1
Pigment pigmentFromColors(const glm::vec3 &onWhite, const glm::vec3 &onBlack);

// Builds a pigment from its RGB color (0 to 1) over white paper
// opacity (0 to 1) is how much of that color still shows over black paper, 0 means fully transparent, 1 means fully opaque
// The default 0.3 is typical of the pigments in Curtis et al. (1997)
Pigment pigmentFromRGB(const glm::vec3 &color, float opacity = 0.3f);

// Mixes several pigments into one layer, as in wet-in-wet painting (Curtis et al. 1997)
// Each part's thickness is the amount of that pigment, and the mixed layer's thickness is their sum
// Throws std::invalid_argument if parts is empty
Layer layerFromMix(const std::vector<Layer> &parts);

// Returns the final color of layers of paint on paper
// layers are ordered from bottom (painted first) to top (painted last)
// paperColor is the color of the paper underneath the layers of paint, 0 to 1 per channel
glm::vec3 colorFromLayers(const std::vector<Layer> &layers, const glm::vec3 &paperColor);

//===----------------------------------------------------------------------===//
// Lower-level steps used by colorFromLayers
//===----------------------------------------------------------------------===//

// How much light is reflected (R) and transmitted (T), by one layer or by several layers stacked together
using ReflectTransmit = kmcore::ReflectTransmit<glm::vec3>;

// Computes R and T for one layer of paint
ReflectTransmit computeLayer(const Layer &layer);

// Combines two stacked layers into one effective layer, and computes the resulting R and T
// top is the layer light reaches first; bottom is the layer under it
ReflectTransmit stackLayers(const ReflectTransmit &top, const ReflectTransmit &bottom);
