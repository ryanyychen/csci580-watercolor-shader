// curvature.vert
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in float aK1;
layout(location = 3) in float aK2;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out float vK1;
out float vK2;

void main()
{
    vK1 = aK1;
    vK2 = aK2;

    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}