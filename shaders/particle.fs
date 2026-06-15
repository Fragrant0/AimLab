#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 ParticleColor;

void main()
{
    vec2 coord = TexCoord - vec2(0.5);
    float dist = length(coord);

    if (dist > 0.5)
        discard;

    float coreRadius = 0.12;
    float midRadius = 0.3;

    float alpha;
    vec3 finalColor;

    if (dist < coreRadius)
    {
        float coreFade = 1.0 - smoothstep(0.0, coreRadius, dist);
        alpha = coreFade * 0.8;
        finalColor = mix(vec3(1.0, 0.8, 0.3), ParticleColor.rgb, dist * 2.0); // ÖÐÐÄÆ«³È»Æ
    }
    else if (dist < midRadius)
    {
        float midFade = 1.0 - smoothstep(coreRadius, midRadius, dist);
        alpha = midFade * 0.6;
        finalColor = ParticleColor.rgb * 0.9;
    }
    else
    {
        float outerFade = 1.0 - smoothstep(midRadius, 0.5, dist);
        alpha = outerFade * 0.3;
        finalColor = ParticleColor.rgb * 0.7;
    }

    FragColor = vec4(finalColor, ParticleColor.a * alpha);
}