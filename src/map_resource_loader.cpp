#include "map_resource_loader.h"

#include "resource_manager.h"

void MapResourceLoader::Load(const MapConfig& map, ModelMap& propModels)
{
    ResourceManager& rm = ResourceManager::GetInstance();

    if (!map.FloorTextureName.empty() && !map.FloorTexturePath.empty())
        rm.LoadTexture(map.FloorTextureName, map.FloorTexturePath.c_str());

    if (map.Skybox.Type == SkyboxType::HDR && !map.Skybox.HDRPath.empty())
        rm.LoadHDRSkybox("skybox", map.Skybox.HDRPath.c_str());
    else
        rm.LoadCubemap("skybox", map.Skybox.Faces);

    for (const PropConfig& prop : map.Props)
    {
        if (!prop.ModelPath.empty() && propModels.find(prop.ModelPath) == propModels.end())
            propModels[prop.ModelPath] = std::make_unique<Model>(prop.ModelPath);
    }
}

void MapResourceLoader::Unload(const MapConfig& map, ModelMap& propModels)
{
    ResourceManager& rm = ResourceManager::GetInstance();

    rm.UnloadTexture(map.FloorTextureName);

    if (map.Skybox.Type == SkyboxType::HDR)
    {
        rm.UnloadHDRSkybox("skybox");
        rm.UnloadHDRTexture("skybox_hdr");
    }
    else
    {
        rm.UnloadCubemap("skybox");
    }

    propModels.clear();
}
