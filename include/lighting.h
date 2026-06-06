#ifndef LIGHTING_H
#define LIGHTING_H

#include <glm/glm.hpp>

#include <vector>

struct DirectionalLightConfig
{
    glm::vec3 Direction = glm::normalize(glm::vec3(-0.35f, -0.9f, -0.25f));
    glm::vec3 Color = glm::vec3(1.0f, 0.94f, 0.84f);
    float Intensity = 3.0f;
    bool SyncWithSkyboxRotation = true;
    float SkyboxRotationOffset = 0.0f;
    float AngularSize = 0.05f;
};

struct PointLightConfig
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    float Range = 8.0f;
};

struct LightingConfig
{
    DirectionalLightConfig MainLight;
    glm::vec3 AmbientColor = glm::vec3(1.0f);
    float AmbientIntensity = 1.0f;
    float IBLDiffuseIntensity = 1.0f;
    float IBLSpecularIntensity = 1.0f;
    std::vector<PointLightConfig> PointLights;
};

struct PostProcessConfig
{
    float Gamma = 2.2f;
    bool PixelateEnabled = false;
    float PixelSize = 4.0f;
    bool UnderwaterEnabled = false;
};

#endif
