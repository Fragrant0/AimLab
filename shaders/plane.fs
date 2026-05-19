#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

uniform sampler2D texture1;
uniform vec3 ambientLight;
uniform vec3 viewPos;
uniform vec3 u_MainLightDirection;
uniform vec3 u_MainLightColor;
uniform float u_MainLightIntensity;
uniform sampler2D u_ShadowMap;
uniform bool u_ShadowsEnabled;
uniform bool u_PCSSEnabled;
uniform float u_LightSize;
uniform float u_ShadowBias;

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
    return blockerCount < 0.5 ? -1.0 : blockerDepth / blockerCount;
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

float CalculateShadow(vec3 N, vec3 L)
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

    float avgBlockerDepth = SearchBlocker(projCoords, receiverDepth, max(u_LightSize * 220.0, 4.0));
    if (avgBlockerDepth < 0.0)
        return 0.0;

    float penumbra = max(receiverDepth - avgBlockerDepth, 0.0) / max(avgBlockerDepth, 0.0001);
    float lightRadius = max(u_LightSize * 900.0, 6.0);
    float filterRadius = clamp(PCSS_MIN_FILTER_RADIUS + penumbra * lightRadius,
                               PCSS_MIN_FILTER_RADIUS,
                               PCSS_MAX_FILTER_RADIUS);
    return PCF(projCoords, receiverDepth, filterRadius);
}

void main()
{
    vec3 baseColor = texture(texture1, TexCoords).rgb;
    vec3 N = normalize(Normal);
    vec3 L = normalize(-u_MainLightDirection);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = CalculateShadow(N, L);

    vec3 ambient = baseColor * ambientLight * 0.45;
    vec3 diffuse = baseColor * u_MainLightColor * u_MainLightIntensity * NdotL * (1.0 - shadow) * 0.65;
    FragColor = vec4(ambient + diffuse, 1.0);
}
