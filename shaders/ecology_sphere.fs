#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec3 SphereColor;

out vec4 FragColor;

uniform vec3 viewPos;
uniform vec3 ambientLight;
uniform float alpha;
uniform vec3 lightDir;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float emissionStrength;
uniform float fresnelStrength;
uniform float shininess;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 lightDirection = normalize(lightDir);
    float diff = max(dot(norm, lightDirection), 0.0);

    vec3 reflectDir = reflect(-lightDirection, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    vec3 ambient = SphereColor * ambientStrength * ambientLight;
    vec3 diffuse = SphereColor * diff * diffuseStrength;
    vec3 specular = vec3(1.0) * spec * specularStrength;

    vec3 emission = SphereColor * emissionStrength;

    vec3 result = ambient + diffuse + specular + emission;

    float fresnel = pow(1.0 - max(dot(viewDir, norm), 0.0), 3.0);
    result += SphereColor * fresnel * fresnelStrength;

    FragColor = vec4(result, alpha);
}
