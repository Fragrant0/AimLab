#include "map_resource_loader.h"

#include "resource_manager.h"
#include <stdexcept>

void MapResourceLoader::Load(const MapConfig& map, ModelMap& propModels)
{
    ResourceManager& rm = ResourceManager::GetInstance();

    if (map.FloorTextureName.empty() || map.FloorTexturePath.empty())
        throw std::runtime_error("[MapResources] Missing floor texture in map: " + map.Name);
    if (!rm.LoadTexture(map.FloorTextureName, map.FloorTexturePath.c_str()))
        throw std::runtime_error("[MapResources] Could not load floor texture: " + map.FloorTexturePath);

    unsigned int skybox = 0;
    if (map.Skybox.Type == SkyboxType::HDR)
        skybox = rm.LoadHDRSkybox("skybox", map.Skybox.HDRPath.c_str());
    else
        skybox = rm.LoadCubemap("skybox", map.Skybox.Faces);
    if (!skybox)
        throw std::runtime_error("[MapResources] Could not load skybox in map: " + map.Name);

    for (const PropConfig& prop : map.Props)
    {
        if (prop.ModelPath.empty())
            throw std::runtime_error("[MapResources] Missing prop model path in map: " + map.Name);
        if (propModels.find(prop.ModelPath) == propModels.end())
            propModels.emplace(prop.ModelPath, std::make_unique<Model>(prop.ModelPath));
    }
}
