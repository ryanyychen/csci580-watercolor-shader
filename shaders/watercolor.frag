#version 330 core

in vec2 noiseUV;
in vec3 vObjectPosition;
in vec3 vObjectNormal;
in vec3 vWorldPosition;
in vec3 vWorldNormal;
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
uniform sampler2D pigmentLookup;
uniform int uUsePigmentLookup;
uniform float uPigmentMaxThickness;
uniform float uPaintThickness;
uniform float uPigmentNoiseStrength;
uniform int uLightCount;
uniform vec3 uLightPosition[8];
uniform vec3 uLightColor[8];
uniform float uAmbientIntensity;
uniform vec3 uAmbientColor;

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
  // Preserve a noise diagnostic for custom scenes that have not supplied a table.
  if (uUsePigmentLookup == 0)
  {
    c = vec4(vec3(contrastedNoise), 1.0);
    return;
  }

  // Lighting drives pigment amount, not a hardcoded blue RGB tint. More paint
  // goes into shaded regions. The fixed lookup assumes its baked illuminant;
  // colored lights affect density through luminance, not spectral illumination.
  const vec3 luminanceWeights = vec3(0.2126, 0.7152, 0.0722);
  vec3 N = normalize(vWorldNormal);
  float illumination = uAmbientIntensity * dot(uAmbientColor, luminanceWeights);
  for (int i = 0; i < uLightCount; ++i)
  {
    vec3 L = normalize(uLightPosition[i] - vWorldPosition);
    illumination += max(dot(N, L), 0.0) * dot(uLightColor[i], luminanceWeights);
  }
  float shade = 1.0 - clamp(illumination, 0.0, 1.0);
  float variation = 1.0 + clamp(uPigmentNoiseStrength, 0.0, 1.0) *
                          (1.0 - 2.0 * contrastedNoise);
  float thickness = clamp(max(uPaintThickness, 0.0) * (0.2 + 1.4 * shade) * variation,
                          0.0, uPigmentMaxThickness);

  // Samples include both endpoints. Map them to texel centers so zero thickness
  // returns paper and the maximum returns the last baked color exactly.
  float width = float(textureSize(pigmentLookup, 0).x);
  float fraction = thickness / max(uPigmentMaxThickness, 1e-6);
  float lookupU = (0.5 + fraction * (width - 1.0)) / width;
  vec3 linearColor = texture(pigmentLookup, vec2(lookupU, 0.5)).rgb;
  c = vec4(pow(clamp(linearColor, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
  

