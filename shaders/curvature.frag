// curvature.frag
#version 330 core

in float vK1;
in float vK2;
out vec4 fragColor;

void main()
{
    float curvature = max(abs(vK1), abs(vK2));
    float t = curvature / (1.0 + curvature);

    vec3 lowCurvature = vec3(0.05, 0.15, 0.9);
    vec3 highCurvature = vec3(1.0, 1.0, 0.98);
    fragColor = vec4(mix(lowCurvature, highCurvature, t), 1.0);
}