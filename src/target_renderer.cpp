#include "target_renderer.h"

#include "camera.h"
#include "resource_manager.h"
#include "shader.h"
#include "target_manager.h"

#include <glad/glad.h>

namespace
{
    struct BlendState
    {
        GLboolean enabled;
        GLint srcRGB;
        GLint dstRGB;
        GLint srcAlpha;
        GLint dstAlpha;
    };

    BlendState CaptureBlendState()
    {
        BlendState state{};
        state.enabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB, &state.srcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &state.dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.srcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &state.dstAlpha);
        return state;
    }

    void RestoreBlendState(const BlendState& state)
    {
        if (state.enabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        glBlendFuncSeparate(static_cast<GLenum>(state.srcRGB),
                            static_cast<GLenum>(state.dstRGB),
                            static_cast<GLenum>(state.srcAlpha),
                            static_cast<GLenum>(state.dstAlpha));
    }
}

void TargetRenderer::Render(TargetManager* targetManager,
                            const Camera& camera,
                            const glm::vec3& ambientLight,
                            const glm::mat4& projection,
                            const glm::mat4& view)
{
    if (!targetManager)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* sphereShader = rm.GetShader("sphere");
    if (!sphereShader)
        return;

    sphereShader->use();
    sphereShader->setMat4("projection", projection);
    sphereShader->setMat4("view", view);
    sphereShader->setVec3("viewPos", camera.Position);
    sphereShader->setVec3("ambientLight", ambientLight);
    sphereShader->setFloat("alpha", 1.0f);
    sphereShader->setVec3("lightDir", glm::vec3(0.5f, 1.0f, 0.3f));
    sphereShader->setFloat("ambientStrength", 0.4f);
    sphereShader->setFloat("diffuseStrength", 0.6f);
    sphereShader->setFloat("specularStrength", 0.3f);
    sphereShader->setFloat("emissionStrength", 0.3f);
    sphereShader->setFloat("fresnelStrength", 0.5f);
    sphereShader->setFloat("shininess", 32.0f);

    const BlendState previousBlendState = CaptureBlendState();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    targetManager->Render(*sphereShader, projection, view);

    RestoreBlendState(previousBlendState);
}
