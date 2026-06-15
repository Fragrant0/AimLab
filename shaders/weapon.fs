#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 weaponColor;
uniform vec3 viewPos;
uniform float shininess;

// 光照参数 - 可配置
uniform vec3 lightDir = vec3(-0.3, 1.0, 0.3);
uniform vec3 ambientLight = vec3(0.1, 0.1, 0.12);
uniform float specularStrength = 0.8;
uniform vec3 specularColor = vec3(0.8, 0.8, 0.9);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDirection = normalize(lightDir);
    
    // 漫反射
    float diff = max(dot(norm, lightDirection), 0.0);
    
    // 镜面反射 (Blinn-Phong)
    vec3 halfDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
    
    // 环境光
    vec3 ambient = weaponColor * ambientLight;
    
    // 漫反射
    vec3 diffuse = weaponColor * diff * 1.3;
    
    // 镜面反射
    vec3 specular = specularColor * spec * specularStrength;
    
    
    vec3 result = ambient + diffuse + specular;
    
    
    FragColor = vec4(result, 1.0);
}
