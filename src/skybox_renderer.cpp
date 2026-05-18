#include "skybox_renderer.h"

#include "gl_state_guard.h"
#include "map_manager.h"
#include "resource_manager.h"
#include "shader.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float kSkyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f
    };
}

SkyboxRenderer::SkyboxRenderer()
    : m_VAO(0),
      m_VBO(0)
{
}

SkyboxRenderer::~SkyboxRenderer()
{
    Cleanup();
}

bool SkyboxRenderer::Initialize()
{
    if (m_VAO != 0)
        return true;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices), kSkyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    return true;
}

void SkyboxRenderer::Render(const MapConfig& map, const glm::mat4& projection, const glm::mat4& view, float rotation)
{
    if (m_VAO == 0)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* skyboxShader = rm.GetShader("skybox");
    if (!skyboxShader)
        return;

    DepthFuncGuard depthFunc(GL_LEQUAL);

    skyboxShader->use();
    skyboxShader->setMat4("view", glm::mat4(glm::mat3(view)));
    skyboxShader->setMat4("projection", projection);
    skyboxShader->setFloat("rotation", rotation);

    const bool isHDR = (map.Skybox.Type == SkyboxType::HDR);
    skyboxShader->setBool("isHDR", isHDR);
    skyboxShader->setFloat("exposure", isHDR ? 1.25f : 1.0f);

    glBindVertexArray(m_VAO);
    glActiveTexture(GL_TEXTURE0);

    if (isHDR)
        glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetHDRSkybox("skybox"));
    else
        glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetCubemap("skybox"));

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void SkyboxRenderer::Cleanup()
{
    if (m_VAO != 0)
    {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }

    if (m_VBO != 0)
    {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
}
