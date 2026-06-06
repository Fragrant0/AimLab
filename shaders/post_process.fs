#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float gammaValue;
uniform float timeSeconds;

uniform bool pixelateEnabled;
uniform float pixelSize;
uniform vec2 resolution;

uniform bool underwaterEnabled;

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

    if (underwaterEnabled)
    {
        uv.x += sin(uv.y * 15.0 + timeSeconds * 2.0) * 0.005;
        uv.y += cos(uv.x * 15.0 + timeSeconds * 2.0) * 0.005;
    }

    if (pixelateEnabled && pixelSize > 1.0)
    {
        vec2 size = resolution / pixelSize;
        uv = floor(uv * size) / size;
    }

    vec3 color = texture(sceneTexture, uv).rgb;

    if (underwaterEnabled)
    {
        // Red light is absorbed first under water (absorption rates: Red > Green > Blue)
        color.r *= 0.3;
        color.g *= 0.7;
        color.b *= 0.9;
        
        // Add a blue-green underwater tint
        vec3 waterColor = vec3(0.0, 0.2, 0.4);
        color = mix(color, waterColor, 0.4);
    }

    // --- Color Grading & Tone Mapping ---
    // 1. Exposure scaling (cinematic default)
    color *= 0.62;

    // 2. ACES Film Tone Mapping
    color = ACESFilm(color);

    // 3. Saturation correction (cinematic default: 1.02)
    color = mix(vec3(dot(color, vec3(0.299, 0.587, 0.114))), color, 1.02);

    // 4. Contrast correction (cinematic default: 1.03)
    color = (color - 0.5) * 1.03 + 0.5;

    // 5. Vignette (cinematic default: 0.08)
    vec2 centered = uv - 0.5;
    float vignetteMask = smoothstep(0.85, 0.2, length(centered));
    color *= mix(1.0 - 0.08, 1.0, vignetteMask);

    // Gamma correction
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / max(gammaValue, 0.001)));
    FragColor = vec4(color, 1.0);
}
