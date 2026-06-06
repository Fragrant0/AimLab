#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec2 TexCoord;
out float LifeRatio;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float lifeRatio;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;

    TexCoord = aPos.xy / 0.16f + 0.5f;
    LifeRatio = lifeRatio;
}
