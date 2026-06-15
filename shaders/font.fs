#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 VertexColor;

uniform sampler2D fontAtlas;

void main()
{
    float alpha = texture(fontAtlas, TexCoord).r;
    FragColor = vec4(VertexColor.rgb, VertexColor.a * alpha);
}
