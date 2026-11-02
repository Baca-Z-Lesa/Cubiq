#version 330 core
in vec2 vUV;
in float vShade;
in vec3 vWorldPos;

uniform sampler2D uAtlas;
uniform vec3 uSunColor;
uniform vec3 uCameraPos;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

void main()
{
    vec4 tex = texture(uAtlas, vUV);
    if (tex.a < 0.5)
        discard;
    // Tónované pixely (tráva, listy) dostanú typickú zelenú ako v ikonách.
    if (abs(tex.a - 254.0 / 255.0) < 0.002)
        tex.rgb *= vec3(0.48, 0.74, 0.30);

    vec3 col = tex.rgb * vShade * mix(vec3(1.0), uSunColor, 0.4);
    float dist = length(vWorldPos - uCameraPos);
    float fog = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(col, uFogColor, fog), 1.0);
}
