#include "map_manager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace
{
    glm::vec3 ReadVec3(const json& value, const glm::vec3& fallback)
    {
        if (!value.is_array() || value.size() < 3)
            return fallback;
        return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
    }

    void SyncLegacyAmbient(MapConfig& map)
    {
        map.Lighting.AmbientColor = map.AmbientColor;
        map.Lighting.AmbientIntensity = map.AmbientIntensity;
    }
}

MapManager::MapManager()
    : m_CurrentMapIndex(0)
{
}

MapManager::~MapManager()
{
}

void MapManager::Initialize()
{
    LoadMapsFromJson("resources/maps_config.json");
    if (m_Maps.empty())
    {
        SetupDefaultMaps();
    }
    m_CurrentMapIndex = m_Maps.empty() ? 0 : std::min(1, static_cast<int>(m_Maps.size()) - 1);
}

int MapManager::GetCurrentMapIndex() const
{
    return m_CurrentMapIndex;
}

const MapConfig& MapManager::GetCurrentMap() const
{
    if (m_Maps.empty())
    {
        static MapConfig defaultMap;
        return defaultMap;
    }
    if (m_CurrentMapIndex < 0 || m_CurrentMapIndex >= static_cast<int>(m_Maps.size()))
        return m_Maps[0];
    return m_Maps[m_CurrentMapIndex];
}

const MapConfig& MapManager::GetMap(int index) const
{
    if (m_Maps.empty())
    {
        static MapConfig defaultMap;
        return defaultMap;
    }
    if (index >= 0 && index < static_cast<int>(m_Maps.size()))
        return m_Maps[index];
    return m_Maps[0];
}

int MapManager::GetMapCount() const
{
    return static_cast<int>(m_Maps.size());
}

void MapManager::SwitchToMap(int index)
{
    if (index >= 0 && index < static_cast<int>(m_Maps.size()))
        m_CurrentMapIndex = index;
}

const std::string& MapManager::GetCurrentFloorTextureName() const
{
    return GetCurrentMap().FloorTextureName;
}

glm::vec3 MapManager::GetCurrentAmbientLight() const
{
    const MapConfig& map = GetCurrentMap();
    return map.Lighting.AmbientColor * map.Lighting.AmbientIntensity;
}

const LightingConfig& MapManager::GetCurrentLighting() const
{
    return GetCurrentMap().Lighting;
}

const PostProcessConfig& MapManager::GetCurrentPostProcess() const
{
    return GetCurrentMap().PostProcess;
}

bool MapManager::IsCurrentMapTerrain() const
{
    return GetCurrentMap().Terrain == TerrainType::Heightmap;
}

const HeightmapConfig& MapManager::GetCurrentHeightmapConfig() const
{
    return GetCurrentMap().Heightmap;
}

glm::vec3 MapManager::GetCurrentPlayerStartPos() const
{
    return GetCurrentMap().PlayerStartPos;
}

