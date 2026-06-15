#version 330 core
out vec4 FragColor;

in vec3 vertexColor;

uniform float alpha;

void main()
{
    FragColor = vec4(vertexColor, alpha);
}
