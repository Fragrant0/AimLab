#include "shadow_pass_renderer.h"

#include "camera.h"
#include "ecology_system.h"
#include "map_manager.h"
#include "pbr_prop_renderer.h"
#include "shader.h"
#include "shadow_mapper.h"
#include "terrain.h"
#include "terrain_renderer.h"
#include "resource_manager.h"

void ShadowPassRenderer::Render(ShadowMapper& shadowMapper,
                                const MapConfig& map,
                                Terrain* terrain,
                                const EcologySystem* ecologySystem,
                                TerrainRenderer& terrainRenderer,
                                PBRPropRenderer& propRenderer,
                                const ModelMap& propModels,
                                const Camera& camera,
                                const glm::vec3& mainLightDirection)
{
    shadowMapper.BeginDepthPass(mainLightDirection, camera);

    Shader* depthShader = shadowMapper.GetDepthShader();
    if (depthShader)
    {
        depthShader->use();
        depthShader->setMat4("lightSpaceMatrix", shadowMapper.GetLightSpaceMatrix());

        RenderTerrainCasters(map, terrain, terrainRenderer, *depthShader);

        if (ecologySystem && ecologySystem->IsReady())
        {
            ResourceManager& rm = ResourceManager::GetInstance();
            Shader* instancedDepthShader = rm.GetShader("ecology_shadow_depth");
            if (instancedDepthShader)
            {
                instancedDepthShader->use();
                instancedDepthShader->setMat4("lightSpaceMatrix", shadowMapper.GetLightSpaceMatrix());
                ecologySystem->RenderDepth(*instancedDepthShader);
            }
        }

        propRenderer.RenderDepth(map, propModels, terrain, *depthShader);
    }

    shadowMapper.EndDepthPass();
}

void ShadowPassRenderer::RenderTerrainCasters(const MapConfig& map,
                                              Terrain* terrain,
                                              TerrainRenderer& terrainRenderer,
                                              Shader& depthShader)
{
    if (map.Terrain == TerrainType::Heightmap && terrain && terrain->IsInitialized())
        terrainRenderer.RenderDepthHeightmap(*terrain, depthShader);
    else
        terrainRenderer.RenderDepthPlane(depthShader);
}
