#ifndef TEXTURE_GENERATOR_H
#define TEXTURE_GENERATOR_H

#include <glm/glm.hpp>
#include <vector>

void generateGradientGrid();
std::vector<float> createLineIntegralConvolutionTexture(
    int size,
    const std::vector<float> &source,
    const std::vector<glm::vec2> &directionField);
float ** createPerlinTexture(int size);
float ** createLineIntegralConvolutionTexture(
    int size,
    float ** source,
    const std::vector<glm::vec2> &directionField);
float ** createPaperTexture(int width, int height);
#endif
