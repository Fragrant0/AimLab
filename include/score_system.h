#ifndef SCORE_SYSTEM_H
#define SCORE_SYSTEM_H

struct HitResult
{
    int points;
    float precision;
    int combo;
    float distance;
};

class ScoreSystem
{
public:
    static const int BASE_SCORE = 10;
    static const float COMBO_TIMEOUT;

    ScoreSystem();

    HitResult OnHit(float distance = 0.0f, float precision = 0.0f);
    void Update(float deltaTime);

    int GetScore() const { return m_Score; }
    int GetCombo() const { return m_Combo; }
    int GetBestScore() const { return m_BestScore; }
    float GetComboTimer() const { return m_ComboTimer; }

    void Reset();

private:
    int m_Score;
    int m_Combo;
    int m_BestScore;
    float m_ComboTimer;
};

#endif
