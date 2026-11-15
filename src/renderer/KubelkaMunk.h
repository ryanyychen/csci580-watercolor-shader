#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Kubelka-Munk color model
// K, S, R, and T are defined per wavelength of light; 
// Here they are approximated with red, green, and blue, so each is a vec3 with one value per color channel

struct Pigment
{
    glm::vec3 K; // absorption coefficient: how strongly the pigment absorbs light; absorbed light is lost, lowering both R and T
    glm::vec3 S; // scattering coefficient: how strongly the pigment scatters light back up; this raises R and lowers T
};

// One layer of paint: which pigment, and how thick
struct Layer
{
    Pigment pigment;
    float thickness; // 0 means no paint, larger values mean thicker paint
};

// How much light is reflected and transmitted, by one layer or by several layers stacked together
struct ReflectTransmit
{
    glm::vec3 R; // reflectance: fraction (0 to 1) of light the layer reflects back
    glm::vec3 T; // transmittance: fraction (0 to 1) of light that passes through the layer
};

// A pigment with a human-readable name, used for palette entries
struct NamedPigment
{
    std::string name;
    Pigment pigment;
};

// Returns the 12 pigments defined by Curtis et al. (1997)
const std::vector<NamedPigment> &curtisPalette();

// Computes R and T for one layer of paint
ReflectTransmit computeLayer(const Layer &layer);

// Combines two stacked layers into one effective layer, and computes the resulting R and T
// top is the layer light reaches first; bottom is the layer under it
ReflectTransmit stackLayers(const ReflectTransmit &top, const ReflectTransmit &bottom);

// Returns the final color of layers of paint on paper
// layers are ordered from bottom (painted first) to top (painted last)
// paperColor is the color of the paper underneath the layers of paint, 0 to 1 per channel
glm::vec3 composite(const std::vector<Layer> &layers, const glm::vec3 &paperColor);
