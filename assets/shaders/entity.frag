#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;

uniform vec3 uColor;
uniform vec3 uLight;     // svetlo oblohy v mieste zvieraťa
uniform vec3 uSunDir;
uniform float uEmissive; // 1 = svieti sama (oči mátohy)
uniform float uHurt;     // 1 = práve zasiahnuté (červené bliknutie)
uniform vec3 uCameraPos;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

void main()
{
    // Rovnaký charakter ako bloky: vrch najsvetlejší, boky tmavšie, spodok najtmavší.
    vec3 n = normalize(vNormal);
    float shade = n.y > 0.5 ? 1.0 : n.y < -0.5 ? 0.5 : (abs(n.x) > 0.5 ? 0.62 : 0.8);
    shade += 0.15 * max(dot(n, uSunDir), 0.0);

    vec3 col = uColor * shade * uLight;
    col = mix(col, uColor * 1.4, uEmissive);
    col = mix(col, vec3(0.9, 0.15, 0.1), uHurt * 0.55);

    float dist = length(vWorldPos - uCameraPos);
    float fog = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(col, uFogColor, fog), 1.0);
}
