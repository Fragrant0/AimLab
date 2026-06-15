#include "hit_feedback.h"
#include "font_renderer.h"
#include "ui_utils.h"

#include <algorithm>
#include <sstream>

namespace
{
    constexpr float HIT_TEXT_OFFSET_X = 28.0f;
    constexpr float HIT_TEXT_OFFSET_Y = 20.0f;
    constexpr float HIT_TEXT_BASE_SCALE = 2.0f;
    constexpr float HIT_TEXT_POP_SCALE = 1.8f;
    constexpr float HIT_TEXT_VELOCITY_X = 60.0f;
    constexpr float HIT_TEXT_VELOCITY_Y = 90.0f;
    constexpr float VELOCITY_DAMPING = 0.8f;
    constexpr float POP_PHASE_DURATION = 0.15f;
    constexpr float FADE_PHASE_START = 0.4f;

    glm::vec3 GetHitColor(float precision, int combo)
    {
        if (combo > 1)
            return glm::vec3(1.0f, 0.5f, 0.0f);
        if (precision >= 0.8f)
            return glm::vec3(1.0f, 0.85f, 0.0f);
        if (precision >= 0.5f)
            return glm::vec3(0.9f, 0.9f, 0.9f);
        return glm::vec3(1.0f, 0.4f, 0.3f);
    }

    float ComputePopScale(float t, float baseScale)
    {
        if (t >= POP_PHASE_DURATION)
            return baseScale;
        float easeT = t / POP_PHASE_DURATION;
        return baseScale * (HIT_TEXT_POP_SCALE - (HIT_TEXT_POP_SCALE - 1.0f) * easeT);
    }

    float ComputeAlpha(float t)
    {
        if (t <= FADE_PHASE_START)
            return 1.0f;
        float fadeT = (t - FADE_PHASE_START) / (1.0f - FADE_PHASE_START);
        return 1.0f - fadeT * fadeT;
    }
}

HitFeedback::HitFeedback()
    : m_ScreenWidth(1920),
      m_ScreenHeight(1080)
{
}

HitFeedback::~HitFeedback()
{
}

void HitFeedback::Initialize(int screenWidth, int screenHeight)
{
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;
}

void HitFeedback::AddHitFeedback(float screenX, float screenY,
                                  int points, float precision, int combo)
{
    float uiScale = UIUtils::GetUIScale(m_ScreenWidth, m_ScreenHeight);

    FloatingText ft;
    ft.x = screenX + HIT_TEXT_OFFSET_X * uiScale;
    ft.y = screenY + HIT_TEXT_OFFSET_Y * uiScale;
    ft.maxLife = 1.5f;
    ft.life = ft.maxLife;
    ft.alpha = 1.0f;
    ft.baseScale = HIT_TEXT_BASE_SCALE * uiScale;
    ft.scale = ft.baseScale * HIT_TEXT_POP_SCALE;
    ft.velocityX = HIT_TEXT_VELOCITY_X * uiScale;
    ft.velocityY = HIT_TEXT_VELOCITY_Y * uiScale;
    ft.text = "+" + std::to_string(points);
    ft.color = GetHitColor(precision, combo);

    m_Texts.push_back(ft);
}

void HitFeedback::Update(float deltaTime)
{
    for (auto& ft : m_Texts)
    {
        ft.life -= deltaTime;
        ft.x += ft.velocityX * deltaTime;
        ft.y += ft.velocityY * deltaTime;

        float t = 1.0f - (ft.life / ft.maxLife);

        ft.scale = ComputePopScale(t, ft.baseScale);
        ft.alpha = ComputeAlpha(t);

        ft.velocityX *= (1.0f - deltaTime * VELOCITY_DAMPING);
        ft.velocityY *= (1.0f - deltaTime * VELOCITY_DAMPING);
    }

    m_Texts.erase(
        std::remove_if(m_Texts.begin(), m_Texts.end(),
                       [](const FloatingText& ft) { return ft.life <= 0.0f; }),
        m_Texts.end());
}

void HitFeedback::Render(FontRenderer& fontRenderer)
{
    for (const auto& ft : m_Texts)
    {
        if (ft.alpha <= 0.01f)
            continue;

        float textW = fontRenderer.GetTextWidth(ft.scale, ft.text);
        float startX = ft.x - textW * 0.5f;

        fontRenderer.DrawText(startX, ft.y, ft.scale,
                              ft.text, ft.color, ft.alpha);
    }
}

void HitFeedback::SetScreenSize(int w, int h)
{
    m_ScreenWidth = w;
    m_ScreenHeight = h;
}

void HitFeedback::Clear()
{
    m_Texts.clear();
}
