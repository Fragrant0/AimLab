#include "screen_shake.h"

#include <cmath>

namespace
{
    constexpr float MAX_OFFSET_X = 0.07f;
    constexpr float MAX_OFFSET_Y = 0.05f;
    constexpr float MAX_ROTATION_DEG = 0.5f;
    constexpr float BASE_FREQ = 30.0f;
    constexpr float FREQ_VAR = 8.0f;
    constexpr float DECAY_POWER = 2.5f;
}

ScreenShake::ScreenShake()
    : m_Timer(0.0f),
      m_Duration(0.0f),
      m_Intensity(0.0f),
      m_Angle(0.0f),
      m_RandomEngine(std::random_device{}())
{
}

void ScreenShake::Trigger(float intensity, float duration)
{
    m_Timer = duration;
    m_Duration = duration;
    m_Intensity = glm::clamp(intensity, 0.0f, 2.0f);

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    m_Angle = angleDist(m_RandomEngine);
}

void ScreenShake::Update(float deltaTime)
{
    if (m_Timer <= 0.0f)
        return;

    m_Timer -= deltaTime;
    if (m_Timer <= 0.0f)
    {
        m_Timer = 0.0f;
        return;
    }
}

glm::mat4 ScreenShake::GetViewMatrix(const glm::mat4& baseView) const
{
    if (m_Timer <= 0.0f || m_Intensity <= 0.001f)
        return baseView;

    float t = 1.0f - (m_Timer / m_Duration);
    float decay = std::pow(1.0f - t, DECAY_POWER);
    float currentIntensity = m_Intensity * decay;

    float time = m_Duration - m_Timer;
    float freq = BASE_FREQ + m_Angle * FREQ_VAR;

    float phase1 = time * freq + m_Angle;
    float phase2 = time * freq * 1.4f + m_Angle * 2.0f;

    float envelope = std::sin(phase1);
    float envelope2 = std::sin(phase2);

    float offsetX = envelope * currentIntensity * MAX_OFFSET_X;
    float offsetY = envelope2 * currentIntensity * MAX_OFFSET_Y;

    float rotX = envelope2 * currentIntensity * MAX_ROTATION_DEG;
    float rotY = envelope * currentIntensity * MAX_ROTATION_DEG * 0.6f;

    glm::mat4 shake = glm::mat4(1.0f);
    shake = glm::translate(shake, glm::vec3(offsetX, offsetY, 0.0f));
    shake = glm::rotate(shake, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f));
    shake = glm::rotate(shake, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));

    return shake * baseView;
}
