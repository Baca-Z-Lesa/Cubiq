#version 330 core
in vec2 vUV;
flat in float vTile;
in float vShade;
in vec4 vLight;
in vec3 vWorldPos;
in float vDistance;
flat in int vLava;

uniform sampler2D uAtlas;
uniform float uAtlasTiles;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunBrightness;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uTime;

out vec4 FragColor;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

// Výška vĺn: 4 vrstvy šumu, každá pootočená a plynúca iným smerom a rýchlosťou,
// takže sa nevytvára pravidelný pás ani mriežka.
float waveHeight(vec2 p, float t)
{
    const mat2 R = mat2(0.8, -0.6, 0.6, 0.8);
    float h = 0.0, amp = 0.5;
    vec2 drift = vec2(0.12, 0.05);
    for (int i = 0; i < 4; ++i) {
        h += amp * vnoise(p + drift * t * (1.0 + float(i) * 0.7));
        p = R * p * 2.03 + vec2(3.7, -1.9);
        drift = R * drift * -1.3;
        amp *= 0.5;
    }
    return h;
}

void main()
{
    // UV v blokoch → súradnica v atlase (textúra sa opakuje po zlúčených stenách)
    vec2 f = clamp(fract(vUV), 0.5 / 16.0, 1.0 - 0.5 / 16.0);
    vec4 tex = texture(uAtlas, vec2((vTile + f.x) / uAtlasTiles, f.y));
    vec3 sun = uSunColor * vLight.x * uSunBrightness;
    vec3 light = max(sun, vLight.yzw);
    float fog = clamp((vDistance - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);

    if (vLava == 1) {
        // Láva: nepriehľadná, svieti sama a pomaly pulzuje.
        float pulse = 0.9 + 0.1 * sin(uTime * 1.5 + vWorldPos.x * 0.7 + vWorldPos.z * 0.5);
        FragColor = vec4(mix(tex.rgb * pulse, uFogColor, fog), 1.0);
        return;
    }

    // Geometrická normála z derivácií pozície; na hladine ju rozvlníme.
    vec3 ng = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    vec3 n = ng;
    if (abs(ng.y) > 0.5) {
        // Normála z derivácie výšky vĺn (konečné diferencie).
        vec2 p = vWorldPos.xz * 0.6;
        const float e = 0.08;
        float h0 = waveHeight(p, uTime);
        float dx = (waveHeight(p + vec2(e, 0.0), uTime) - h0) / e;
        float dz = (waveHeight(p + vec2(0.0, e), uTime) - h0) / e;
        n = normalize(vec3(-dx * 0.22, sign(ng.y), -dz * 0.22));
    }

    vec3 v = normalize(uCameraPos - vWorldPos);
    if (dot(n, v) < 0.0) n = -n; // pohľad zospodu (pod vodou)

    // Fresnel: pri šikmom pohľade voda viac odráža oblohu.
    float fresnel = 0.03 + 0.97 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
    vec3 r = reflect(-v, n);
    vec3 sky = mix(uHorizon, uZenith, clamp(r.y, 0.0, 1.0));

    vec3 base = tex.rgb * vShade * light;
    vec3 col = mix(base, sky, fresnel * 0.8);

    // Odlesk slnka na vlnách.
    float spec = pow(max(dot(r, uSunDir), 0.0), 180.0) * smoothstep(-0.05, 0.1, uSunDir.y);
    col += uSunColor * spec * 2.5 * vLight.x;

    float alpha = mix(0.62, 0.95, fresnel);
    FragColor = vec4(mix(col, uFogColor, fog), alpha);
}
