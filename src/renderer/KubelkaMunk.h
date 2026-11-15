#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Kubelka-Munk color model
// K, S, R, and T are defined per wavelength of light; 
// Here they are approximated with red, green, and blue, so each is a vec3 with one value per color channel

struct Pigment
{
    std::string name;
    glm::vec3 K; // absorption coefficient: how strongly the pigment absorbs light; absorbed light is lost, lowering both R and T
    glm::vec3 S; // scattering coefficient: how strongly the pigment scatters light back up; this raises R and lowers T
};

// How much light is reflected and transmitted, by one layer or by several layers stacked together
struct ReflectTransmit
{
    glm::vec3 R; // reflectance: fraction (0 to 1) of light the layer reflects back
    glm::vec3 T; // transmittance: fraction (0 to 1) of light that passes through the layer
};

// Returns the 12 pigments defined by Curtis et al. (1997)
// A pigment is identified by its index in this list
const std::vector<Pigment> &curtisPalette();

// Computes R and T for one layer of a single pigment
// thickness is how much paint the layer has: 0 means no paint, and larger values mean thicker paint
ReflectTransmit computeLayer(const Pigment &pigment, float thickness);

// Combines two stacked layers into one effective layer, and computes the resulting R and T
// top is the layer light reaches first; bottom is the layer under it
ReflectTransmit stackLayers(const ReflectTransmit &top, const ReflectTransmit &bottom);
