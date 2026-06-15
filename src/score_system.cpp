#include "score_system.h"

#include <algorithm>
#include <iostream>

namespace
{
    constexpr int COMBO_BONUS_MULTIPLIER = 5;
    constexpr float DISTANCE_THRESHOLD = 5.0f;
    constexpr float DISTANCE_BONUS_SCALE = 0.5f;
    constexpr float PRECISION_BONUS_SCALE = 5.0f;
}

const float ScoreSystem::COMBO_TIMEOUT = 2.0f;

ScoreSystem::ScoreSystem()
    : m_Score(0),
      m_Combo(0),
      m_BestScore(0),
      m_ComboTimer(0.0f)
{
}

HitResult ScoreSystem::OnHit(float distance, float precision)
{
    ++m_Combo;

    int comboBonus = (m_Combo > 1) ? (m_Combo - 1) * COMBO_BONUS_MULTIPLIER : 0;

    int distanceBonus = 0;
    if (distance > DISTANCE_THRESHOLD)
        distanceBonus = static_cast<int>((distance - DISTANCE_THRESHOLD) * DISTANCE_BONUS_SCALE);

    int precisionBonus = static_cast<int>(std::max(0.0f, precision) * PRECISION_BONUS_SCALE);

    int points = BASE_SCORE + comboBonus + distanceBonus + precisionBonus;
    m_Score += points;

    if (m_Score > m_BestScore)
        m_BestScore = m_Score;

    m_ComboTimer = 0.0f;

    std::cout << "[HIT] Combo:" << m_Combo
              << " Dist:" << static_cast<int>(distance) << "m"
              << " Prec:" << static_cast<int>(precision * 100) << "%"
              << " +" << points << "pts"
              << " Total:" << m_Score << "\n";

    return { points, precision, m_Combo, distance };
}

void ScoreSystem::Update(float deltaTime)
{
    if (m_Combo <= 0)
        return;

    m_ComboTimer += deltaTime;
    if (m_ComboTimer >= COMBO_TIMEOUT)
    {
        m_Combo = 0;
        m_ComboTimer = 0.0f;
    }
}

void ScoreSystem::Reset()
{
    m_Score = 0;
    m_Combo = 0;
    m_ComboTimer = 0.0f;
}
