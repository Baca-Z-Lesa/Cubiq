#version 330 core
in vec2 vUV;
in float vShade;

uniform sampler2D uAtlas;

out vec4 FragColor;

void main()
{
    vec4 color = texture(uAtlas, vUV);
    FragColor = vec4(color.rgb * vShade, color.a);
}
