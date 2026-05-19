#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float threshold;

void main()
{
    vec3 color = max(texture(sceneTexture, TexCoords).rgb, vec3(0.0));
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = max(threshold * 0.5, 0.0001);
    float soft = clamp((brightness - threshold + knee) / (2.0 * knee), 0.0, 1.0);
    float contribution = max(brightness - threshold, 0.0) + soft * soft * knee;
    vec3 bloom = color * (contribution / max(brightness, 0.0001));
    bloom = min(bloom, vec3(8.0));
    FragColor = vec4(bloom, 1.0);
}
