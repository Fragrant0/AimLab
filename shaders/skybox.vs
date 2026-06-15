#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform float rotation;

void main()
{
    float c = cos(rotation);
    float s = sin(rotation);
    vec3 rotated = vec3(
        c * aPos.x - s * aPos.z,
        aPos.y,
        s * aPos.x + c * aPos.z
    );
    TexCoords = rotated;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
