#include "target_renderer.h"

#include "camera.h"
#include "gl_state_guard.h"
#include "resource_manager.h"
#include "shader.h"
#include "target_manager.h"

#include <glad/glad.h>

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

    CapabilityGuard blend(GL_BLEND, true);
    BlendFuncGuard blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    targetManager->Render(*sphereShader, projection, view);
}