void MapManager::LoadMapsFromJson(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "[MapManager] JSON config not found: " << path << ", using defaults." << std::endl;
        return;
    }

    try
    {
        json j;
        file >> j;

        for (const auto& jMap : j["maps"])
        {
            MapConfig map;
            map.Name = jMap.value("name", "Unnamed");

            std::string skyboxType = jMap.value("skybox_type", "cubemap");
            if (skyboxType == "hdr")
            {
                map.Skybox.Type = SkyboxType::HDR;
                map.Skybox.HDRPath = jMap.value("skybox_hdr", "");
            }
            else
            {
                map.Skybox.Type = SkyboxType::Cubemap;
                if (jMap.contains("skybox_faces"))
                {
                    for (const auto& face : jMap["skybox_faces"])
                        map.Skybox.Faces.push_back(face.get<std::string>());
                }
            }

            map.FloorTexturePath = jMap.value("floor_texture", "");
            map.FloorTextureName = jMap.value("floor_texture_name", "floor_" + map.Name);

            if (jMap.contains("ambient_color"))
                map.AmbientColor = ReadVec3(jMap["ambient_color"], map.AmbientColor);
            map.AmbientIntensity = jMap.value("ambient_intensity", 1.0f);
            SyncLegacyAmbient(map);

            if (jMap.contains("lighting"))
            {
                const auto& jLighting = jMap["lighting"];
                if (jLighting.contains("main_light"))
                {
                    const auto& jMain = jLighting["main_light"];
                    map.Lighting.MainLight.Direction =
                        glm::normalize(ReadVec3(jMain.value("direction", json::array({-0.35f, -0.9f, -0.25f})),
                                                map.Lighting.MainLight.Direction));
                    map.Lighting.MainLight.Color =
                        ReadVec3(jMain.value("color", json::array({1.0f, 0.94f, 0.84f})),
                                 map.Lighting.MainLight.Color);
                    map.Lighting.MainLight.Intensity = jMain.value("intensity", map.Lighting.MainLight.Intensity);
                    map.Lighting.MainLight.SyncWithSkyboxRotation =
                        jMain.value("sync_with_skybox_rotation", map.Lighting.MainLight.SyncWithSkyboxRotation);
                    map.Lighting.MainLight.SkyboxRotationOffset =
                        jMain.value("skybox_rotation_offset", map.Lighting.MainLight.SkyboxRotationOffset);
                    map.Lighting.MainLight.AngularSize = jMain.value("angular_size", map.Lighting.MainLight.AngularSize);
                }

                if (jLighting.contains("ambient"))
                {
                    const auto& jAmbient = jLighting["ambient"];
                    if (jAmbient.contains("ambient_color"))
                        map.Lighting.AmbientColor = ReadVec3(jAmbient["ambient_color"], map.Lighting.AmbientColor);
                    map.Lighting.AmbientIntensity = jAmbient.value("ambient_intensity", map.Lighting.AmbientIntensity);
                    map.Lighting.IBLDiffuseIntensity =
                        jAmbient.value("ibl_diffuse_intensity", map.Lighting.IBLDiffuseIntensity);
                    map.Lighting.IBLSpecularIntensity =
                        jAmbient.value("ibl_specular_intensity", map.Lighting.IBLSpecularIntensity);
                }

                if (jLighting.contains("point_lights"))
                {
                    for (const auto& jPoint : jLighting["point_lights"])
                    {
                        PointLightConfig point;
                        point.Position = ReadVec3(jPoint.value("position", json::array({0.0f, 0.0f, 0.0f})),
                                                  point.Position);
                        point.Color = ReadVec3(jPoint.value("color", json::array({1.0f, 1.0f, 1.0f})),
                                               point.Color);
                        point.Intensity = jPoint.value("intensity", point.Intensity);
                        point.Range = jPoint.value("range", point.Range);
                        map.Lighting.PointLights.push_back(point);
                    }
                }
            }

            if (jMap.contains("post_process"))
            {
                const auto& jPost = jMap["post_process"];
                map.PostProcess.Exposure = jPost.value("exposure", map.PostProcess.Exposure);
                map.PostProcess.Gamma = jPost.value("gamma", map.PostProcess.Gamma);
                map.PostProcess.Contrast = jPost.value("contrast", map.PostProcess.Contrast);
                map.PostProcess.Saturation = jPost.value("saturation", map.PostProcess.Saturation);
                map.PostProcess.Vignette = jPost.value("vignette", map.PostProcess.Vignette);
                map.PostProcess.ChromaticAberration =
                    jPost.value("chromatic_aberration", map.PostProcess.ChromaticAberration);
                map.PostProcess.FilmGrain = jPost.value("film_grain", map.PostProcess.FilmGrain);

                if (jPost.contains("bloom"))
                {
                    const auto& jBloom = jPost["bloom"];
                    map.PostProcess.Bloom.Enabled = jBloom.value("enabled", map.PostProcess.Bloom.Enabled);
                    map.PostProcess.Bloom.Threshold = jBloom.value("threshold", map.PostProcess.Bloom.Threshold);
                    map.PostProcess.Bloom.Intensity = jBloom.value("intensity", map.PostProcess.Bloom.Intensity);
                    map.PostProcess.Bloom.Radius = jBloom.value("radius", map.PostProcess.Bloom.Radius);
                }
            }

            map.SpawnRadius = jMap.value("spawn_radius", 15.0f);

            std::string terrainType = jMap.value("terrain", "flat");
            map.Terrain = (terrainType == "heightmap") ? TerrainType::Heightmap : TerrainType::Flat;
            map.EcologyEnabled = jMap.value("ecology_enabled", map.Terrain == TerrainType::Heightmap && map.Name == "Nature");

            if (jMap.contains("heightmap"))
            {
                const auto& jHm = jMap["heightmap"];
                map.Heightmap.Path = jHm.value("path", "");
                map.Heightmap.HeightScale = jHm.value("height_scale", 5.0f);
                map.Heightmap.GridSize = jHm.value("grid_size", 100);
                map.Heightmap.Size = jHm.value("size", 20.0f);
            }

            if (jMap.contains("props"))
            {
                for (const auto& jProp : jMap["props"])
                {
                    PropConfig prop;
                    prop.ModelPath = jProp.value("model", "");
                    if (jProp.contains("position"))
                        prop.Position = ReadVec3(jProp["position"], prop.Position);
                    if (jProp.contains("rotation"))
                        prop.Rotation = ReadVec3(jProp["rotation"], prop.Rotation);
                    if (jProp.contains("scale"))
                    {
                        const auto& s = jProp["scale"];
                        if (s.is_number())
                            prop.Scale = glm::vec3(s.get<float>());
                        else
                            prop.Scale = ReadVec3(s, prop.Scale);
                    }
                    prop.TargetHeight = jProp.value("target_height", prop.TargetHeight);
                    prop.PBRBrightness = jProp.value("pbr_brightness", prop.PBRBrightness);
                    prop.PBRAmbientIntensity = jProp.value("pbr_ambient_intensity", prop.PBRAmbientIntensity);
                    if (jProp.contains("pbr_ambient_color"))
                        prop.PBRAmbientColor = ReadVec3(jProp["pbr_ambient_color"], prop.PBRAmbientColor);
                    map.Props.push_back(prop);
                }
            }

            if (jMap.contains("player_start"))
                map.PlayerStartPos = ReadVec3(jMap["player_start"], map.PlayerStartPos);

            if (jMap.contains("target_area"))
            {
                const auto& jTa = jMap["target_area"];
                map.TargetArea.WallDistance = jTa.value("wall_distance", 15.0f);
                map.TargetArea.WallWidth = jTa.value("wall_width", 12.0f);
                map.TargetArea.WallHeight = jTa.value("wall_height", 8.0f);
                map.TargetArea.WallCenterY = jTa.value("wall_center_y", 2.5f);
                map.TargetArea.Lifetime = jTa.value("lifetime", 5.0f);
            }

            m_Maps.push_back(map);
        }

        std::cout << "[MapManager] Loaded " << m_Maps.size() << " maps from JSON." << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "[MapManager] JSON parse error: " << e.what() << std::endl;
        m_Maps.clear();
    }
}

