#include "TextureGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <random>
#include <stdexcept>
#include <vector>

namespace
{
  constexpr int minimumGradientGridSize = 49;
  int gradientGridSize = 256; // expanded grid size to account for fractal noise layers
  //this is a 2D array of unit vectors that will be used to generate the noise texture.
  std::vector<std::vector<glm::vec2>> gradientGrid(
      gradientGridSize, std::vector<glm::vec2>(gradientGridSize));

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

  float sampleScalar(const std::vector<float> &values, int size, glm::vec2 point)
  {
      point = glm::clamp(point, glm::vec2(0.0f), glm::vec2(static_cast<float>(size - 1)));
      const int x0 = static_cast<int>(std::floor(point.x));
      const int y0 = static_cast<int>(std::floor(point.y));
      const int x1 = std::min(x0 + 1, size - 1);
      const int y1 = std::min(y0 + 1, size - 1);
      const float tx = point.x - x0;
      const float ty = point.y - y0;
      const float top = glm::mix(values[y0 * size + x0], values[y0 * size + x1], tx);
      const float bottom = glm::mix(values[y1 * size + x0], values[y1 * size + x1], tx);
      return glm::mix(top, bottom, ty);
  }

  glm::vec2 sampleDirection(const std::vector<glm::vec2> &field, int size, glm::vec2 point)
  {
      if (point.x < 0.0f || point.y < 0.0f ||
          point.x > static_cast<float>(size - 1) ||
          point.y > static_cast<float>(size - 1))
          return glm::vec2(0.0f);

      const int x0 = static_cast<int>(std::floor(point.x));
      const int y0 = static_cast<int>(std::floor(point.y));
      const int x1 = std::min(x0 + 1, size - 1);
      const int y1 = std::min(y0 + 1, size - 1);
      const float tx = point.x - x0;
      const float ty = point.y - y0;
      const glm::vec2 reference = field[y0 * size + x0];
      if (glm::length(reference) < 1e-6f)
          return glm::vec2(0.0f);

      glm::vec2 sum(0.0f);
      const int xs[4] = {x0, x1, x0, x1};
      const int ys[4] = {y0, y0, y1, y1};
      const float weights[4] = {
          (1.0f - tx) * (1.0f - ty), tx * (1.0f - ty),
          (1.0f - tx) * ty, tx * ty};
      for (int i = 0; i < 4; ++i)
      {
          glm::vec2 direction = field[ys[i] * size + xs[i]];
          if (glm::dot(direction, reference) < 0.0f)
              direction = -direction;
          sum += weights[i] * direction;
      }

      const float length = glm::length(sum);
      return length > 1e-6f ? sum / length : glm::vec2(0.0f);
  }

  void traceLIC(const std::vector<float> &source, const std::vector<glm::vec2> &field,
                int size, glm::vec2 start, glm::vec2 initialDirection, float sign,
                float &sum, float &weightSum)
  {
      constexpr int traceLength = 16;
      glm::vec2 position = start;
      glm::vec2 previousDirection = sign * initialDirection;
      for (int step = 1; step <= traceLength; ++step)
      {
          glm::vec2 direction = sampleDirection(field, size, position);
          if (glm::length(direction) < 1e-6f)
              break;
          if (glm::dot(direction, previousDirection) < 0.0f)
              direction = -direction;

          const glm::vec2 midpoint = position + 0.5f * direction;
          glm::vec2 midpointDirection = sampleDirection(field, size, midpoint);
          if (glm::length(midpointDirection) < 1e-6f)
              break;
          if (glm::dot(midpointDirection, direction) < 0.0f)
              midpointDirection = -midpointDirection;

          position += midpointDirection;
          if (position.x < 0.0f || position.y < 0.0f ||
              position.x > static_cast<float>(size - 1) ||
              position.y > static_cast<float>(size - 1))
              break;

          const float normalizedDistance =
              static_cast<float>(step) / static_cast<float>(traceLength + 1);
          const float weight = 0.5f * (1.0f + std::cos(
              static_cast<float>(std::acos(-1.0)) * normalizedDistance));
          sum += weight * sampleScalar(source, size, position);
          weightSum += weight;
          previousDirection = midpointDirection;
      }
  }
}

