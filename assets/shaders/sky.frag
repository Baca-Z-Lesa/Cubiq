#version 330 core
in vec2 vNdc;

uniform mat4 uInvViewProj;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform float uStars;
uniform float uTime;

out vec4 FragColor;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    vec4 far = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w);
    float h = dir.y;

    // Gradient: obzor → zenit; pod obzorom trochu tmavšie.
    vec3 col = mix(uHorizon, uZenith, pow(clamp(h, 0.0, 1.0), 0.45));
    col = mix(col, uHorizon * 0.7, clamp(-h * 4.0, 0.0, 1.0));

    // Žiara okolo slnka (pri západe sa roztiahne po obzore).
    float sd = dot(dir, uSunDir);
    float sunUp = smoothstep(-0.25, 0.05, uSunDir.y);
    col += uSunColor * pow(max(sd, 0.0), 10.0) * 0.45 * sunUp;
    col += uSunColor * pow(max(sd, 0.0), 3.0) * 0.18 * sunUp * (1.0 - clamp(abs(h) * 3.0, 0.0, 1.0));

    // Slnečný disk
    col = mix(col, vec3(1.0, 0.96, 0.85) * 1.4, smoothstep(0.99935, 0.9997, sd) * sunUp);

    // Mesiac (opačne ako slnko) s jemným svitom
    float md = dot(dir, -uSunDir);
    float moonUp = smoothstep(-0.05, 0.1, -uSunDir.y);
    col += vec3(0.6, 0.65, 0.8) * pow(max(md, 0.0), 40.0) * 0.12 * moonUp;
    col = mix(col, vec3(0.86, 0.88, 0.94), smoothstep(0.99955, 0.9998, md) * moonUp);

    // Hviezdy: smer rozdelíme na malé bunky, v niektorých svieti hviezda a jemne bliká.
    if (uStars > 0.01 && h > 0.0) {
        vec3 d = dir * 220.0;
        vec3 cell = floor(d);
        float r = hash(cell);
        if (r > 0.9965) {
            float dist = length(fract(d) - 0.5);
            float twinkle = 0.65 + 0.35 * sin(uTime * (2.0 + r * 4.0) + r * 120.0);
            col += vec3(smoothstep(0.32, 0.0, dist) * uStars * twinkle * smoothstep(0.0, 0.15, h));
        }
    }

    FragColor = vec4(col, 1.0);
}
