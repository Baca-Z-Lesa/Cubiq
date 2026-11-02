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
uniform float uNearRadius; // vnútri (štvorec) kreslia skutočné chunky

out vec4 FragColor;

void main()
{
    vec2 d = abs(vWorldPos.xz - uCameraPos.xz);
    float square = max(d.x, d.y); // vzdialenosť "v štvorci" – svet je hranatý ako v Minecrafte
    if (square < uNearRadius)
        discard;

    // Rovnaké tieňovanie strán ako pri blokoch: vrch najsvetlejší, boky tmavšie.
    vec3 n = normalize(vNormal);
    float shade = n.y > 0.5 ? 1.0 : (abs(n.x) > 0.5 ? 0.6 : 0.8);
    float sun = max(dot(n, uSunDir), 0.0) * smoothstep(-0.05, 0.15, uSunDir.y);
    vec3 light = uSunColor * uSunBrightness * (shade * 0.85 + sun * 0.2);
    vec3 col = vColor * light;

    // Hmla podľa štvorcovej vzdialenosti → okraj sveta je hranatý, nie kruh.
    float fog = clamp((square - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(col, uFogColor, fog), 1.0);
}
