#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in mat3 TBN;
in vec4 FragPosLightSpace;

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

uniform DirectionalLight u_MainLight;
uniform int u_PointLightCount;
uniform vec3 u_PointLightPositions[8];
uniform vec3 u_PointLightColors[8];
uniform float u_PointLightIntensities[8];
uniform float u_PointLightRanges[8];

uniform vec3 cameraPos;
uniform vec3 u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_IBLDiffuseIntensity;
uniform float u_IBLSpecularIntensity;
uniform float u_ModelBrightness;
uniform float u_ModelAmbientIntensity;
uniform vec3 u_ModelAmbientColor;

uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_ARMMap;
uniform sampler2D u_EmissiveMap;

uniform bool u_HasAlbedoMap;
uniform bool u_HasNormalMap;
uniform bool u_HasMetallicMap;
uniform bool u_HasRoughnessMap;
uniform bool u_HasAOMap;
uniform bool u_HasARMMap;
uniform bool u_HasEmissiveMap;

uniform sampler2D u_ShadowMap;
uniform bool u_ShadowsEnabled;
uniform bool u_PCSSEnabled;
uniform float u_LightSize;
uniform float u_ShadowBias;

const float PI = 3.14159265359;
const float PCF_FILTER_RADIUS = 1.25;
const float PCSS_MIN_FILTER_RADIUS = 2.75;
const float PCSS_MAX_FILTER_RADIUS = 32.0;

vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float SearchBlocker(vec3 projCoords, float receiverDepth, float searchRadius)
{
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    float blockerDepth = 0.0;
    float blockerCount = 0.0;

    for (int i = 0; i < 16; ++i)
    {
        float sampleDepth = texture(u_ShadowMap, projCoords.xy + poissonDisk[i] * texelSize * searchRadius).r;
        if (sampleDepth < receiverDepth)
        {
            blockerDepth += sampleDepth;
            blockerCount += 1.0;
        }
    }

    if (blockerCount < 0.5)
        return -1.0;
    return blockerDepth / blockerCount;
}

float PCF(vec3 projCoords, float receiverDepth, float filterRadius)
{
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    float shadow = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        float pcfDepth = texture(u_ShadowMap, projCoords.xy + poissonDisk[i] * texelSize * filterRadius).r;
        shadow += receiverDepth > pcfDepth ? 1.0 : 0.0;
    }
    return shadow / 16.0;
}

float CalculatePCSSShadow(vec3 N, vec3 L)
{
    if (!u_ShadowsEnabled)
        return 0.0;

    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(u_ShadowBias * (1.0 - dot(N, L)), u_ShadowBias * 0.35);
    float receiverDepth = projCoords.z - bias;
    if (!u_PCSSEnabled)
        return PCF(projCoords, receiverDepth, PCF_FILTER_RADIUS);

    float searchRadius = max(u_LightSize * 220.0, 4.0);
    float avgBlockerDepth = SearchBlocker(projCoords, receiverDepth, searchRadius);
    if (avgBlockerDepth < 0.0)
        return 0.0;

    float penumbra = max(receiverDepth - avgBlockerDepth, 0.0) / max(avgBlockerDepth, 0.0001);
    float lightRadius = max(u_LightSize * 900.0, 6.0);
    float filterRadius = clamp(PCSS_MIN_FILTER_RADIUS + penumbra * lightRadius,
                               PCSS_MIN_FILTER_RADIUS,
                               PCSS_MAX_FILTER_RADIUS);
    return PCF(projCoords, receiverDepth, filterRadius);
}

vec3 CalculateRadiance(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 radiance)
{
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main()
{
    vec3 albedo = u_HasAlbedoMap ? texture(u_AlbedoMap, TexCoords).rgb : u_AlbedoColor;
    albedo = max(albedo, vec3(0.0));

    vec3 arm = u_HasARMMap ? texture(u_ARMMap, TexCoords).rgb : vec3(u_AO, u_Roughness, u_Metallic);
    float ao = u_HasAOMap ? texture(u_AOMap, TexCoords).r : arm.r;
    float roughness = u_HasRoughnessMap ? texture(u_RoughnessMap, TexCoords).r : arm.g;
    float metallic = u_HasMetallicMap ? texture(u_MetallicMap, TexCoords).r : arm.b;
    roughness = clamp(roughness, 0.04, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    ao = clamp(ao, 0.0, 1.0);

    vec3 N = normalize(Normal);
    if (u_HasNormalMap)
    {
        vec3 tangentNormal = texture(u_NormalMap, TexCoords).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(cameraPos - FragPos);
    vec3 Lo = vec3(0.0);

    vec3 mainL = normalize(-u_MainLight.direction);
    float shadow = CalculatePCSSShadow(N, mainL);
    Lo += CalculateRadiance(albedo, metallic, roughness, N, V, mainL,
                            u_MainLight.color * u_MainLight.intensity) * (1.0 - shadow);

    for (int i = 0; i < min(u_PointLightCount, 8); ++i)
    {
        vec3 lightVec = u_PointLightPositions[i] - FragPos;
        float distance = length(lightVec);
        vec3 L = lightVec / max(distance, 0.0001);
        float attenuation = clamp(1.0 - distance / max(u_PointLightRanges[i], 0.001), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 radiance = u_PointLightColors[i] * u_PointLightIntensities[i] * attenuation;
        Lo += CalculateRadiance(albedo, metallic, roughness, N, V, L, radiance);
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo * u_IBLDiffuseIntensity;

    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * u_IBLSpecularIntensity;

    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;
    vec3 baseAmbient = albedo * u_ModelAmbientColor * u_ModelAmbientIntensity * mix(1.0, ao, 0.35);
    vec3 emissive = u_HasEmissiveMap ? texture(u_EmissiveMap, TexCoords).rgb : vec3(0.0);
    vec3 color = (ambient + baseAmbient + Lo + emissive) * u_ModelBrightness;

    FragColor = vec4(color, 1.0);
}
