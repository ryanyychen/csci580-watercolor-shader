#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/gl.h>

/*helper class for managing OpenGL textures,
  there are several ways to initialize a texture 
  this is just a simple way to do it*/

/*Similar to the vao and vbo classes, textures generate unique IDs when created
  It is usefull to encapsulate this in an object that keeps track of the ID*/

class Texture
{
public:
  //open gl prefers raw bytes for texel data, so we can initialize a texture from a 2D array of bytes
  Texture(int width, int height, const char * pixels);
  //many of our noise textures are generated as float values, so we can also initialize a texture from a 2D array of floats
  Texture(int width, int height, const float * const * values);
  ~Texture();

  Texture(const Texture &) = delete;
  Texture & operator=(const Texture &) = delete;

  //textureUnit is the texture unit to bind to, default is GL_TEXTURE0, but you can have more,
  //use glActiveTexture(GL_TEXTURE0 + n) to select the nth texture unit, and then bind the texture to that unit
  void Bind(GLenum textureUnit = GL_TEXTURE0) const;
  //if you want manual control over the texture unit, you can use this function to get the texture ID and bind it yourself
  GLuint GetId() const;

private:
  void Initialize(const void * pixels);

  int width;
  int height;
  GLuint textureId;
};

#endif
