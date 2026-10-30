#version 330 core
in vec2 vUV;
in float vShade;
in vec3 vTint;
in vec4 vLight;
in float vDistance;

uniform sampler2D uAtlas;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uSunBrightness; // 1 = deň (v F10 sa bude meniť s časom)

out vec4 FragColor;

void main()
{
    vec4 color = texture(uAtlas, vUV);
    if (color.a < 0.1)
        discard; // dierky v listoch a v skle

    // Alpha 254 v atlase = pixel sa tónuje farbou biómu.
    if (abs(color.a - 254.0 / 255.0) < 0.002) {
        color.rgb *= vTint;
        color.a = 1.0;
    }

    // Svetlo: slnko (biele, mierne teplé) vs. farebné svetlo zo zdrojov – berie sa silnejšie po kanáloch.
    vec3 sun = vec3(1.0, 0.98, 0.94) * vLight.x * uSunBrightness;
    vec3 light = max(sun, vLight.yzw);
    vec3 lit = color.rgb * vShade * light;

    // Hmla skryje okraj načítaného sveta.
    float fog = clamp((vDistance - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(lit, uFogColor, fog), color.a);
}
