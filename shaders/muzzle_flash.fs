#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in float LifeRatio;

uniform vec3 flashColor;
uniform float intensity;
uniform float time;

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main()
{
    vec2 center = vec2(0.5, 0.5);
    float dist = length(TexCoord - center);

    float circle = 1.0 - smoothstep(0.0, 0.5, dist);

    float n = noise(TexCoord * 8.0 + time * 10.0);
    n += noise(TexCoord * 16.0 - time * 15.0) * 0.5;
    n = n / 1.5;

    float glow = 1.0 - smoothstep(0.2, 0.5, dist);
    glow = pow(glow, 2.0);

    float flicker = 0.8 + 0.4 * random(vec2(time * 20.0, 0.0));
    float alpha = circle * n * glow * (1.0 - LifeRatio) * flicker;

    vec3 coreColor = vec3(1.0, 1.0, 0.9);
    vec3 outerColor = flashColor;
    vec3 finalColor = mix(coreColor, outerColor, clamp(dist * 2.0, 0.0, 1.0));
    finalColor *= intensity * (1.0 + (1.0 - LifeRatio) * 2.0);
    finalColor *= alpha; // Premultiply color by alpha to prevent square artifacts under additive blending

    FragColor = vec4(finalColor, alpha);
}
