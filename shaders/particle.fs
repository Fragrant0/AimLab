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

    // Smooth radial gradient for a glowing particle
    float t = dist / 0.5; // 0.0 at center, 1.0 at edge
    
    // Color interpolation: bright white-yellow core, transitioning to rich orange-red spark color
    vec3 coreColor = vec3(1.0, 0.95, 0.85);
    vec3 baseColor = ParticleColor.rgb;
    
    // Smooth transition from core to base color
    vec3 color = mix(coreColor, baseColor, smoothstep(0.1, 0.45, dist));
    
    // Glow falloff function: steep drop close to center, soft tail
    float coreGlow = exp(-dist * 12.0) * 2.0; 
    float outerGlow = pow(1.0 - t, 2.5);
    float glow = outerGlow + coreGlow;
    
    // Calculate final alpha
    float alpha = outerGlow * 1.2; // fade out to 0 at the edge
    
    // Additive output
    FragColor = vec4(color * glow * 1.5, ParticleColor.a * alpha);
}