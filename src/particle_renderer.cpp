#include "particle_renderer.h"

#include "particle_system.h"
#include "resource_manager.h"
#include "shader.h"

void ParticleRenderer::Render(ParticleSystem* particleSystem, const glm::mat4& projection, const glm::mat4& view)
{
    if (!particleSystem)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* particleShader = rm.GetShader("particle");
    if (!particleShader)
        return;

    particleSystem->Render(*particleShader, projection, view);
}
