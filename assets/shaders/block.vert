#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in vec3 aTint;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out float vShade;
out vec3 vTint;
out float vDistance;

void main()
{
    vUV = aUV;
    vShade = aShade;
    vTint = aTint;
    vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
    vDistance = length(viewPos.xyz);
    gl_Position = uProjection * viewPos;
}
