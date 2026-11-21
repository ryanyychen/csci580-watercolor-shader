#version 330 core

in vec2 paperUV;
uniform sampler2D paperTexture;
uniform float uPaperStrength;

out vec4 fragColor;

void main()
{
    float height = texture(paperTexture, paperUV).r;
    float grain = smoothstep(0.15, 0.85, height);
    vec3 paperColor = vec3(0.98, 0.96, 0.90) *
                      (0.72 + 0.28 * grain);
    fragColor = vec4(paperColor, uPaperStrength);
}