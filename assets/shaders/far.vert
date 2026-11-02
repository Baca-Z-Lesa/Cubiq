#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vColor;
out vec3 vNormal;

void main()
{
    vWorldPos = aPos;
    vColor = aColor;
    vNormal = aNormal;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
