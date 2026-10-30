#version 330 core
in vec3 vWorldPos;

uniform vec3 uCameraPos;
uniform float uExtent;
uniform float uTime;
uniform vec3 uSunColor;
uniform float uSunBrightness;
uniform vec3 uHorizon;

out vec4 FragColor;

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

float fbm(vec2 p)
{
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 5; ++i) {
        sum += amp * valueNoise(p);
        p = p * 2.03 + vec2(17.1, 9.2);
        amp *= 0.5;
    }
    return sum;
}

void main()
{
    // Mraky plávajú s vetrom.
    vec2 p = (vWorldPos.xz + vec2(uTime * 2.5, uTime * 0.8)) * 0.0045;
    float n = fbm(p);
    float density = smoothstep(0.44, 0.66, n);
    if (density < 0.01)
        discard;

    // Hustejšie miesta sú zospodu tmavšie (náznak objemu), osvetlené farbou slnka.
    float inner = smoothstep(0.55, 0.85, n);
    vec3 lit = mix(vec3(1.0), uSunColor, 0.35) * mix(1.0, 0.72, inner);
    vec3 col = lit * (0.12 + 0.88 * uSunBrightness);

    // Do diaľky splývajú s oblohou pri obzore.
    float dist = length(vWorldPos.xz - uCameraPos.xz);
    float fade = 1.0 - smoothstep(uExtent * 0.45, uExtent * 0.95, dist);
    col = mix(uHorizon, col, fade);

    FragColor = vec4(col, density * 0.92 * fade);
}
