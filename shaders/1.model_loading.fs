#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_ambient1;
uniform samplerCube skybox;
uniform bool skyboxIsHDR;

void main()
{    
    vec3 diffuseColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 specularColor = texture(texture_specular1, TexCoords).rgb;
    vec3 reflectionColor = texture(texture_ambient1, TexCoords).rgb;

    vec3 I = normalize(Position - cameraPos);
    vec3 R = reflect(I, normalize(Normal));
    vec3 envReflection = texture(skybox, R).rgb;

    vec3 result;
    if(specularColor.r > 0.1)
    {
        vec3 mappedReflection = envReflection;
        if (skyboxIsHDR)
        {
            mappedReflection = vec3(1.0) - exp(-mappedReflection * 1.0);
        }
        specularColor = specularColor * mappedReflection;
        result = diffuseColor + specularColor;
    }
    else
    {
        result = diffuseColor;
    }

    if (skyboxIsHDR)
    {
        result = result / (result + vec3(1.0));
    }
    result = pow(result, vec3(1.0 / 2.2));

    FragColor = vec4(result, 1.0);
}