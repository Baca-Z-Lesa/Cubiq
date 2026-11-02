#version 330 core
in vec3 vWorldPos;
in vec3 vColor;
in vec3 vNormal;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunBrightness;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uNearRadius; // vnútri kreslia skutočné chunky

out vec4 FragColor;

void main()
{
    float horizontal = length(vWorldPos.xz - uCameraPos.xz);
    if (horizontal < uNearRadius)
        discard;

    // Rovnaký charakter svetla ako bloky: zhora jasno, boky tmavšie, k tomu slnko.
    vec3 n = normalize(vNormal);
    float sky = 0.55 + 0.45 * n.y;
    float sun = max(dot(n, uSunDir), 0.0) * smoothstep(-0.05, 0.15, uSunDir.y);
    vec3 light = uSunColor * uSunBrightness * (sky * 0.8 + sun * 0.3);
    vec3 col = vColor * light;

    float dist = length(vWorldPos - uCameraPos);
    float fog = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(col, uFogColor, fog), 1.0);
}
