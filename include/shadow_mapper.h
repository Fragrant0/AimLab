#ifndef SHADOW_MAPPER_H
#define SHADOW_MAPPER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <memory>

#include "camera.h"
#include "shader.h"

class ShadowMapper
{
public:
    ShadowMapper();
    ~ShadowMapper();

    bool Initialize(unsigned int size = 4096);
    void Cleanup();

    void BeginDepthPass(const glm::vec3& lightDirection, const Camera& camera);
    void EndDepthPass();

    Shader* GetDepthShader() const { return m_DepthShader.get(); }
    unsigned int GetDepthMap() const { return m_DepthMap; }
    const glm::mat4& GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }

private:
    unsigned int m_Size;
    unsigned int m_DepthFBO;
    unsigned int m_DepthMap;
    std::unique_ptr<Shader> m_DepthShader;
    glm::mat4 m_LightSpaceMatrix;
    GLint m_PreviousViewport[4];
    bool m_Initialized;
};

#endif
