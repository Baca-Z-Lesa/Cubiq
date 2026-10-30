#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out float vShade;

void main()
{
    vUV = aUV;
    vShade = aShade;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
