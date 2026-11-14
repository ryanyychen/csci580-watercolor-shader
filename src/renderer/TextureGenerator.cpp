#include "TextureGenerator.h"

#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <random>

namespace
{
const int gradientGridSize = 256; //expanded grid size to account for fractal noise layers
//this is a 2D array of unit vectors that will be used to generate the noise texture.
glm::vec2 gradientGrid[gradientGridSize][gradientGridSize];

float perlinNoise(float x, float y)
{
    // multiplying by base gradient size = 6 to scale x,y to corresponding gradient coordinates
    // gradient size scaling per noise layer implicitly handled by x, y scaling on input
    float x_grad = x * 6; 
    float y_grad = y * 6; 
    int left = static_cast<int>(std::floor(x_grad));
    int right = static_cast<int>(std::floor(x_grad)) + 1;
    int top = static_cast<int>(std::floor(y_grad));
    int bottom = static_cast<int>(std::floor(y_grad)) + 1;

    glm::vec2 topLeftVal = gradientGrid[top][left];
    glm::vec2 topRightVal = gradientGrid[top][right];
    glm::vec2 bottomLeftVal = gradientGrid[bottom][left];
    glm::vec2 bottomRightVal = gradientGrid[bottom][right];

    glm::vec2 topLeftVec = glm::vec2(x_grad, y_grad) - glm::vec2(left, top);
    glm::vec2 topRightVec = glm::vec2(x_grad, y_grad) - glm::vec2(right, top);
    glm::vec2 bottomLeftVec = glm::vec2(x_grad, y_grad) - glm::vec2(left, bottom);
    glm::vec2 bottomRightVec = glm::vec2(x_grad, y_grad) - glm::vec2(right, bottom);

    float topLeftDot = glm::dot(topLeftVec, topLeftVal);
    float topRightDot = glm::dot(topRightVec, topRightVal);
    float bottomLeftDot = glm::dot(bottomLeftVec, bottomLeftVal);
    float bottomRightDot = glm::dot(bottomRightVec, bottomRightVal);

    // smooth step prevent grid artifacts
    float tx = x_grad - left;
    float smoothX = (3 * std::pow(tx, 2)) - (2 * std::pow(tx, 3));
    float ty = y_grad - top;
    float smoothY = (3 * std::pow(ty, 2)) - (2 * std::pow(ty, 3));
    float lerpTopDots = (topLeftDot * (1 - smoothX)) + (topRightDot * smoothX);
    float lerpBottomDots = (bottomLeftDot * (1 - smoothX)) + (bottomRightDot * smoothX);
    float blerp = (lerpTopDots * (1 - smoothY)) + (lerpBottomDots * smoothY);
    
    return blerp;
}
}

void generateGradientGrid()
{
  std::default_random_engine perlin_generator;
  std::uniform_real_distribution<float> distribution(0, 2.0f * std::acos(-1.0f));
  for (int i = 0; i < gradientGridSize; i++) {
    for (int j = 0; j < gradientGridSize; j++) {
      float angle = distribution(perlin_generator);
      float y = std::sin(angle);
      float x = std::cos(angle);
      glm::vec2 v(x, y);
      gradientGrid[i][j] = v;
    }
  }
}

float ** createPerlinTexture(int size)
{
  float ** pixels = static_cast<float **>(malloc(size * sizeof(float *)));
  if (pixels == nullptr)
    return nullptr;

  for (int y = 0; y < size; ++y)
  {
    pixels[y] = static_cast<float *>(malloc(size * sizeof(float)));
    if (pixels[y] == nullptr)
    {
      for (int allocatedRow = 0; allocatedRow < y; ++allocatedRow)
        free(pixels[allocatedRow]);
      free(pixels);
      return nullptr;
    }
  }

  float persistence = 0.5; // amplitude multiplier per layer
  float lacunarity = 2.0; // noise_size multiplier per layer
  float amp = 1.0;
  float size_scale = 1;
  float total_amp = 0;
  for (int y = 0; y < size; ++y)
  {
    for (int x = 0; x < size; ++x)
    {
      pixels[y][x] = 0;
    }
  }
  for (int i = 0; i < 4; i++) { // iterate multiple times for fractal noise
    for (int y = 0; y < size; ++y)
    {
      for (int x = 0; x < size; ++x)
      {
        float perlin = perlinNoise((static_cast<float>(x) / size) * size_scale, (static_cast<float>(y) / size) * size_scale);
        pixels[y][x] += perlin * amp;
      }
    }
    total_amp += amp;
    amp *= persistence;
    size_scale *= lacunarity;
  }

  // normalize perlin noise to [0,1] for textures
  for (int y = 0; y < size; ++y)
    {
      for (int x = 0; x < size; ++x)
      {
        pixels[y][x] = 0.5f * ((pixels[y][x] / total_amp) + 1.0f);
      }
    }
  return pixels;
}
