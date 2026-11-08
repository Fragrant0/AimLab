#include "map_manager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using json = nlohmann::json;

namespace
{
    [[noreturn]] void InvalidField(const std::string& field, const std::string& reason)
    {
        throw std::runtime_error("[MapManager] " + field + ": " + reason);
    }

    std::string FieldPath(const std::string& context, const char* key)
    {
        return context + "." + key;
    }

    float ReadNumber(const json& value, const std::string& field)
    {
        if (!value.is_number())
            InvalidField(field, "expected a finite number.");

        const double number = value.get<double>();
        if (!std::isfinite(number) || number < std::numeric_limits<float>::lowest() ||
            number > std::numeric_limits<float>::max())
            InvalidField(field, "expected a finite number representable as float.");
        return static_cast<float>(number);
    }

    float ReadFloat(const json& object, const char* key, float fallback, const std::string& context)
    {
        const auto value = object.find(key);
        return value == object.end() ? fallback : ReadNumber(*value, FieldPath(context, key));
    }

    float ReadNonNegative(const json& object, const char* key, float fallback, const std::string& context)
    {
        const float value = ReadFloat(object, key, fallback, context);
        if (value < 0.0f)
            InvalidField(FieldPath(context, key), "must be non-negative.");
        return value;
    }

    float ReadPositive(const json& object, const char* key, float fallback, const std::string& context)
    {
        const float value = ReadFloat(object, key, fallback, context);
        if (value <= 0.0f)
            InvalidField(FieldPath(context, key), "must be greater than zero.");
        return value;
    }

    std::string ReadString(const json& object, const char* key, const std::string& fallback,
                           const std::string& context, bool requireNonEmpty = false)
    {
        const auto value = object.find(key);
        if (value != object.end() && !value->is_string())
            InvalidField(FieldPath(context, key), "expected a string.");
        const std::string result = value == object.end() ? fallback : value->get<std::string>();
        if (result.find('\0') != std::string::npos)
            InvalidField(FieldPath(context, key), "must not contain an embedded null character.");
        if (requireNonEmpty && result.find_first_not_of(" \t\r\n") == std::string::npos)
            InvalidField(FieldPath(context, key), "must be a non-empty string.");
        return result;
    }

    bool ReadBool(const json& object, const char* key, bool fallback, const std::string& context)
    {
        const auto value = object.find(key);
        if (value == object.end())
            return fallback;
        if (!value->is_boolean())
            InvalidField(FieldPath(context, key), "expected a boolean.");
        return value->get<bool>();
    }

    const json& ReadObject(const json& object, const char* key, const std::string& context)
    {
        static const json emptyObject = json::object();
        const auto value = object.find(key);
        if (value == object.end())
            return emptyObject;
        if (!value->is_object())
            InvalidField(FieldPath(context, key), "expected an object.");
        return *value;
    }

    const json& ReadArray(const json& object, const char* key, const std::string& context)
    {
        static const json emptyArray = json::array();
        const auto value = object.find(key);
        if (value == object.end())
            return emptyArray;
        if (!value->is_array())
            InvalidField(FieldPath(context, key), "expected an array.");
        return *value;
    }

    glm::vec3 ReadVec3(const json& object, const char* key, const glm::vec3& fallback,
                       const std::string& context)
    {
        const auto value = object.find(key);
        if (value == object.end())
            return fallback;
        const std::string field = FieldPath(context, key);
        if (!value->is_array() || value->size() != 3)
            InvalidField(field, "expected an array of exactly three numbers.");
        glm::vec3 result;
        for (int component = 0; component < 3; ++component)
            result[component] = ReadNumber((*value)[component], field + "[" + std::to_string(component) + "]");
        return result;
    }

    glm::vec3 ReadColor(const json& object, const char* key, const glm::vec3& fallback,
                        const std::string& context)
    {
        const glm::vec3 color = ReadVec3(object, key, fallback, context);
        if (color.x < 0.0f || color.y < 0.0f || color.z < 0.0f)
            InvalidField(FieldPath(context, key), "color components must be non-negative.");
        return color;
    }
}

MapManager::MapManager()
    : m_CurrentMapIndex(0)
{
}

MapManager::~MapManager() = default;

void MapManager::Initialize()
{
    LoadMapsFromJson("resources/maps_config.json");
    m_CurrentMapIndex = std::min(1, static_cast<int>(m_Maps.size()) - 1);
}

