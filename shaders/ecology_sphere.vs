#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

// Instance attributes
layout (location = 7) in mat4 aInstanceModel;
layout (location = 11) in vec3 aInstanceColor;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 SphereColor;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(aInstanceModel * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(aInstanceModel))) * aNormal;
    TexCoords = aTexCoords;
    SphereColor = aInstanceColor;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
