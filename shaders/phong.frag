#version 330 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
out vec4 fragColor;

uniform vec3 uCameraPosition;
uniform int uLightCount;
uniform vec3 uLightPosition[8];
uniform vec3 uLightColor[8];
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform vec3 uColorAmbient;
uniform vec3 uColorDiffuse;
uniform vec3 uColorSpecular;
uniform float uSpecularPower;
uniform int uShowNormals;

void main()
{
    // Interpolation changes vector length, so normalize again for lighting.
    vec3 N = normalize(vWorldNormal);
    if (uShowNormals != 0)
    {
        // Remap signed direction components from [-1, 1] to visible [0, 1] RGB.
        fragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    // V points from the fragment toward the viewer; all lighting uses world space.
    vec3 V = normalize(uCameraPosition - vWorldPosition);
    vec3 lit = uAmbientIntensity * uAmbientColor * uColorAmbient;
    for (int i = 0; i < uLightCount; ++i)
    {
        // L points toward the light; -L is the incident ray reflected about N.
        vec3 L = normalize(uLightPosition[i] - vWorldPosition);
        vec3 R = reflect(-L, N);
        // Lambert diffuse models the cosine falloff as a surface turns from the light.
        float diffuse = max(dot(N, L), 0.0);
        // Reflection-vector Phong specular, evaluated per fragment.
        // Suppress highlights when the light is behind the surface.
        float specular = diffuse > 0.0 ? pow(max(dot(V, R), 0.0), uSpecularPower) : 0.0;
        lit += uLightColor[i] * (diffuse * uColorDiffuse + uColorSpecular * specular);
    }
    // Linear lighting -> display gamma; framebuffer sRGB conversion is disabled.
    fragColor = vec4(pow(clamp(lit, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
