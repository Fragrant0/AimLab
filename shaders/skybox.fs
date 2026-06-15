#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform bool isHDR;
uniform float exposure;

void main()
{    
    vec3 skyColor = texture(skybox, TexCoords).rgb;

    if (isHDR)
    {
        FragColor = vec4(skyColor * exposure, 1.0);
    }
    else
    {
        FragColor = vec4(pow(skyColor, vec3(2.2)), 1.0);
    }
}
