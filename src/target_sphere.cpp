#include "target_sphere.h"

#include <algorithm>
#include <cmath>
#include <memory>

const float TargetSphere::SHRINK_DURATION = 0.1f;

TargetSphere::TargetSphere()
    : m_State(TargetState::Inactive),
      m_Position(glm::vec3(0.0f)),
      m_Color(glm::vec3(1.0f)),
      m_Radius(0.5f),
      m_Lifetime(0.0f),
      m_MaxLifetime(5.0f),
      m_ShrinkTimer(0.0f)
{
}

TargetSphere::~TargetSphere() = default;

void TargetSphere::Update(float deltaTime)
{
    if (m_State == TargetState::Inactive)
        return;

    if (m_State == TargetState::Active)
    {
        m_Lifetime += deltaTime;
        if (m_Lifetime >= m_MaxLifetime)
        {
            Deactivate();
        }
    }
    else if (m_State == TargetState::Shrinking)
    {
        m_ShrinkTimer += deltaTime;
        if (m_ShrinkTimer >= SHRINK_DURATION)
        {
            Deactivate();
        }
    }
}

void TargetSphere::Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view)
{
    if (m_State == TargetState::Inactive || !m_Mesh)
        return;

    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_Position);

    glm::vec3 renderColor = m_Color;
    float alpha = 1.0f;

    if (m_State == TargetState::Shrinking)
    {
        float t = m_ShrinkTimer / SHRINK_DURATION;
        float scale = 1.0f - t * t;
        if (scale < 0.0f) scale = 0.0f;
        model = glm::scale(model, glm::vec3(scale));
        alpha = 1.0f - t;
        renderColor = glm::mix(glm::vec3(1.0f), m_Color, std::min(t * 3.0f, 1.0f));
    }

    shader.setMat4("model", model);
    shader.setVec3("sphereColor", renderColor);
    shader.setFloat("alpha", alpha);

    m_Mesh->Draw(shader);
}

void TargetSphere::Activate(float radius, const glm::vec3& position, const glm::vec3& color, float lifetime)
{
    m_Radius = radius;
    m_Position = position;
    m_Color = color;
    m_State = TargetState::Active;
    m_Lifetime = 0.0f;
    m_MaxLifetime = lifetime;
    m_ShrinkTimer = 0.0f;

    if (!m_Mesh)
    {
        m_Mesh = std::make_unique<SphereMesh>(radius, 36, 18);
    }
}

void TargetSphere::Deactivate()
{
    m_State = TargetState::Inactive;
}

void TargetSphere::OnHit()
{
    if (m_State != TargetState::Active)
        return;
    m_State = TargetState::Shrinking;
    m_ShrinkTimer = 0.0f;
}

bool TargetSphere::CheckCollision(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& t) const
{
    if (m_State != TargetState::Active)
        return false;

    glm::vec3 oc = rayOrigin - m_Position;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - m_Radius * m_Radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f)
        return false;

    float sqrtDisc = sqrtf(discriminant);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    if (t1 > 0.0f)
    {
        t = t1;
        return true;
    }
    if (t2 > 0.0f)
    {
        t = t2;
        return true;
    }

    return false;
}
