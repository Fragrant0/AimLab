#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include <string>

class FontRenderer;
class HitFeedback;
class ScoreSystem;
class UIRenderer;

namespace HUDSettings
{
    constexpr float kHitMarkerDuration = 0.2f;
}

class HudRenderer
{
public:
    void Render(UIRenderer& uiRenderer,
                FontRenderer& fontRenderer,
                const ScoreSystem& scoreSystem,
                HitFeedback* hitFeedback,
                const std::string& mapName,
                int screenWidth,
                int screenHeight,
                bool hitMarkerActive,
                float hitMarkerTimer);

private:
    void RenderCenterOverlay(UIRenderer& uiRenderer,
                             int screenWidth,
                             int screenHeight,
                             float uiScale,
                             bool hitMarkerActive,
                             float hitMarkerTimer);

    void RenderScoreboard(UIRenderer& uiRenderer,
                          FontRenderer& fontRenderer,
                          const ScoreSystem& scoreSystem,
                          int screenWidth,
                          int screenHeight,
                          float uiScale);

    void RenderHintPanel(FontRenderer& fontRenderer,
                         const std::string& mapName,
                         int screenWidth,
                         int screenHeight,
                         float uiScale);
};

#endif
