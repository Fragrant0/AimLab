#ifndef TARGET_SPHERE_H
#define TARGET_SPHERE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"
#include "sphere_mesh.h"

enum class TargetState
{
    Inactive,
    Active,
    Shrinking
};

class TargetSphere
{
public:
    static const float SHRINK_DURATION;

    TargetSphere();
    ~TargetSphere();

    void Update(float deltaTime);
    void Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view);

    void Activate(float radius, const glm::vec3& position, const glm::vec3& color, float lifetime = 5.0f);
    void Deactivate();
    void OnHit();

    bool IsActive() const { return m_State == TargetState::Active; }
    bool IsFullyDead() const { return m_State == TargetState::Inactive; }

    glm::vec3 GetPosition() const { return m_Position; }
    float GetRadius() const { return m_Radius; }
    glm::vec3 GetColor() const { return m_Color; }

    void SetPosition(const glm::vec3& position) { m_Position = position; }

    bool CheckCollision(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& t) const;

private:
    TargetState m_State;
    glm::vec3 m_Position;
    glm::vec3 m_Color;
    float m_Radius;
    float m_Lifetime;
    float m_MaxLifetime;
    float m_ShrinkTimer;

    SphereMesh* m_Mesh;
};

#endif
