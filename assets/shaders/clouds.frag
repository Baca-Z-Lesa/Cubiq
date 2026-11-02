#version 330 core
// Volumetrické mraky: lúč z kamery prejde vrstvou mrakov (uBottom..uTop), po krokoch sa
// sčíta hustota z 3D šumu a svetlo od slnka (aj s tieňom, ktorý si mrak vrhá sám na seba).
in vec2 vNdc;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform sampler3D uNoise;   // R = tvar (nízka frekvencia), G = detail (vysoká frekvencia), opakuje sa
uniform float uTime;
uniform float uBottom;
uniform float uTop;
uniform float uMaxDistance;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunBrightness;
uniform vec3 uZenith;
uniform vec3 uHorizon;

out vec4 FragColor;

const int STEPS = 40;

float density(vec3 p)
{
    vec3 wind = vec3(uTime * 3.0, 0.0, uTime * 1.2);
    vec3 q = p + wind;
    // Pokrytie: veľké oblasti s mrakmi a bez nich.
    float coverage = texture(uNoise, vec3(q.xz * 0.00035, 0.3)).r;
    float shape = texture(uNoise, q * vec3(0.0032, 0.006, 0.0032)).r;
    float detail = texture(uNoise, q * 0.014 + vec3(0.0, uTime * 0.01, 0.0)).g;

    // Profil výšky: spodok rovnejší, vrch zaoblený (kopovitý mrak).
    float h = clamp((p.y - uBottom) / (uTop - uBottom), 0.0, 1.0);
    float profile = smoothstep(0.0, 0.12, h) * smoothstep(1.0, 0.45, h);

    float d = shape * 0.65 + coverage * 0.75 - 0.74;
    d = d * profile - detail * 0.12;
    return clamp(d * 3.5, 0.0, 1.0);
}

// Henyey-Greenstein: svetlo sa v mrakoch rozptyľuje hlavne dopredu (žiarivé okraje proti slnku).
float phase(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159 * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main()
{
    vec4 far = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - uCameraPos);

    // Prienik lúča s vrstvou mrakov.
    float t0, t1;
    if (abs(dir.y) < 1e-4) {
        if (uCameraPos.y < uBottom || uCameraPos.y > uTop) discard;
        t0 = 0.0;
        t1 = uMaxDistance;
    } else {
        float a = (uBottom - uCameraPos.y) / dir.y, b = (uTop - uCameraPos.y) / dir.y;
        t0 = max(min(a, b), 0.0);
        t1 = max(a, b);
    }
    t1 = min(t1, uMaxDistance);
    if (t1 <= t0) discard;

    float stepLen = (t1 - t0) / float(STEPS);
    float t = t0 + stepLen * hash(gl_FragCoord.xy); // posun proti pruhom
    float transmittance = 1.0;
    vec3 color = vec3(0.0);

    float cosTheta = dot(dir, uSunDir);
    float ph = mix(phase(cosTheta, 0.6), phase(cosTheta, -0.25), 0.4) * 6.0 + 0.4;
    vec3 sun = uSunColor * uSunBrightness;
    vec3 ambient = mix(uHorizon, uZenith, 0.5) * 0.9;

    for (int i = 0; i < STEPS; ++i) {
        vec3 p = uCameraPos + dir * t;
        float d = density(p);
        if (d > 0.01) {
            // Tieň: koľko mraku je medzi týmto bodom a slnkom (2 vzorky).
            float toSun = density(p + uSunDir * 10.0) + density(p + uSunDir * 28.0);
            float shadow = exp(-toSun * 1.8);
            float hFrac = clamp((p.y - uBottom) / (uTop - uBottom), 0.0, 1.0);
            vec3 lit = sun * shadow * ph + ambient * (0.45 + 0.55 * hFrac);

            float absorb = 1.0 - exp(-d * stepLen * 0.09);
            color += transmittance * absorb * lit;
            transmittance *= 1.0 - absorb;
            if (transmittance < 0.02) break;
        }
        t += stepLen;
    }

    float alpha = 1.0 - transmittance;
    if (alpha < 0.005) discard;

    // V diaľke mraky splývajú s oblohou pri obzore.
    float fade = smoothstep(uMaxDistance * 0.55, uMaxDistance, t0);
    color = mix(color, uHorizon * alpha, fade);
    alpha *= 1.0 - fade * 0.6;

    // Premultiplied alpha (blend ONE, ONE_MINUS_SRC_ALPHA).
    FragColor = vec4(color, alpha);
}
