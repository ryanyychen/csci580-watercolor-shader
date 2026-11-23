#ifndef TEXTURE_GENERATOR_H
#define TEXTURE_GENERATOR_H

#include <glm/glm.hpp>
#include <vector>

void generateGradientGrid();
void resizeGradientGrid(int size);
std::vector<float> createLineIntegralConvolutionTexture(
    int size,
    const std::vector<float> &source,
    const std::vector<glm::vec2> &directionField);
float ** createPerlinTexture(
    int size,
    int octaveCount = 4,
    float persistence = 0.5f,
    float lacunarity = 2.0f);
float ** createLineIntegralConvolutionTexture(
    int size,
    float ** source,
    const std::vector<glm::vec2> &directionField);
float ** createPaperTexture(int width, int height);
#endif
