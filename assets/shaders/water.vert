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

// Šum hodnôt – nepravidelné vlny bez opakujúceho sa vzoru (na rozdiel od sínusov).
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vLava = aTint.x > 1.5 ? 1 : 0;

    // Vlnenie hladiny: len vrcholy na hladine (y = .875 v rámci bloku), nie dno ani boky pod ňou.
    if (vLava == 0 && abs(fract(aPos.y) - 0.875) < 0.01) {
        vec2 p = world.xz;
        float swell = vnoise(p * 0.18 + vec2(uTime * 0.12, uTime * 0.05))
                    + 0.5 * vnoise(mat2(0.8, -0.6, 0.6, 0.8) * p * 0.45 - vec2(uTime * 0.21, -uTime * 0.17));
        world.y += (swell - 0.75) * 0.12;
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
