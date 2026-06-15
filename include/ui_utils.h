#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <algorithm>
#include <cmath>

namespace UIUtils
{
    constexpr float REFERENCE_SHORT_SIDE = 1080.0f;
    constexpr float MIN_UI_SCALE = 0.75f;
    constexpr float MAX_UI_SCALE = 1.35f;

    inline float GetUIScale(int screenWidth, int screenHeight)
    {
        float shortSide = static_cast<float>(std::min(screenWidth, screenHeight));
        float rawScale = shortSide / REFERENCE_SHORT_SIDE;
        return std::max(MIN_UI_SCALE, std::min(MAX_UI_SCALE, rawScale));
    }

    inline float SnapToPixel(float value)
    {
        return std::floor(value + 0.5f);
    }

    inline float ScaleToScreen(float baseValue, float uiScale)
    {
        return SnapToPixel(baseValue * uiScale);
    }
}

#endif
