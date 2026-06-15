#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

#include "shader.h"

struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    float life;
    float maxLife;
    glm::vec4 color;
    float size;
    bool active;

    Particle()
        : position(0.0f),
          velocity(0.0f),
          life(0.0f),
          maxLife(1.0f),
          color(1.0f),
          size(1.0f),
          active(false)
    {
    }
};

class ParticleSystem
{
public:
    static const int MAX_PARTICLES = 2000;

    ParticleSystem();
    ~ParticleSystem();

    void Initialize();
    void Update(float deltaTime);
    void Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view);
    void Cleanup();

    void EmitExplosion(const glm::vec3& position, const glm::vec3& color, int count, float minSpeed, float maxSpeed);

    int GetActiveParticleCount() const { return m_ActiveCount; }

private:
    int AllocateParticle();
    void FreeParticle(int index);
    void UpdateInstanceData();

    std::vector<Particle> m_Particles;
    std::deque<int> m_FreeList;
    int m_ActiveCount;

    unsigned int m_VAO;
    unsigned int m_QuadVBO;
    unsigned int m_InstanceVBO;

    struct InstanceData
    {
        glm::vec3 position;
        glm::vec4 color;
        float size;
    };
    std::vector<InstanceData> m_InstanceData;

    std::mt19937 m_RandomEngine;
};

#endif
