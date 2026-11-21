#version 330 core

uniform vec3 uLineColor;
out vec4 fragColor;

void main()
{
    fragColor = vec4(uLineColor, 1.0);
}
