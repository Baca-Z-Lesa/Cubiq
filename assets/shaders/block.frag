#version 330 core
in vec2 vUV;
flat in float vTile;
in float vShade;
in vec3 vTint;
in vec4 vLight;
in float vDistance;

uniform sampler2D uAtlas;
uniform float uAtlasTiles;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uSunBrightness; // 0.06 (noc) .. 1 (deň)
uniform vec3 uSunColor;       // pri západe slnka oranžová

out vec4 FragColor;

// UV v blokoch → súradnica v atlase. Textúra sa opakuje, takže zlúčená stena 5×3 (greedy meshing)
// vyzerá ako 15 samostatných. Polovica texelu od okraja, aby nepresakovala susedná dlaždica.
vec2 atlasUV(vec2 local, float tile)
{
    vec2 f = clamp(fract(local), 0.5 / 16.0, 1.0 - 0.5 / 16.0);
    return vec2((tile + f.x) / uAtlasTiles, f.y);
}

void main()
{
    vec4 color = texture(uAtlas, atlasUV(vUV, vTile));
    if (color.a < 0.1)
        discard; // dierky v listoch a v skle

    // Alpha 254 v atlase = pixel sa tónuje farbou biómu.
    if (abs(color.a - 254.0 / 255.0) < 0.002) {
        color.rgb *= vTint;
        color.a = 1.0;
    }

    // Svetlo: slnko vs. farebné svetlo zo zdrojov – berie sa silnejšie po kanáloch.
    vec3 sun = uSunColor * vLight.x * uSunBrightness;
    vec3 light = max(sun, vLight.yzw);
    vec3 lit = color.rgb * vShade * light;

    // Hmla skryje okraj načítaného sveta.
    float fog = clamp((vDistance - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(lit, uFogColor, fog), color.a);
}
