#include "particle_system.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float GRAVITY = -9.8f;
    constexpr float DRAG = 0.97f;
    constexpr float TWO_PI = 6.283185f;
    constexpr float COLOR_FADE_THRESHOLD = 0.6f;
    constexpr float COLOR_FADE_END = 0.4f;
    constexpr float SIZE_MIN_RATIO = 0.3f;
    constexpr float ALPHA_CURVE_POWER = 2.0f;
}

ParticleSystem::ParticleSystem()
    : m_ActiveCount(0),
      m_VAO(0),
      m_QuadVBO(0),
      m_InstanceVBO(0),
      m_RandomEngine(std::random_device{}())
{
}

ParticleSystem::~ParticleSystem()
{
    Cleanup();
}

void ParticleSystem::Initialize()
{
    m_Particles.reserve(MAX_PARTICLES);
    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        m_Particles.emplace_back();
        m_FreeList.push_back(i);
    }

    m_InstanceData.reserve(MAX_PARTICLES);

    float quadVertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_InstanceVBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(7 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
}

void ParticleSystem::Update(float deltaTime)
{
    const float dragFactor = std::pow(DRAG, deltaTime * 60.0f);

    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        Particle& p = m_Particles[i];
        if (!p.active)
            continue;

        p.velocity.y += GRAVITY * deltaTime;
        p.velocity *= dragFactor;
        p.position += p.velocity * deltaTime;

        p.life -= deltaTime;

        if (p.life <= 0.0f)
            FreeParticle(i);
    }
}

void ParticleSystem::Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view)
{
    if (m_ActiveCount == 0)
        return;

    UpdateInstanceData();

    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDst);
    GLboolean depthWriteEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_VAO);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, m_ActiveCount);
    glBindVertexArray(0);

    glDepthMask(depthWriteEnabled ? GL_TRUE : GL_FALSE);
    if (!blendEnabled)
        glDisable(GL_BLEND);
    glBlendFunc(blendSrc, blendDst);
}

void ParticleSystem::Cleanup()
{
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    if (m_InstanceVBO) { glDeleteBuffers(1, &m_InstanceVBO); m_InstanceVBO = 0; }

    m_Particles.clear();
    m_FreeList.clear();
    m_InstanceData.clear();
    m_ActiveCount = 0;
}

void ParticleSystem::EmitExplosion(const glm::vec3& position, const glm::vec3& color, int count, float minSpeed, float maxSpeed)
{
    std::uniform_real_distribution<float> speedDist(minSpeed, maxSpeed);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> lifeDist(0.4f, 1.2f);
    std::uniform_real_distribution<float> sizeDist(0.25f, 0.4f);
    std::uniform_real_distribution<float> offsetDist(-0.15f, 0.15f);

    const float invCount = 1.0f / static_cast<float>(count);

    for (int i = 0; i < count; ++i)
    {
        int index = AllocateParticle();
        if (index < 0)
            break;

        Particle& p = m_Particles[index];

        glm::vec3 direction(dirDist(m_RandomEngine), dirDist(m_RandomEngine), dirDist(m_RandomEngine));
        float len = glm::length(direction);
        direction = (len > 0.001f) ? (direction / len) : glm::vec3(0.0f, 1.0f, 0.0f);

        p.velocity = direction * speedDist(m_RandomEngine);
        p.position = position + glm::vec3(offsetDist(m_RandomEngine), offsetDist(m_RandomEngine), offsetDist(m_RandomEngine));
        p.maxLife = lifeDist(m_RandomEngine);
        p.life = p.maxLife;

        float t = static_cast<float>(i) * invCount;
        glm::vec3 finalColor = color;
        if (t > COLOR_FADE_THRESHOLD)
        {
            float fadeT = (t - COLOR_FADE_THRESHOLD) / COLOR_FADE_END;
            finalColor = glm::mix(color, glm::vec3(0.35f, 0.12f, 0.05f), fadeT);
        }
        p.color = glm::vec4(finalColor, 1.0f);

        p.size = sizeDist(m_RandomEngine);
        p.active = true;
    }
}

int ParticleSystem::AllocateParticle()
{
    if (m_FreeList.empty())
        return -1;
    int index = m_FreeList.front();
    m_FreeList.pop_front();
    ++m_ActiveCount;
    return index;
}

void ParticleSystem::FreeParticle(int index)
{
    m_Particles[index].active = false;
    m_FreeList.push_back(index);
    --m_ActiveCount;
}

void ParticleSystem::UpdateInstanceData()
{
    m_InstanceData.clear();

    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        const Particle& p = m_Particles[i];
        if (!p.active)
            continue;

        float lifeRatio = glm::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        float alphaCurve = std::pow(lifeRatio, ALPHA_CURVE_POWER);
        float sizeCurve = SIZE_MIN_RATIO + (1.0f - SIZE_MIN_RATIO) * std::sqrt(lifeRatio);

        InstanceData data;
        data.position = p.position;
        data.color = p.color;
        data.color.a = alphaCurve;
        data.size = p.size * sizeCurve;

        m_InstanceData.push_back(data);
    }

    if (m_InstanceData.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_InstanceData.size() * sizeof(InstanceData), m_InstanceData.data());
}
