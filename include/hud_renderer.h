#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include <glm/glm.hpp>

#include <string>

#include "lighting.h"

class FontRenderer;
class HitFeedback;
class ScoreSystem;
class UIRenderer;

namespace HUDSettings
{
    constexpr float kHitMarkerDuration = 0.2f;
}

struct DebugOverlayState
{
    bool Visible = false;
    bool PostEffectsEnabled = true;
    bool BloomEnabled = true;
    bool ShadowsEnabled = true;
    bool WireframeEnabled = false;
    int SelectedParameter = 0;
    PostProcessConfig PostProcess;
    glm::vec3 MainLightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    float SkyboxRotationDegrees = 0.0f;
};

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
                float hitMarkerTimer,
                const DebugOverlayState* debugOverlay);

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

    void RenderDebugOverlay(UIRenderer& uiRenderer,
                            FontRenderer& fontRenderer,
                            const DebugOverlayState& state,
                            int screenWidth,
                            int screenHeight,
                            float uiScale);
};

#endif
