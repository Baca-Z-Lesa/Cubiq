#version 330 core
// Veľký štvorec vo výške mrakov, vždy vycentrovaný nad kamerou (vrcholy z gl_VertexID).
uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform float uHeight;
uniform float uExtent;

out vec3 vWorldPos;

void main()
{
    const vec2 corners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
    vec2 c = corners[gl_VertexID] * uExtent;
    vWorldPos = vec3(uCameraPos.x + c.x, uHeight, uCameraPos.z + c.y);
    gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
