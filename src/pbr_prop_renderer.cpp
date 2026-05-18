#include "pbr_prop_renderer.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "render_settings.h"
#include "resource_manager.h"

void PBRPropRenderer::Render(const MapConfig& map,
                             const ModelMap& models,
                             const Terrain* terrain,
                             const Camera& camera,
                             const glm::mat4& projection,
                             const glm::mat4& view,
                             const glm::vec3& mainLightDirection,
                             const ShadowMapper* shadowMapper)
{
    if (models.empty())
        return;

    Shader* shader = ResourceManager::GetInstance().GetShader("pbr_model");
    if (!shader)
        return;

    shader->use();
    shader->setVec3("cameraPos", camera.Position);
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    ApplyLighting(*shader, map, mainLightDirection, shadowMapper);
    BindEnvironmentMaps(*shader);

    for (const PropConfig& prop : map.Props)
    {
        auto it = models.find(prop.ModelPath);
        if (it == models.end() || !it->second)
            continue;

        glm::mat4 model = BuildModelMatrix(prop, *it->second, map, terrain);
        shader->setMat4("model", model);
        shader->setMat3("normalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        ApplyPropSettings(*shader, prop);
        it->second->Draw(*shader);
    }
}

void PBRPropRenderer::RenderDepth(const MapConfig& map,
                                  const ModelMap& models,
                                  const Terrain* terrain,
                                  Shader& depthShader)
{
    if (models.empty())
        return;

    for (const PropConfig& prop : map.Props)
    {
        auto it = models.find(prop.ModelPath);
        if (it == models.end() || !it->second)
            continue;

        glm::mat4 model = BuildModelMatrix(prop, *it->second, map, terrain);
        depthShader.setMat4("model", model);
        it->second->DrawGeometry();
    }
}

glm::mat4 PBRPropRenderer::BuildModelMatrix(const PropConfig& prop, const Model& model,
                                            const MapConfig& map, const Terrain* terrain) const
{
    glm::vec3 propPosition = prop.Position;
    if (map.Terrain == TerrainType::Heightmap && terrain && terrain->IsInitialized())
        propPosition.y = terrain->GetHeightAt(propPosition.x, propPosition.z) + prop.Position.y;

    glm::vec3 finalScale = prop.Scale;
    const glm::vec3 boundsSize = model.GetBoundsSize();
    if (prop.TargetHeight > 0.0f && boundsSize.y > 0.0001f)
    {
        const float normalizeScale = prop.TargetHeight / boundsSize.y;
        finalScale *= normalizeScale;
    }

    propPosition.y -= model.GetBoundsMin().y * finalScale.y;

    glm::mat4 matrix(1.0f);
    matrix = glm::translate(matrix, propPosition);
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, finalScale);
    return matrix;
}

void PBRPropRenderer::ApplyLighting(Shader& shader, const MapConfig& map,
                                    const glm::vec3& mainLightDirection,
                                    const ShadowMapper* shadowMapper)
{
    const LightingConfig& lighting = map.Lighting;
    shader.setVec3("u_MainLight.direction", mainLightDirection);
    shader.setVec3("u_MainLight.color", lighting.MainLight.Color);
    shader.setFloat("u_MainLight.intensity", lighting.MainLight.Intensity);
    shader.setFloat("u_IBLDiffuseIntensity", lighting.IBLDiffuseIntensity);
    shader.setFloat("u_IBLSpecularIntensity", lighting.IBLSpecularIntensity);

    int count = std::min(static_cast<int>(lighting.PointLights.size()), 8);
    shader.setInt("u_PointLightCount", count);
    for (int i = 0; i < count; ++i)
    {
        const PointLightConfig& light = lighting.PointLights[i];
        const std::string index = std::to_string(i);
        shader.setVec3("u_PointLightPositions[" + index + "]", light.Position);
        shader.setVec3("u_PointLightColors[" + index + "]", light.Color);
        shader.setFloat("u_PointLightIntensities[" + index + "]", light.Intensity);
        shader.setFloat("u_PointLightRanges[" + index + "]", light.Range);
    }

    shader.setBool("u_ShadowsEnabled", shadowMapper != nullptr);
    shader.setFloat("u_LightSize", lighting.MainLight.AngularSize);
    shader.setFloat("u_ShadowBias", 0.0018f);
    if (shadowMapper)
    {
        shader.setMat4("u_LightSpaceMatrix", shadowMapper->GetLightSpaceMatrix());
        glActiveTexture(GL_TEXTURE0 + TextureUnit::Shadow);
        glBindTexture(GL_TEXTURE_2D, shadowMapper->GetDepthMap());
        shader.setInt("u_ShadowMap", TextureUnit::Shadow);
    }
}

void PBRPropRenderer::BindEnvironmentMaps(Shader& shader)
{
    ResourceManager& rm = ResourceManager::GetInstance();

    glActiveTexture(GL_TEXTURE0 + TextureUnit::Irradiance);
    glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetIrradianceMap("skybox"));
    shader.setInt("irradianceMap", TextureUnit::Irradiance);

    glActiveTexture(GL_TEXTURE0 + TextureUnit::Prefilter);
    glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetPrefilterMap("skybox"));
    shader.setInt("prefilterMap", TextureUnit::Prefilter);

    glActiveTexture(GL_TEXTURE0 + TextureUnit::BRDFLUT);
    glBindTexture(GL_TEXTURE_2D, rm.GetBRDFLUT());
    shader.setInt("brdfLUT", TextureUnit::BRDFLUT);
}

void PBRPropRenderer::ApplyPropSettings(Shader& shader, const PropConfig& prop)
{
    shader.setVec3("u_AlbedoColor", glm::vec3(0.8f, 0.8f, 0.8f));
    shader.setFloat("u_Metallic", 0.0f);
    shader.setFloat("u_Roughness", 0.65f);
    shader.setFloat("u_AO", 1.0f);
    shader.setFloat("u_ModelBrightness", prop.PBRBrightness);
    shader.setFloat("u_ModelAmbientIntensity", prop.PBRAmbientIntensity);
    shader.setVec3("u_ModelAmbientColor", prop.PBRAmbientColor);
}
