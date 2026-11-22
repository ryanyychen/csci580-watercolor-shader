/*
  CSCI 580 Computer Graphics, Computer Science, USC
  Texture setup helper code
  Carter Slocum, PhD.
*/

#include "Texture.h"

#include <algorithm>
#include <stdlib.h>
#include <stdexcept>

Texture::Texture(int width, int height, const char * pixels)
  : width(width), height(height), textureId(0)
{
  if (pixels == nullptr)
	throw std::invalid_argument("Texture pixel data cannot be null.");

  Initialize(pixels);
}

Texture::Texture(int width, int height, const float * const * values)
  : width(width), height(height), textureId(0)
{
	if (width <= 0 || height <= 0)
	throw std::invalid_argument("Texture dimensions must be positive.");
  if (values == nullptr)
	throw std::invalid_argument("Texture float data cannot be null.");

  for (int y = 0; y < height; ++y)
  {
	if (values[y] == nullptr)
	  throw std::invalid_argument("Texture float rows cannot be null.");
  }

  const size_t pixelCount = static_cast<size_t>(width) * height;
  unsigned char * pixels = static_cast<unsigned char *>(malloc(pixelCount));
  if (pixels == nullptr)
	throw std::runtime_error("Failed to allocate texture pixel data.");

  for (int y = 0; y < height; ++y)
  {
	for (int x = 0; x < width; ++x)
	{
	  const float value = std::max(0.0f, std::min(1.0f, values[y][x]));
	  pixels[y * width + x] = static_cast<unsigned char>(value * 255.0f);
	}
  }

	Initialize(pixels);
  free(pixels);
}

Texture::Texture(int width, int height, const std::vector<glm::vec3> &colors)
  : width(width), height(height), textureId(0)
{
  if (width <= 0 || height <= 0 ||
      colors.size() != static_cast<size_t>(width) * static_cast<size_t>(height))
    throw std::invalid_argument("RGB texture dimensions do not match its colors.");

  // Pack explicitly so GLM alignment settings cannot insert padding between texels.
  std::vector<float> pixels;
  pixels.reserve(colors.size() * 3);
  for (const glm::vec3 &color : colors)
    for (int channel = 0; channel < 3; ++channel)
      pixels.push_back(color[channel]);
  Initialize(pixels.data(), GL_RGB32F, GL_RGB, GL_FLOAT);
}

Texture::~Texture()
{
  if (textureId != 0)
	glDeleteTextures(1, &textureId);
}

void Texture::Initialize(const void * pixels, GLint internalFormat, GLenum format, GLenum type)
{
  if (width <= 0 || height <= 0)
	throw std::invalid_argument("Texture dimensions must be positive.");

  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // A single-channel noise row need not be a multiple of the default four bytes.
  GLint previousAlignment = 0;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, pixels);
  glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
}

void Texture::Bind(GLenum textureUnit) const
{
  glActiveTexture(textureUnit);
  glBindTexture(GL_TEXTURE_2D, textureId);
}

GLuint Texture::GetId() const
{
  return textureId;
}
