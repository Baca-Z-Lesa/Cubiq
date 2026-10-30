#version 330 core
// Jeden trojuholník cez celú obrazovku, vrcholy z gl_VertexID (bez bufferu).
out vec2 vNdc;

void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
