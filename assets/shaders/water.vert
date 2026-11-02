#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in vec3 aTint;   // pre tekutiny: x > 1.5 = láva
layout(location = 4) in vec4 aLight;
layout(location = 5) in float aTile;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out vec2 vUV;
flat out float vTile;
out float vShade;
out vec4 vLight;
out vec3 vWorldPos;
out float vDistance;
flat out int vLava;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vLava = aTint.x > 1.5 ? 1 : 0;

    // Vlnenie hladiny: len vrcholy na hladine (y = .875 v rámci bloku), nie dno ani boky pod ňou.
    if (vLava == 0 && abs(fract(aPos.y) - 0.875) < 0.01) {
        world.y += 0.05 * sin(world.x * 0.8 + uTime * 1.6) * cos(world.z * 0.6 + uTime * 1.2)
                 + 0.03 * sin((world.x + world.z) * 1.7 + uTime * 2.3);
    }

    vUV = aUV;
    vTile = aTile;
    vShade = aShade;
    vLight = aLight;
    vWorldPos = world.xyz;
    vec4 viewPos = uView * world;
    vDistance = length(viewPos.xyz);
    gl_Position = uProjection * viewPos;
}
