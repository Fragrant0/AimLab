#ifndef PARTICLE_RENDERER_H
#define PARTICLE_RENDERER_H

#include <glm/glm.hpp>

class ParticleSystem;

class ParticleRenderer
{
public:
    void Render(ParticleSystem* particleSystem, const glm::mat4& projection, const glm::mat4& view);
};

#endif
