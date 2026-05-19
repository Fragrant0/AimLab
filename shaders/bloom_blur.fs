#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform vec2 texelSize;
uniform vec2 direction;
uniform float sampleRadius;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec3 color = texture(image, TexCoords).rgb * weights[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = direction * texelSize * sampleRadius * float(i);
        color += texture(image, TexCoords + offset).rgb * weights[i];
        color += texture(image, TexCoords - offset).rgb * weights[i];
    }

    FragColor = vec4(color, 1.0);
}