int MapManager::GetCurrentMapIndex() const
{
    return m_CurrentMapIndex;
}

const MapConfig& MapManager::GetCurrentMap() const
{
    return GetMap(m_CurrentMapIndex);
}

const MapConfig& MapManager::GetMap(int index) const
{
    if (m_Maps.empty())
        throw std::runtime_error("[MapManager] No maps have been loaded.");
    if (index < 0 || index >= static_cast<int>(m_Maps.size()))
        throw std::out_of_range("[MapManager] Map index " + std::to_string(index) +
                                " is outside [0, " + std::to_string(m_Maps.size()) + ").");
    return m_Maps[index];
}

int MapManager::GetMapCount() const
{
    return static_cast<int>(m_Maps.size());
}

void MapManager::SwitchToMap(int index)
{
    GetMap(index);
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
        throw std::runtime_error("[MapManager] Could not open JSON config: " + path);

    json document;
    try
    {
        document = json::parse(file);
    }
    catch (const json::exception& error)
    {
        throw std::runtime_error("[MapManager] Could not parse JSON config " + path + ": " + error.what());
    }
    if (file.bad())
        throw std::runtime_error("[MapManager] Could not read JSON config: " + path);
    if (!document.is_object())
        InvalidField(path, "expected an object containing a non-empty maps array.");

    const json& maps = ReadArray(document, "maps", path);
    if (maps.empty() || maps.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        InvalidField(path + ".maps", "expected a non-empty maps array with a representable map count.");

    std::vector<MapConfig> parsedMaps;
    parsedMaps.reserve(maps.size());
    for (size_t index = 0; index < maps.size(); ++index)
    {
        const json& jMap = maps[index];
        const std::string context = path + " maps[" + std::to_string(index) + "]";
        if (!jMap.is_object())
            InvalidField(context, "expected a map object.");

        MapConfig map;
        map.Name = ReadString(jMap, "name", "Unnamed", context, true);
        const std::string skyboxType = ReadString(jMap, "skybox_type", "cubemap", context);
        map.Skybox.HDRPath = ReadString(jMap, "skybox_hdr", "", context);
        const json& faces = ReadArray(jMap, "skybox_faces", context);
        if (jMap.contains("skybox_faces") && faces.size() != 6)
            InvalidField(context + ".skybox_faces", "a cubemap requires exactly six face paths.");
        for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
        {
            const std::string faceContext = context + ".skybox_faces[" + std::to_string(faceIndex) + "]";
            if (!faces[faceIndex].is_string())
                InvalidField(faceContext, "expected a non-empty face path string.");
            const std::string face = faces[faceIndex].get<std::string>();
            if (face.find_first_not_of(" \t\r\n") == std::string::npos || face.find('\0') != std::string::npos)
                InvalidField(faceContext, "expected a non-empty face path without null characters.");
            map.Skybox.Faces.push_back(face);
        }
        if (skyboxType == "hdr")
        {
            map.Skybox.Type = SkyboxType::HDR;
            map.Skybox.HDRPath = ReadString(jMap, "skybox_hdr", "", context, true);
        }
        else if (skyboxType == "cubemap")
        {
            map.Skybox.Type = SkyboxType::Cubemap;
            if (faces.size() != 6)
                InvalidField(context + ".skybox_faces", "a cubemap requires exactly six face paths.");
        }
        else
        {
            InvalidField(context + ".skybox_type", "expected 'hdr' or 'cubemap', got '" + skyboxType + "'.");
        }

        map.FloorTexturePath = ReadString(jMap, "floor_texture", "", context, true);
        map.FloorTextureName = ReadString(jMap, "floor_texture_name", "floor_" + map.Name, context, true);
        map.AmbientColor = ReadColor(jMap, "ambient_color", map.AmbientColor, context);
        map.AmbientIntensity = ReadNonNegative(jMap, "ambient_intensity", map.AmbientIntensity, context);
        map.Lighting.AmbientColor = map.AmbientColor;
        map.Lighting.AmbientIntensity = map.AmbientIntensity;

        const json& lighting = ReadObject(jMap, "lighting", context);
        const std::string lightingContext = context + ".lighting";
        const json& mainLight = ReadObject(lighting, "main_light", lightingContext);
        const std::string mainContext = lightingContext + ".main_light";
        glm::vec3 direction = ReadVec3(mainLight, "direction", map.Lighting.MainLight.Direction, mainContext);
        const float directionScale = std::max({ std::abs(direction.x), std::abs(direction.y), std::abs(direction.z) });
        if (directionScale == 0.0f)
            InvalidField(mainContext + ".direction", "must be a non-zero direction vector.");
        map.Lighting.MainLight.Direction = glm::normalize(direction / directionScale);
        map.Lighting.MainLight.Color = ReadColor(mainLight, "color", map.Lighting.MainLight.Color, mainContext);
        map.Lighting.MainLight.Intensity = ReadNonNegative(mainLight, "intensity", map.Lighting.MainLight.Intensity, mainContext);
        map.Lighting.MainLight.SyncWithSkyboxRotation = ReadBool(mainLight, "sync_with_skybox_rotation",
            map.Lighting.MainLight.SyncWithSkyboxRotation, mainContext);
        map.Lighting.MainLight.SkyboxRotationOffset = ReadFloat(mainLight, "skybox_rotation_offset",
            map.Lighting.MainLight.SkyboxRotationOffset, mainContext);
        map.Lighting.MainLight.AngularSize = ReadNonNegative(mainLight, "angular_size", map.Lighting.MainLight.AngularSize, mainContext);

        const json& ambient = ReadObject(lighting, "ambient", lightingContext);
        const std::string ambientContext = lightingContext + ".ambient";
        map.Lighting.AmbientColor = ReadColor(ambient, "ambient_color", map.Lighting.AmbientColor, ambientContext);
        map.Lighting.AmbientIntensity = ReadNonNegative(ambient, "ambient_intensity", map.Lighting.AmbientIntensity, ambientContext);
        map.Lighting.IBLDiffuseIntensity = ReadNonNegative(ambient, "ibl_diffuse_intensity", map.Lighting.IBLDiffuseIntensity, ambientContext);
        map.Lighting.IBLSpecularIntensity = ReadNonNegative(ambient, "ibl_specular_intensity", map.Lighting.IBLSpecularIntensity, ambientContext);

        const json& pointLights = ReadArray(lighting, "point_lights", lightingContext);
        for (size_t pointIndex = 0; pointIndex < pointLights.size(); ++pointIndex)
        {
            const json& jPoint = pointLights[pointIndex];
            const std::string pointContext = lightingContext + ".point_lights[" + std::to_string(pointIndex) + "]";
            if (!jPoint.is_object())
                InvalidField(pointContext, "expected a point-light object.");
            PointLightConfig point;
            point.Position = ReadVec3(jPoint, "position", point.Position, pointContext);
            point.Color = ReadColor(jPoint, "color", point.Color, pointContext);
            point.Intensity = ReadNonNegative(jPoint, "intensity", point.Intensity, pointContext);
            point.Range = ReadPositive(jPoint, "range", point.Range, pointContext);
            map.Lighting.PointLights.push_back(point);
        }

        const json& post = ReadObject(jMap, "post_process", context);
        const std::string postContext = context + ".post_process";
        map.PostProcess.Exposure = ReadNonNegative(post, "exposure", map.PostProcess.Exposure, postContext);
        map.PostProcess.Gamma = ReadPositive(post, "gamma", map.PostProcess.Gamma, postContext);
        map.PostProcess.Contrast = ReadNonNegative(post, "contrast", map.PostProcess.Contrast, postContext);
        map.PostProcess.Saturation = ReadNonNegative(post, "saturation", map.PostProcess.Saturation, postContext);
        map.PostProcess.Vignette = ReadNonNegative(post, "vignette", map.PostProcess.Vignette, postContext);
        if (map.PostProcess.Vignette > 1.0f)
            InvalidField(postContext + ".vignette", "must be in [0, 1].");
        map.PostProcess.PixelateEnabled = ReadBool(post, "pixelate_enabled", map.PostProcess.PixelateEnabled, postContext);
        map.PostProcess.PixelSize = ReadPositive(post, "pixel_size", map.PostProcess.PixelSize, postContext);
        map.PostProcess.UnderwaterEnabled = ReadBool(post, "underwater_enabled", map.PostProcess.UnderwaterEnabled, postContext);

        map.SpawnRadius = ReadPositive(jMap, "spawn_radius", map.SpawnRadius, context);
        const std::string terrainType = ReadString(jMap, "terrain", "flat", context);
        if (terrainType != "flat" && terrainType != "heightmap")
            InvalidField(context + ".terrain", "expected 'flat' or 'heightmap', got '" + terrainType + "'.");
        map.Terrain = terrainType == "heightmap" ? TerrainType::Heightmap : TerrainType::Flat;
        map.EcologyEnabled = ReadBool(jMap, "ecology_enabled",
            map.Terrain == TerrainType::Heightmap && map.Name == "Nature", context);

        const json& heightmap = ReadObject(jMap, "heightmap", context);
        const std::string heightmapContext = context + ".heightmap";
        map.Heightmap.Path = ReadString(heightmap, "path", "", heightmapContext, map.Terrain == TerrainType::Heightmap);
        map.Heightmap.HeightScale = ReadFloat(heightmap, "height_scale", map.Heightmap.HeightScale, heightmapContext);
        map.Heightmap.Size = ReadPositive(heightmap, "size", map.Heightmap.Size, heightmapContext);
        if (heightmap.contains("grid_size"))
        {
            const json& gridValue = heightmap["grid_size"];
            if (!gridValue.is_number())
                InvalidField(heightmapContext + ".grid_size", "expected an integer.");
            const double gridSize = gridValue.get<double>();
            // Cap this project's terrain allocation budget to about four million vertices.
            if (!std::isfinite(gridSize) || gridSize < 1.0 || gridSize > 2048.0 || std::floor(gridSize) != gridSize)
                InvalidField(heightmapContext + ".grid_size", "must be an integer in [1, 2048] (terrain allocation budget).");
            map.Heightmap.GridSize = static_cast<int>(gridSize);
        }

        const json& props = ReadArray(jMap, "props", context);
        for (size_t propIndex = 0; propIndex < props.size(); ++propIndex)
        {
            const json& jProp = props[propIndex];
            const std::string propContext = context + ".props[" + std::to_string(propIndex) + "]";
            if (!jProp.is_object())
                InvalidField(propContext, "expected a prop object.");
            PropConfig prop;
            prop.ModelPath = ReadString(jProp, "model", "", propContext, true);
            prop.Position = ReadVec3(jProp, "position", prop.Position, propContext);
            prop.Rotation = ReadVec3(jProp, "rotation", prop.Rotation, propContext);
            if (jProp.contains("scale"))
            {
                prop.Scale = jProp["scale"].is_number()
                    ? glm::vec3(ReadNumber(jProp["scale"], propContext + ".scale"))
                    : ReadVec3(jProp, "scale", prop.Scale, propContext);
            }
            if (prop.Scale.x == 0.0f || prop.Scale.y == 0.0f || prop.Scale.z == 0.0f)
                InvalidField(propContext + ".scale", "all scale components must be non-zero.");
            prop.TargetHeight = ReadNonNegative(jProp, "target_height", prop.TargetHeight, propContext);
            prop.PBRBrightness = ReadNonNegative(jProp, "pbr_brightness", prop.PBRBrightness, propContext);
            prop.PBRAmbientIntensity = ReadNonNegative(jProp, "pbr_ambient_intensity", prop.PBRAmbientIntensity, propContext);
            prop.PBRAmbientColor = ReadColor(jProp, "pbr_ambient_color", prop.PBRAmbientColor, propContext);
            map.Props.push_back(std::move(prop));
        }

        map.PlayerStartPos = ReadVec3(jMap, "player_start", map.PlayerStartPos, context);
        const json& target = ReadObject(jMap, "target_area", context);
        const std::string targetContext = context + ".target_area";
        map.TargetArea.WallDistance = ReadPositive(target, "wall_distance", map.TargetArea.WallDistance, targetContext);
        map.TargetArea.WallWidth = ReadNonNegative(target, "wall_width", map.TargetArea.WallWidth, targetContext);
        map.TargetArea.WallHeight = ReadNonNegative(target, "wall_height", map.TargetArea.WallHeight, targetContext);
        map.TargetArea.WallCenterY = ReadFloat(target, "wall_center_y", map.TargetArea.WallCenterY, targetContext);
        map.TargetArea.Lifetime = ReadPositive(target, "lifetime", map.TargetArea.Lifetime, targetContext);
        parsedMaps.push_back(std::move(map));
    }

    m_Maps.swap(parsedMaps);
    std::cout << "[MapManager] Loaded " << m_Maps.size() << " maps from JSON." << std::endl;
}
