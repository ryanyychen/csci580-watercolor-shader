#version 330 core
// Attribute locations/stride are configured by Mesh when its VAO is created.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 4) in vec2 aTexcoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uNoiseScale;
uniform int uUseMeshTextureCoordinates;
// Rasterization interpolates these world-space values across each triangle.
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vObjectPosition;
out vec3 vObjectNormal;


out vec2 noiseUV;
void main()
{
    // Positions are points (w = 1), so the model transform includes translation.
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vObjectPosition = aPosition;
    vObjectNormal = aNormal;
    // Inverse transpose keeps normals perpendicular under non-uniform scaling.
    vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    // OpenGL clips in homogeneous coordinates, then divides by w for perspective.
    gl_Position = uProjection * uView * world;
    // Mesh positions are normalized to roughly [-1, 1]; keep noise in object space.
    noiseUV = uUseMeshTextureCoordinates != 0
        ? aTexcoord
        : vec2(0.5) + 0.5 * aPosition.xz * uNoiseScale;
}