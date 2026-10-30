#version 330 core
in vec2 vUV;
in float vShade;
in vec4 vLight;
in vec3 vWorldPos;
in float vDistance;
flat in int vLava;

uniform sampler2D uAtlas;
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

void main()
{
    vec4 tex = texture(uAtlas, vUV);
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
        vec2 p = vWorldPos.xz;
        float dx = 0.10 * cos(p.x * 0.8 + uTime * 1.6) + 0.06 * cos((p.x + p.y) * 1.7 + uTime * 2.3)
                 + 0.04 * cos(p.x * 3.1 - uTime * 2.9);
        float dz = 0.10 * cos(p.y * 0.6 + uTime * 1.2) + 0.06 * cos((p.x + p.y) * 1.7 + uTime * 2.3)
                 + 0.04 * cos(p.y * 2.7 + uTime * 3.3);
        n = normalize(vec3(-dx, sign(ng.y), -dz));
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
