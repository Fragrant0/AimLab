#ifndef SCREEN_SHAKE_H
#define SCREEN_SHAKE_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>

class ScreenShake
{
public:
    ScreenShake();

    void Trigger(float intensity = 1.0f, float duration = 0.3f);
    void Update(float deltaTime);

    bool IsActive() const { return m_Timer > 0.0f; }

    glm::mat4 GetViewMatrix(const glm::mat4& baseView) const;

private:
    float m_Timer;
    float m_Duration;
    float m_Intensity;
    float m_Angle;

    std::mt19937 m_RandomEngine;
};

#endif
