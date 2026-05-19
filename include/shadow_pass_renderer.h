#ifndef SHADOW_PASS_RENDERER_H
#define SHADOW_PASS_RENDERER_H

#include <glm/glm.hpp>

#include <map>
#include <memory>
#include <string>

class Camera;
class EcologySystem;
class Model;
class PBRPropRenderer;
class Shader;
class ShadowMapper;
class Terrain;
class TerrainRenderer;
struct MapConfig;

class ShadowPassRenderer
{
public:
    using ModelMap = std::map<std::string, std::unique_ptr<Model>>;

    void Render(ShadowMapper& shadowMapper,
                const MapConfig& map,
                Terrain* terrain,
                const EcologySystem* ecologySystem,
                TerrainRenderer& terrainRenderer,
                PBRPropRenderer& propRenderer,
                const ModelMap& propModels,
                const Camera& camera,
                const glm::vec3& mainLightDirection);

private:
    void RenderTerrainCasters(const MapConfig& map,
                              Terrain* terrain,
                              TerrainRenderer& terrainRenderer,
                              Shader& depthShader);
};

#endif
