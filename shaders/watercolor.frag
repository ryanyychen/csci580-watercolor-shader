#version 150

in vec2 noiseUV;
in vec3 vObjectPosition;
in vec3 vObjectNormal;
uniform sampler2D noiseTexture;
uniform sampler2D licTexture;
uniform sampler2D licTextureXPositive;
uniform sampler2D licTextureXNegative;
uniform sampler2D licTextureYPositive;
uniform sampler2D licTextureYNegative;
uniform sampler2D licTextureZPositive;
uniform sampler2D licTextureZNegative;
uniform float uNoiseScale;
uniform float uLICBias;
uniform float uLICGain;
uniform int uUseMeshTextureCoordinates;
uniform int uUseTriplanarNoise;
uniform int uUseLIC;

out vec4 c;

float biasValue(float value, float amount)
{
  float safeAmount = clamp(amount, 0.001, 0.999);
  return pow(clamp(value, 0.0, 1.0), log(safeAmount) / log(0.5));
}

float gainValue(float value, float amount)
{
  float safeAmount = clamp(amount, 0.001, 0.999);
  return value < 0.5
      ? 0.5 * biasValue(2.0 * value, 1.0 - safeAmount)
      : 1.0 - 0.5 * biasValue(2.0 - 2.0 * value, 1.0 - safeAmount);
}

void main()
{
  bool licEnabled = uUseLIC != 0;
  float noise;
  if (uUseMeshTextureCoordinates != 0)
  {
    noise = licEnabled
        ? texture(licTexture, noiseUV).r
        : texture(noiseTexture, noiseUV).r;
  }
  else if (uUseTriplanarNoise != 0)
  {
    vec3 normal = normalize(vObjectNormal);
    vec3 weights = pow(abs(normal), vec3(4.0));
    weights /= max(weights.x + weights.y + weights.z, 1e-6);
    vec2 uvX = 0.5 + 0.5 * vObjectPosition.yz * uNoiseScale;
    vec2 uvY = 0.5 + 0.5 * vObjectPosition.xz * uNoiseScale;
    vec2 uvZ = 0.5 + 0.5 * vObjectPosition.xy * uNoiseScale;

    float noiseX = licEnabled
        ? (normal.x >= 0.0
               ? texture(licTextureXPositive, uvX).r
               : texture(licTextureXNegative, uvX).r)
        : texture(noiseTexture, uvX).r;
    float noiseY = licEnabled
        ? (normal.y >= 0.0
               ? texture(licTextureYPositive, uvY).r
               : texture(licTextureYNegative, uvY).r)
        : texture(noiseTexture, uvY).r;
    float noiseZ = licEnabled
        ? (normal.z >= 0.0
               ? texture(licTextureZPositive, uvZ).r
               : texture(licTextureZNegative, uvZ).r)
        : texture(noiseTexture, uvZ).r;
    noise = dot(weights, vec3(noiseX, noiseY, noiseZ));
  }
  else
  {
    noise = licEnabled
        ? texture(licTexture, noiseUV).r
        : texture(noiseTexture, noiseUV).r;
  }

  float contrastedNoise = clamp(0.5 + 2.5 * (noise - 0.5), 0.0, 1.0);
  if (licEnabled)
    contrastedNoise = gainValue(biasValue(contrastedNoise, uLICBias), uLICGain);
  vec3 color = licEnabled
      ? vec3(0.45 + 0.4 * contrastedNoise,
             0.62 + 0.3 * contrastedNoise,
             0.82 + 0.18 * contrastedNoise)
      : vec3(noise);
  c = vec4(color, 1.0);
}
  

