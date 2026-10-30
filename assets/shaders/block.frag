#version 330 core
in vec2 vUV;
in float vShade;
in float vDistance;

uniform sampler2D uAtlas;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

void main()
{
    vec4 color = texture(uAtlas, vUV);
    vec3 lit = color.rgb * vShade;

    // Hmla skryje okraj načítaného sveta.
    float fog = clamp((vDistance - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = vec4(mix(lit, uFogColor, fog), color.a);
}
