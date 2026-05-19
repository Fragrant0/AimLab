#ifndef PBR_PROP_RENDERER_H
#define PBR_PROP_RENDERER_H

#include <map>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "camera.h"
#include "map_manager.h"
#include "model.h"
#include "shader.h"
#include "shadow_mapper.h"
#include "terrain.h"

class PBRPropRenderer
{
public:
    using ModelMap = std::map<std::string, std::unique_ptr<Model>>;

    void Render(const MapConfig& map,
                const ModelMap& models,
                const Terrain* terrain,
                const Camera& camera,
                const glm::mat4& projection,
                const glm::mat4& view,
                const glm::vec3& mainLightDirection,
                const ShadowMapper* shadowMapper,
                bool iblEnabled,
                bool pcssEnabled,
                float shadowBias);

    void RenderDepth(const MapConfig& map,
                     const ModelMap& models,
                     const Terrain* terrain,
                     Shader& depthShader);

private:
    glm::mat4 BuildModelMatrix(const PropConfig& prop, const Model& model,
                               const MapConfig& map, const Terrain* terrain) const;
    void ApplyLighting(Shader& shader, const MapConfig& map,
                       const glm::vec3& mainLightDirection,
                       const ShadowMapper* shadowMapper,
                       bool iblEnabled,
                       bool pcssEnabled,
                       float shadowBias);
    void BindEnvironmentMaps(Shader& shader);
    void ApplyPropSettings(Shader& shader, const PropConfig& prop);
};

#endif
