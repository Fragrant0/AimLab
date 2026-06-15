#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

#include "lighting.h"

enum class TerrainType
{
    Flat,
    Heightmap
};

struct HeightmapConfig
{
    std::string Path;
    float HeightScale = 5.0f;
    int GridSize = 100;
    float Size = 20.0f;
};

enum class SkyboxType
{
    Cubemap,
    HDR
};

struct SkyboxConfig
{
    SkyboxType Type = SkyboxType::Cubemap;
    std::vector<std::string> Faces;
    std::string HDRPath;
};

struct PropConfig
{
    std::string ModelPath;
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Rotation = glm::vec3(0.0f);
    glm::vec3 Scale = glm::vec3(1.0f);
    float TargetHeight = 0.0f;
    float PBRBrightness = 1.0f;
    float PBRAmbientIntensity = 0.35f;
    glm::vec3 PBRAmbientColor = glm::vec3(1.0f);
};

struct TargetAreaConfig
{
    float WallDistance = 15.0f;
    float WallWidth = 12.0f;
    float WallHeight = 8.0f;
    float WallCenterY = 2.5f;
    float Lifetime = 5.0f;
};

struct MapConfig
{
    std::string Name;
    SkyboxConfig Skybox;
    std::string FloorTexturePath;
    std::string FloorTextureName;
    glm::vec3 AmbientColor = glm::vec3(1.0f);
    float AmbientIntensity = 1.0f;
    LightingConfig Lighting;
    PostProcessConfig PostProcess;
    float SpawnRadius = 15.0f;
    TerrainType Terrain = TerrainType::Flat;
    HeightmapConfig Heightmap;
    bool EcologyEnabled = false;
    std::vector<PropConfig> Props;
    glm::vec3 PlayerStartPos = glm::vec3(0.0f, 1.0f, 3.0f);
    TargetAreaConfig TargetArea;
};

class MapManager
{
public:
    MapManager();
    ~MapManager();

    void Initialize();

    int GetCurrentMapIndex() const;
    const MapConfig& GetCurrentMap() const;
    const MapConfig& GetMap(int index) const;
    int GetMapCount() const;

    void SwitchToMap(int index);

    const std::string& GetCurrentFloorTextureName() const;
    glm::vec3 GetCurrentAmbientLight() const;
    const LightingConfig& GetCurrentLighting() const;
    const PostProcessConfig& GetCurrentPostProcess() const;

    bool IsCurrentMapTerrain() const;
    const HeightmapConfig& GetCurrentHeightmapConfig() const;
    glm::vec3 GetCurrentPlayerStartPos() const;

private:
    std::vector<MapConfig> m_Maps;
    int m_CurrentMapIndex;

    void SetupDefaultMaps();
    void LoadMapsFromJson(const std::string& path);
};

#endif