void resizeGradientGrid(int newSize)
{
  if (newSize < minimumGradientGridSize)
    throw std::invalid_argument("Gradient grid is too small for the Perlin noise octaves");

  std::vector<std::vector<glm::vec2>> newGradientGrid(
      newSize, std::vector<glm::vec2>(newSize));
  std::default_random_engine perlin_generator;
  std::uniform_real_distribution<float> distribution(0, 2.0f * std::acos(-1.0f));
  for (int i = 0; i < newSize; i++) {
    for (int j = 0; j < newSize; j++) {
      float angle = distribution(perlin_generator);
      float y = std::sin(angle);
      float x = std::cos(angle);
      glm::vec2 v(x, y);
      newGradientGrid[i][j] = v;
    }
  }
  gradientGrid.swap(newGradientGrid);
  gradientGridSize = newSize;
}

void generateGradientGrid()
{
  resizeGradientGrid(256);
}

float ** createPerlinTexture(
    int size, int octaveCount, float persistence, float lacunarity)
{
  if (size <= 0)
    return nullptr;
  if (octaveCount <= 0)
    throw std::invalid_argument("Perlin octave count must be positive");
  if (!std::isfinite(persistence) || persistence < 0.0f || persistence > 1.0f)
    throw std::invalid_argument("Perlin persistence must be between zero and one");
  if (!std::isfinite(lacunarity) || lacunarity <= 0.0f)
    throw std::invalid_argument("Perlin lacunarity must be positive and finite");

  const double largestScale = std::max(
      1.0, std::pow(static_cast<double>(lacunarity), octaveCount - 1));
  const double requiredGridSize = std::ceil(6.0 * largestScale) + 1.0;
  if (!std::isfinite(requiredGridSize) || requiredGridSize > gradientGridSize)
    throw std::invalid_argument("Gradient grid is too small for the requested Perlin octaves");

  float ** pixels = static_cast<float **>(malloc(static_cast<size_t>(size) * sizeof(float *)));
  if (pixels == nullptr)
    return nullptr;

  for (int y = 0; y < size; ++y)
  {
    pixels[y] = static_cast<float *>(malloc(static_cast<size_t>(size) * sizeof(float)));
    if (pixels[y] == nullptr)
    {
      for (int allocatedRow = 0; allocatedRow < y; ++allocatedRow)
        free(pixels[allocatedRow]);
      free(pixels);
      return nullptr;
    }
  }

  float amp = 1.0;
  float size_scale = 1;
  float total_amp = 0;
  for (int y = 0; y < size; ++y)
  {
    for (int x = 0; x < size; ++x)
      pixels[y][x] = 0.0f;
  }
  for (int i = 0; i < octaveCount; i++) {
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

std::vector<float> createLineIntegralConvolutionTexture(
    int size, const std::vector<float> &source,
    const std::vector<glm::vec2> &directionField)
{
  if (size <= 0 || source.size() != static_cast<size_t>(size) * size ||
      directionField.size() != source.size())
    throw std::invalid_argument("LIC input dimensions do not match");

  std::vector<float> result = source;
  for (int y = 0; y < size; ++y)
  {
    for (int x = 0; x < size; ++x)
    {
      const size_t index = static_cast<size_t>(y) * size + x;
      glm::vec2 direction = directionField[index];
      const float directionLength = glm::length(direction);
      if (directionLength < 1e-6f)
        continue;
      direction /= directionLength;

      float sum = source[index];
      float weightSum = 1.0f;
      traceLIC(source, directionField, size, glm::vec2(x, y), direction, 1.0f,
               sum, weightSum);
      traceLIC(source, directionField, size, glm::vec2(x, y), direction, -1.0f,
               sum, weightSum);
      result[index] = sum / weightSum;
    }
  }
  return result;
}

float ** createLineIntegralConvolutionTexture(
    int size, float ** source, const std::vector<glm::vec2> &directionField)
{
  if (size <= 0 || source == nullptr)
    throw std::invalid_argument("LIC source texture is invalid");

  std::vector<float> sourceValues(static_cast<size_t>(size) * size);
  for (int y = 0; y < size; ++y)
  {
    if (source[y] == nullptr)
      throw std::invalid_argument("LIC source texture contains a null row");
    for (int x = 0; x < size; ++x)
      sourceValues[static_cast<size_t>(y) * size + x] = source[y][x];
  }
  const std::vector<float> values =
      createLineIntegralConvolutionTexture(size, sourceValues, directionField);

  float ** pixels = static_cast<float **>(malloc(static_cast<size_t>(size) * sizeof(float *)));
  if (pixels == nullptr)
    return nullptr;

  for (int y = 0; y < size; ++y)
  {
    pixels[y] = static_cast<float *>(malloc(static_cast<size_t>(size) * sizeof(float)));
    if (pixels[y] == nullptr)
    {
      for (int allocatedRow = 0; allocatedRow < y; ++allocatedRow)
        free(pixels[allocatedRow]);
      free(pixels);
      return nullptr;
    }
    for (int x = 0; x < size; ++x)
      pixels[y][x] = values[static_cast<size_t>(y) * size + x];
  }
  return pixels;
}

float ** createPaperTexture(int width, int height) {
  if (width <= 0 || height <= 0)
    return nullptr;

  int noiseSize = std::max(width, height);
  generateGradientGrid();
  float **noise = createPerlinTexture(noiseSize);
  if (noise == nullptr)
    return nullptr;

  float **paperHeight = static_cast<float **>(
      malloc(static_cast<size_t>(height) * sizeof(float *)));
  if (paperHeight == nullptr)
  {
    for (int y = 0; y < noiseSize; ++y)
      free(noise[y]);
    free(noise);
    return nullptr;
  }

  for (int y = 0; y < height; ++y)
  {
    paperHeight[y] = static_cast<float *>(
        malloc(static_cast<size_t>(width) * sizeof(float)));
    if (paperHeight[y] == nullptr)
    {
      for (int allocatedRow = 0; allocatedRow < y; ++allocatedRow)
        free(paperHeight[allocatedRow]);
      free(paperHeight);
      for (int noiseRow = 0; noiseRow < noiseSize; ++noiseRow)
        free(noise[noiseRow]);
      free(noise);
      return nullptr;
    }
  }

  // perlin noise for the large bumps, white noise for the fiber grain
  std::vector<float> grain(width * height);
  for (int j = 0; j < height; j++)
  {
      for (int i = 0; i < width; i++)
      {
          float random = static_cast<float>(rand()) /
              static_cast<float>(RAND_MAX);
          grain[j * width + i] = 0.6f * noise[j][i] + 0.4f * random;
      }
  }

  // 3x3 average blur to soften the grain
  float minValue = 1000.0f;
  float maxValue = -1000.0f;
  for (int j = 0; j < height; j++)
  {
      for (int i = 0; i < width; i++)
      {
          float sum = 0.0f;
          int count = 0;
          for (int dj = -1; dj <= 1; dj++)
          {
              for (int di = -1; di <= 1; di++)
              {
                  int ni = i + di;
                  int nj = j + dj;
                  if (ni < 0 || ni >= width || nj < 0 || nj >= height)
                      continue;
                  sum += grain[nj * width + ni];
                  count++;
              }
          }
          float h = sum / count;
          paperHeight[j][i] = h;
          minValue = std::min(minValue, h);
          maxValue = std::max(maxValue, h);
      }
  }

  // rescale to [0,1], the paper needs 0 < h < 1
  float range = maxValue - minValue;
  for (int y = 0; y < height; ++y)
  {
      for (int x = 0; x < width; ++x)
      {
          float h = paperHeight[y][x];
          paperHeight[y][x] =
              range > 1e-6f ? (h - minValue) / range : 0.5f;
      }
  }

  for (int y = 0; y < noiseSize; y++)
      free(noise[y]);
  free(noise);
  return paperHeight;
}
