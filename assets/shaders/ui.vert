#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTextured;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;
out float vTextured;

void main()
{
    vUV = aUV;
    vColor = aColor;
    vTextured = aTextured;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
