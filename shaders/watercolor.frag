#version 150

in vec2 noiseUV;
uniform sampler2D noiseTexture;

out vec4 c;

void main()
{
  float noise = texture(noiseTexture, noiseUV).r;
  c = vec4(vec3(noise), 1.0);
}
  

