#include "terrain_renderer.h"

#include "lighting.h"
#include "render_settings.h"
#include "resource_manager.h"
#include "shader.h"
#include "shadow_mapper.h"
#include "terrain.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float kPlaneVertices[] = {
         5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 0.0f,
        -5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 2.0f,

         5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 2.0f,
         5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 2.0f
    };
}

TerrainRenderer::TerrainRenderer()
    : m_PlaneVAO(0),
      m_PlaneVBO(0)
{
}

TerrainRenderer::~TerrainRenderer()
{
    Cleanup();
}

bool TerrainRenderer::Initialize()
{
    if (m_PlaneVAO != 0)
        return true;

    glGenVertexArrays(1, &m_PlaneVAO);
    glGenBuffers(1, &m_PlaneVBO);
    glBindVertexArray(m_PlaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_PlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kPlaneVertices), kPlaneVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    return true;
}

void TerrainRenderer::Cleanup()
{
    if (m_PlaneVAO != 0)
    {
        glDeleteVertexArrays(1, &m_PlaneVAO);
        m_PlaneVAO = 0;
    }

    if (m_PlaneVBO != 0)
    {
        glDeleteBuffers(1, &m_PlaneVBO);
        m_PlaneVBO = 0;
    }
}

void TerrainRenderer::RenderHeightmap(Terrain& terrain,
                                      const std::string& floorTextureName,
                                      const glm::vec3& ambientLight,
                                      const glm::vec3& viewPos,
                                      const glm::mat4& projection,
                                      const glm::mat4& view,
                                      const LightingConfig& lighting,
                                      const glm::vec3& mainLightDirection,
                                      const ShadowMapper* shadowMapper,
                                      bool pcssEnabled,
                                      float shadowBias)
{
    if (!terrain.IsInitialized())
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* terrainShader = rm.GetShader("terrain");
    if (!terrainShader)
        return;

    terrainShader->use();
    terrainShader->setMat4("projection", projection);
    terrainShader->setMat4("view", view);
    terrainShader->setVec3("ambientLight", ambientLight);
    terrainShader->setVec3("viewPos", viewPos);
    terrainShader->setMat4("model", glm::mat4(1.0f));
    ApplyLighting(*terrainShader, lighting, mainLightDirection, shadowMapper, pcssEnabled, shadowBias);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rm.GetTexture(floorTextureName));

    terrain.Render();
}

void TerrainRenderer::RenderPlane(const std::string& floorTextureName,
                                  const glm::vec3& ambientLight,
                                  const glm::vec3& viewPos,
                                  const glm::mat4& projection,
                                  const glm::mat4& view,
                                  const LightingConfig& lighting,
                                  const glm::vec3& mainLightDirection,
                                  const ShadowMapper* shadowMapper,
                                  bool pcssEnabled,
                                  float shadowBias)
{
    if (m_PlaneVAO == 0)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* planeShader = rm.GetShader("plane");
    if (!planeShader)
        return;

    planeShader->use();
    planeShader->setMat4("projection", projection);
    planeShader->setMat4("view", view);
    planeShader->setVec3("ambientLight", ambientLight);
    planeShader->setVec3("viewPos", viewPos);
    ApplyLighting(*planeShader, lighting, mainLightDirection, shadowMapper, pcssEnabled, shadowBias);

    glBindVertexArray(m_PlaneVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rm.GetTexture(floorTextureName));
    planeShader->setMat4("model", glm::mat4(1.0f));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void TerrainRenderer::RenderDepthHeightmap(Terrain& terrain, Shader& depthShader)
{
    if (!terrain.IsInitialized())
        return;

    depthShader.setMat4("model", glm::mat4(1.0f));
    terrain.Render();
}

void TerrainRenderer::RenderDepthPlane(Shader& depthShader)
{
    if (m_PlaneVAO == 0)
        return;

    depthShader.setMat4("model", glm::mat4(1.0f));
    glBindVertexArray(m_PlaneVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void TerrainRenderer::ApplyLighting(Shader& shader,
                                    const LightingConfig& lighting,
                                    const glm::vec3& mainLightDirection,
                                    const ShadowMapper* shadowMapper,
                                    bool pcssEnabled,
                                    float shadowBias)
{
    shader.setVec3("u_MainLightDirection", mainLightDirection);
    shader.setVec3("u_MainLightColor", lighting.MainLight.Color);
    shader.setFloat("u_MainLightIntensity", lighting.MainLight.Intensity);
    shader.setBool("u_ShadowsEnabled", shadowMapper != nullptr);
    shader.setBool("u_PCSSEnabled", pcssEnabled);
    shader.setFloat("u_LightSize", lighting.MainLight.AngularSize);
    shader.setFloat("u_ShadowBias", shadowBias);

    if (shadowMapper)
    {
        shader.setMat4("u_LightSpaceMatrix", shadowMapper->GetLightSpaceMatrix());
        glActiveTexture(GL_TEXTURE0 + TextureUnit::Shadow);
        glBindTexture(GL_TEXTURE_2D, shadowMapper->GetDepthMap());
        shader.setInt("u_ShadowMap", TextureUnit::Shadow);
    }
}
