#version 330 core
in vec2 vUV;
in vec4 vColor;
in float vTextured;

uniform sampler2D uAtlas;

out vec4 FragColor;

void main()
{
    vec4 color = vColor;
    if (vTextured > 0.5)
        color *= texture(uAtlas, vUV);
    FragColor = color;
}
