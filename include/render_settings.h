#ifndef RENDER_SETTINGS_H
#define RENDER_SETTINGS_H

namespace RenderSettings
{
    constexpr unsigned int kShadowMapSize = 2048;
    constexpr float kSkyboxRotationSpeed = 0.008f;
    constexpr float kDefaultShadowBias = 0.0022f;
}

namespace TextureUnit
{
    constexpr int Albedo = 0;
    constexpr int Normal = 1;
    constexpr int Metallic = 2;
    constexpr int Roughness = 3;
    constexpr int AO = 4;
    constexpr int ARM = 5;
    constexpr int Irradiance = 6;
    constexpr int Prefilter = 7;
    constexpr int BRDFLUT = 8;
    constexpr int Emissive = 9;
    constexpr int Shadow = 10;
}

#endif
