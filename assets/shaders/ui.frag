#version 330 core
in vec2 vUV;
in vec4 vColor;
in float vTextured;

uniform sampler2D uAtlas;

out vec4 FragColor;

void main()
{
    vec4 color = vColor;
    if (vTextured > 0.5) {
        vec4 tex = texture(uAtlas, vUV);
        if (tex.a < 0.1)
            discard;
        // Tónované pixely (tráva, listy) v ikonách dostanú typickú zelenú.
        if (abs(tex.a - 254.0 / 255.0) < 0.002)
            tex = vec4(tex.rgb * vec3(0.48, 0.74, 0.30), 1.0);
        color *= tex;
    }
    FragColor = color;
}
