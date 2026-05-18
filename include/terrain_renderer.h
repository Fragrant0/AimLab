#ifndef TERRAIN_RENDERER_H
#define TERRAIN_RENDERER_H

#include <glm/glm.hpp>

#include <string>

class Shader;
class ShadowMapper;
class Terrain;
struct LightingConfig;

class TerrainRenderer
{
public:
    TerrainRenderer();
    ~TerrainRenderer();

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    bool Initialize();
    void Cleanup();

    void RenderHeightmap(Terrain& terrain,
                         const std::string& floorTextureName,
                         const glm::vec3& ambientLight,
                         const glm::vec3& viewPos,
                         const glm::mat4& projection,
                         const glm::mat4& view,
                         const LightingConfig& lighting,
                         const glm::vec3& mainLightDirection,
                         const ShadowMapper* shadowMapper);

    void RenderPlane(const std::string& floorTextureName,
                     const glm::vec3& ambientLight,
                     const glm::vec3& viewPos,
                     const glm::mat4& projection,
                     const glm::mat4& view,
                     const LightingConfig& lighting,
                     const glm::vec3& mainLightDirection,
                     const ShadowMapper* shadowMapper);

    void RenderDepthHeightmap(Terrain& terrain, Shader& depthShader);
    void RenderDepthPlane(Shader& depthShader);

private:
    void ApplyLighting(Shader& shader,
                       const LightingConfig& lighting,
                       const glm::vec3& mainLightDirection,
                       const ShadowMapper* shadowMapper);

    unsigned int m_PlaneVAO;
    unsigned int m_PlaneVBO;
};

#endif
