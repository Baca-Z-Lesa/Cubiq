#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in vec3 aTint;
layout(location = 4) in vec4 aLight;
layout(location = 5) in float aTile;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
flat out float vTile;
out float vShade;
out vec3 vTint;
out vec4 vLight;
out float vDistance;

void main()
{
    vUV = aUV;
    vTile = aTile;
    vShade = aShade;
    vTint = aTint;
    vLight = aLight;
    vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
    vDistance = length(viewPos.xyz);
    gl_Position = uProjection * viewPos;
}
