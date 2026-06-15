#ifndef HIT_FEEDBACK_H
#define HIT_FEEDBACK_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>

class FontRenderer;

struct FloatingText
{
    float x, y;
    std::string text;
    glm::vec3 color;
    float alpha;
    float life;
    float maxLife;
    float scale;
    float baseScale;
    float velocityX;
    float velocityY;
};

class HitFeedback
{
public:
    HitFeedback();
    ~HitFeedback();

    void Initialize(int screenWidth, int screenHeight);

    void AddHitFeedback(float screenX, float screenY,
                        int points, float precision, int combo);
    void Update(float deltaTime);
    void Render(FontRenderer& fontRenderer);

    void SetScreenSize(int w, int h);

    void Clear();

private:
    std::vector<FloatingText> m_Texts;
    int m_ScreenWidth;
    int m_ScreenHeight;
};

#endif
