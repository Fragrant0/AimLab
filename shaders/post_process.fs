#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform float exposure;
uniform float gammaValue;
uniform float contrast;
uniform float saturation;
uniform float vignette;
uniform float chromaticAberration;
uniform float filmGrain;
uniform float bloomIntensity;
uniform float timeSeconds;

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec2 uv = TexCoords;
    vec2 centered = uv - 0.5;

    vec3 color;
    if (chromaticAberration > 0.00001)
    {
        vec2 offset = centered * chromaticAberration;
        color.r = texture(sceneTexture, uv + offset).r;
        color.g = texture(sceneTexture, uv).g;
        color.b = texture(sceneTexture, uv - offset).b;
    }
    else
    {
        color = texture(sceneTexture, uv).rgb;
    }

    vec3 bloom = min(texture(bloomTexture, uv).rgb, vec3(8.0));
    color += bloom * bloomIntensity;

    color *= exposure;
    color = ACESFilm(color);

    color = mix(vec3(dot(color, vec3(0.299, 0.587, 0.114))), color, saturation);
    color = (color - 0.5) * contrast + 0.5;

    float vignetteMask = smoothstep(0.85, 0.2, length(centered));
    color *= mix(1.0 - vignette, 1.0, vignetteMask);

    if (filmGrain > 0.00001)
    {
        float grain = Hash(uv * vec2(1920.0, 1080.0) + timeSeconds) - 0.5;
        color += grain * filmGrain;
    }

    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / max(gammaValue, 0.001)));
    FragColor = vec4(color, 1.0);
}
