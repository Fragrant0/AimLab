#version 330 core
layout (location = 0) in vec2 aQuadPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in vec4 aInstanceColor;
layout (location = 4) in float aInstanceSize;

out vec2 TexCoord;
out vec4 ParticleColor;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoord = aTexCoord;
    ParticleColor = aInstanceColor;

    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 worldPos = aInstancePos
        + cameraRight * aQuadPos.x * aInstanceSize
        + cameraUp * aQuadPos.y * aInstanceSize;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
