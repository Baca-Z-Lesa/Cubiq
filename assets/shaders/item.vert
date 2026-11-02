#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;

uniform mat4 uViewProj;

out vec2 vUV;
out float vShade;
out vec3 vWorldPos;

void main()
{
    vUV = aUV;
    vShade = aShade;
    vWorldPos = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
