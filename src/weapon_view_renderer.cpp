#include "weapon_view_renderer.h"

#include "camera.h"
#include "resource_manager.h"
#include "shader.h"
#include "weapon.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

void WeaponViewRenderer::Render(Weapon* weapon, const Camera& camera, int screenWidth, int screenHeight)
{
    if (!weapon)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* weaponShader = rm.GetShader("weapon");
    Shader* flashShader = rm.GetShader("muzzle_flash");
    if (!weaponShader)
        return;

    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glClear(GL_DEPTH_BUFFER_BIT);

    const float currentTime = static_cast<float>(glfwGetTime());
    weapon->Render(*weaponShader, flashShader, camera, screenWidth, screenHeight, currentTime);

    glDepthFunc(static_cast<GLenum>(prevDepthFunc));
}