void MapManager::SetupDefaultMaps()
{
    MapConfig spaceMap;
    spaceMap.Name = "Space";
    spaceMap.Skybox.Type = SkyboxType::HDR;
    spaceMap.Skybox.HDRPath = "resources/textures/skybox_hdr/rogland_clear_night_1k.hdr";
    spaceMap.FloorTexturePath = "resources/textures/ground/space_aerial_rocks_02_diff_1k.jpg";
    spaceMap.FloorTextureName = "floor_space";
    spaceMap.AmbientColor = glm::vec3(0.1f, 0.1f, 0.2f);
    spaceMap.AmbientIntensity = 0.24f;
    SyncLegacyAmbient(spaceMap);
    spaceMap.SpawnRadius = 24.0f;
    spaceMap.Terrain = TerrainType::Heightmap;
    spaceMap.Heightmap.Path = "resources/textures/heightmaps/rocky_peaks_height.png";
    spaceMap.Heightmap.HeightScale = 18.0f;
    spaceMap.Heightmap.GridSize = 256;
    spaceMap.Heightmap.Size = 70.0f;
    spaceMap.PlayerStartPos = glm::vec3(0.0f, 3.0f, 8.0f);
    m_Maps.push_back(spaceMap);

    MapConfig natureMap;
    natureMap.Name = "Nature";
    natureMap.Skybox.Type = SkyboxType::HDR;
    natureMap.Skybox.HDRPath = "resources/textures/skybox_hdr/citrus_orchard_road_puresky_1k.hdr";
    natureMap.FloorTexturePath = "resources/textures/terrain/nature_rocky_peaks_diffuse.png";
    natureMap.FloorTextureName = "floor_nature";
    natureMap.AmbientColor = glm::vec3(0.2f, 0.3f, 0.2f);
    natureMap.AmbientIntensity = 1.0f;
    SyncLegacyAmbient(natureMap);
    natureMap.SpawnRadius = 15.0f;
    natureMap.Terrain = TerrainType::Heightmap;
    natureMap.Heightmap.Path = "resources/textures/heightmaps/rocky_peaks_height.png";
    natureMap.Heightmap.HeightScale = 3.0f;
    natureMap.Heightmap.GridSize = 128;
    natureMap.Heightmap.Size = 30.0f;
    natureMap.PlayerStartPos = glm::vec3(0.0f, 3.0f, 5.0f);
    m_Maps.push_back(natureMap);

    MapConfig cityMap;
    cityMap.Name = "City";
    cityMap.Skybox.Type = SkyboxType::HDR;
    cityMap.Skybox.HDRPath = "resources/textures/skybox_hdr/urban_street_04_1k.hdr";
    cityMap.FloorTexturePath = "resources/textures/ground/city_asphalt_02_diff_1k.jpg";
    cityMap.FloorTextureName = "floor_city";
    cityMap.AmbientColor = glm::vec3(0.3f, 0.3f, 0.3f);
    cityMap.AmbientIntensity = 0.22f;
    SyncLegacyAmbient(cityMap);
    cityMap.SpawnRadius = 18.0f;
    cityMap.Terrain = TerrainType::Heightmap;
    cityMap.Heightmap.Path = "resources/textures/heightmaps/rocky_peaks_height.png";
    cityMap.Heightmap.HeightScale = 3.0f;
    cityMap.Heightmap.GridSize = 256;
    cityMap.Heightmap.Size = 48.0f;
    cityMap.PlayerStartPos = glm::vec3(0.0f, 3.0f, 6.0f);
    m_Maps.push_back(cityMap);
}
