#include "hud_renderer.h"

#include "font_renderer.h"
#include "hit_feedback.h"
#include "score_system.h"
#include "ui_renderer.h"
#include "ui_utils.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace UILayout
{
    constexpr float SCORE_MARGIN = 30.0f;
    constexpr float SCORE_COLUMN_GAP = 14.0f;
    constexpr float SCORE_ROW_GAP = 4.0f;
    constexpr float HINT_MARGIN = 30.0f;
    constexpr float HINT_COLUMN_GAP = 15.0f;
    constexpr float HINT_ROW_GAP = 5.0f;
    constexpr float HINT_TOP_OFFSET = 5.0f;
    constexpr int SCORE_RESERVED_DIGITS = 6;

    constexpr float SCORE_FONT_SCALE = 0.72f;
    constexpr float COMBO_FONT_SCALE = 0.62f;
    constexpr float HINT_FONT_SCALE = 0.50f;
    constexpr float BEST_FONT_SCALE = 0.50f;

    constexpr float COMBO_BAR_WIDTH = 100.0f;
    constexpr float COMBO_BAR_HEIGHT = 3.0f;

    constexpr float HIT_MARKER_LEN = 10.0f;
    constexpr float HIT_MARKER_GAP = 5.0f;
    constexpr float HIT_MARKER_THICKNESS = 2.0f;
    constexpr float CROSSHAIR_SIZE = 12.0f;
    constexpr float CROSSHAIR_GAP = 4.0f;
    constexpr float CROSSHAIR_THICKNESS = 2.0f;

    constexpr float DEBUG_PANEL_WIDTH = 360.0f;
    constexpr float DEBUG_PANEL_HEIGHT = 150.0f;
    constexpr float DEBUG_PANEL_PADDING = 6.0f;
    constexpr float DEBUG_FONT_SCALE = 0.30f;
    constexpr float DEBUG_ROW_GAP = 1.0f;
}

namespace
{
    const glm::vec3 kCrosshairColor(0.18f, 0.96f, 0.78f);
    const glm::vec3 kHitMarkerColor(0.88f, 0.96f, 1.0f);
    const glm::vec3 kBestTextColor(0.34f, 0.10f, 0.88f);
    const glm::vec3 kScoreTextColor(0.20f, 0.98f, 0.80f);
    const glm::vec3 kComboTextColor(1.0f, 0.72f, 0.22f);
    const glm::vec3 kComboBarBackgroundColor(0.10f, 0.16f, 0.18f);
    const glm::vec3 kComboBarFillColor(1.0f, 0.76f, 0.24f);

    struct ScoreboardRow
    {
        std::string label;
        std::string value;
        float scale;
        glm::vec3 labelColor;
        glm::vec3 valueColor;
    };

    struct HintRow
    {
        const char* key;
        const char* action;
        glm::vec3 keyColor;
        glm::vec3 actionColor;
    };

    const char* DebugParamName(int index)
    {
        switch (index)
        {
        case 0: return "EXP";
        case 1: return "BLM";
        case 2: return "THR";
        case 3: return "RAD";
        case 4: return "CON";
        case 5: return "SAT";
        default: return "UNK";
        }
    }

    std::string FormatFloat(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << value;
        return stream.str();
    }

    std::string DebugParamValue(int index, const PostProcessConfig& post)
    {
        switch (index)
        {
        case 0: return FormatFloat(post.Exposure);
        case 1: return FormatFloat(post.Bloom.Intensity);
        case 2: return FormatFloat(post.Bloom.Threshold);
        case 3: return FormatFloat(post.Bloom.Radius);
        case 4: return FormatFloat(post.Contrast);
        case 5: return FormatFloat(post.Saturation);
        default: return "--";
        }
    }

    std::string FormatVec3(const glm::vec3& value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << value.x << "," << value.y << "," << value.z;
        return stream.str();
    }

    const char* OnOff(bool enabled)
    {
        return enabled ? "ON" : "OFF";
    }
}

void HudRenderer::Render(UIRenderer& uiRenderer,
                         FontRenderer& fontRenderer,
                         const ScoreSystem& scoreSystem,
                         HitFeedback* hitFeedback,
                         const std::string& mapName,
                         int screenWidth,
                         int screenHeight,
                         bool hitMarkerActive,
                         float hitMarkerTimer,
                         const DebugOverlayState* debugOverlay)
{
    const float uiScale = UIUtils::GetUIScale(screenWidth, screenHeight);

    RenderCenterOverlay(uiRenderer, screenWidth, screenHeight, uiScale, hitMarkerActive, hitMarkerTimer);
    RenderScoreboard(uiRenderer, fontRenderer, scoreSystem, screenWidth, screenHeight, uiScale);
    RenderHintPanel(fontRenderer, mapName, screenWidth, screenHeight, uiScale);

    if (hitFeedback)
        hitFeedback->Render(fontRenderer);

    if (debugOverlay && debugOverlay->Visible)
        RenderDebugOverlay(uiRenderer, fontRenderer, *debugOverlay, screenWidth, screenHeight, uiScale);

    fontRenderer.Flush();
}

void HudRenderer::RenderCenterOverlay(UIRenderer& uiRenderer,
                                      int screenWidth,
                                      int screenHeight,
                                      float uiScale,
                                      bool hitMarkerActive,
                                      float hitMarkerTimer)
{
    const float w = static_cast<float>(screenWidth);
    const float h = static_cast<float>(screenHeight);
    const float centerX = UIUtils::SnapToPixel(w * 0.5f);
    const float centerY = UIUtils::SnapToPixel(h * 0.5f);

    const float crosshairSize = std::max(6.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_SIZE, uiScale));
    const float crosshairGap = std::max(2.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_GAP, uiScale));
    const float crosshairThickness = std::max(1.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_THICKNESS, uiScale));

    uiRenderer.Begin();
    uiRenderer.DrawCrosshair(centerX, centerY, crosshairSize, crosshairGap,
                             crosshairThickness, kCrosshairColor);

    if (hitMarkerActive)
    {
        const float alpha = 1.0f - hitMarkerTimer / HUDSettings::kHitMarkerDuration;
        const float markerLength = std::max(6.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_LEN, uiScale));
        const float markerGap = std::max(3.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_GAP, uiScale));
        const float markerWidth = std::max(1.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_THICKNESS, uiScale));

        auto drawMarker = [&](float dx, float dy) {
            uiRenderer.DrawLine(centerX - dx * markerGap - dx * markerLength, centerY - dy * markerGap - dy * markerLength,
                                centerX - dx * markerGap, centerY - dy * markerGap, kHitMarkerColor, markerWidth, alpha);
        };

        drawMarker(-1.0f, -1.0f);
        drawMarker(1.0f, -1.0f);
        drawMarker(-1.0f, 1.0f);
        drawMarker(1.0f, 1.0f);
    }

    uiRenderer.End();
}

void HudRenderer::RenderScoreboard(UIRenderer& uiRenderer,
                                   FontRenderer& fontRenderer,
                                   const ScoreSystem& scoreSystem,
                                   int screenWidth,
                                   int screenHeight,
                                   float uiScale)
{
    const float w = static_cast<float>(screenWidth);
    const float h = static_cast<float>(screenHeight);
    const float margin = UIUtils::ScaleToScreen(UILayout::SCORE_MARGIN, uiScale);
    const float columnGap = UIUtils::ScaleToScreen(UILayout::SCORE_COLUMN_GAP, uiScale);
    const float rowGap = UIUtils::ScaleToScreen(UILayout::SCORE_ROW_GAP, uiScale);

    std::vector<ScoreboardRow> rows;
    rows.reserve(2);

    rows.push_back({"BEST", std::to_string(scoreSystem.GetBestScore()), UILayout::BEST_FONT_SCALE * uiScale,
                    kBestTextColor, kBestTextColor});

    rows.push_back({"SCORE", std::to_string(scoreSystem.GetScore()), UILayout::SCORE_FONT_SCALE * uiScale,
                    kScoreTextColor, kScoreTextColor});

    float comboScale = UILayout::COMBO_FONT_SCALE * uiScale;
    const std::string reservedValueText(UILayout::SCORE_RESERVED_DIGITS, '0');

    std::string comboText;
    float comboTextWidth = 0.0f;
    float comboBarWidth = UIUtils::ScaleToScreen(UILayout::COMBO_BAR_WIDTH, uiScale);
    if (scoreSystem.GetCombo() > 0)
    {
        comboText = "X" + std::to_string(scoreSystem.GetCombo());
        comboTextWidth = fontRenderer.GetTextWidth(comboScale, comboText);
    }

    auto getValueWidth = [&](const ScoreboardRow& row) {
        return std::max(fontRenderer.GetTextWidth(row.scale, row.value),
                        fontRenderer.GetTextWidth(row.scale, reservedValueText));
    };

    auto getPanelWidth = [&]() {
        float width = std::max(comboTextWidth, comboBarWidth);
        for (const auto& row : rows)
        {
            const float labelWidth = fontRenderer.GetTextWidth(row.scale, row.label);
            width = std::max(width, labelWidth + columnGap + getValueWidth(row));
        }
        return width;
    };

    const float panelWidth = getPanelWidth();
    const float availableWidth = std::max(1.0f, w - margin * 2.0f);
    if (panelWidth > availableWidth)
    {
        const float shrink = availableWidth / panelWidth;
        for (auto& row : rows)
            row.scale *= shrink;
        comboScale *= shrink;
        comboBarWidth *= shrink;

        if (!comboText.empty())
            comboTextWidth = fontRenderer.GetTextWidth(comboScale, comboText);
    }

    const float valueRightX = UIUtils::SnapToPixel(w - margin);
    float currentTop = UIUtils::SnapToPixel(h - margin);

    for (const auto& row : rows)
    {
        const float lineHeight = UIUtils::SnapToPixel(fontRenderer.GetLineHeight(row.scale));
        const float textY = UIUtils::SnapToPixel(currentTop - lineHeight);
        const float labelWidth = fontRenderer.GetTextWidth(row.scale, row.label);
        const float valueWidth = fontRenderer.GetTextWidth(row.scale, row.value);
        const float valueLeftX = UIUtils::SnapToPixel(valueRightX - valueWidth);
        const float labelLeftX = UIUtils::SnapToPixel(valueLeftX - columnGap - labelWidth);

        fontRenderer.DrawText(labelLeftX, textY,
                              row.scale, row.label, row.labelColor);
        fontRenderer.DrawText(valueLeftX, textY,
                              row.scale, row.value, row.valueColor);

        currentTop = UIUtils::SnapToPixel(textY - rowGap);
    }

    if (comboText.empty())
        return;

    const float comboLineHeight = UIUtils::SnapToPixel(fontRenderer.GetLineHeight(comboScale));
    const float comboY = UIUtils::SnapToPixel(currentTop - comboLineHeight);
    const float comboX = UIUtils::SnapToPixel(valueRightX - comboTextWidth);

    fontRenderer.DrawText(comboX, comboY, comboScale, comboText, kComboTextColor);

    const float comboBarHeight = std::max(2.0f, UIUtils::ScaleToScreen(UILayout::COMBO_BAR_HEIGHT, uiScale));
    const float comboVisualWidth = comboBarWidth;
    float comboBarX = UIUtils::SnapToPixel(valueRightX - comboVisualWidth);
    comboBarX = std::max(margin, std::min(comboBarX, w - margin - comboVisualWidth));
    const float comboBarY = UIUtils::SnapToPixel(comboY - comboBarHeight - std::max(2.0f, rowGap * 0.5f));
    float comboProgress = 1.0f - scoreSystem.GetComboTimer() / ScoreSystem::COMBO_TIMEOUT;
    comboProgress = glm::clamp(comboProgress, 0.0f, 1.0f);

    uiRenderer.Begin();
    uiRenderer.DrawRect(comboBarX, comboBarY, comboVisualWidth, comboBarHeight, kComboBarBackgroundColor);
    uiRenderer.DrawRect(comboBarX, comboBarY, comboVisualWidth * comboProgress, comboBarHeight,
                        kComboBarFillColor);
    uiRenderer.End();
}

void HudRenderer::RenderHintPanel(FontRenderer& fontRenderer,
                                  const std::string& mapName,
                                  int screenWidth,
                                  int screenHeight,
                                  float uiScale)
{
    const HintRow kHintRows[] = {
        {"MAP", mapName.c_str(), glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(1.0f, 0.82f, 0.24f)},
        {"WASD", "MOVE", glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(1.0f, 0.82f, 0.24f)},
        {"Space", "WIREFRAME", glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(0.72f, 0.84f, 1.0f)},
        {"F1", "DEBUG", glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(0.72f, 0.84f, 1.0f)}
    };

    const float h = static_cast<float>(screenHeight);
    const float margin = UIUtils::ScaleToScreen(UILayout::HINT_MARGIN, uiScale);
    const float topOffset = UIUtils::ScaleToScreen(UILayout::HINT_TOP_OFFSET, uiScale);
    const float columnGap = UIUtils::ScaleToScreen(UILayout::HINT_COLUMN_GAP, uiScale);
    const float rowGap = UIUtils::ScaleToScreen(UILayout::HINT_ROW_GAP, uiScale);
    const float textScale = UILayout::HINT_FONT_SCALE * uiScale;

    float maxKeyWidth = 0.0f;
    for (const auto& row : kHintRows)
        maxKeyWidth = std::max(maxKeyWidth, fontRenderer.GetTextWidth(textScale, row.key));

    const float keyX = margin;
    const float actionX = UIUtils::SnapToPixel(keyX + maxKeyWidth + columnGap);
    float currentTop = UIUtils::SnapToPixel(h - margin - topOffset);

    for (const auto& row : kHintRows)
    {
        const float lineHeight = UIUtils::SnapToPixel(fontRenderer.GetLineHeight(textScale));
        const float textY = UIUtils::SnapToPixel(currentTop - lineHeight);

        fontRenderer.DrawText(keyX, textY, textScale, row.key, row.keyColor);
        fontRenderer.DrawText(actionX, textY, textScale, row.action, row.actionColor);

        currentTop = UIUtils::SnapToPixel(textY - rowGap);
    }
}

void HudRenderer::RenderDebugOverlay(UIRenderer& uiRenderer,
                                     FontRenderer& fontRenderer,
                                     const DebugOverlayState& state,
                                     int screenWidth,
                                     int screenHeight,
                                     float uiScale)
{
    const float w = static_cast<float>(screenWidth);
    const float margin = UIUtils::ScaleToScreen(UILayout::HINT_MARGIN, uiScale);
    const float panelWidth = std::min(UIUtils::ScaleToScreen(UILayout::DEBUG_PANEL_WIDTH, uiScale),
                                      std::max(1.0f, w - margin * 2.0f));
    const float panelHeight = UIUtils::ScaleToScreen(UILayout::DEBUG_PANEL_HEIGHT, uiScale);
    const float panelX = margin;
    const float panelY = margin;
    const float padding = UIUtils::ScaleToScreen(UILayout::DEBUG_PANEL_PADDING, uiScale);

    uiRenderer.Begin();
    uiRenderer.DrawRect(panelX, panelY, panelWidth, panelHeight, glm::vec3(0.02f, 0.025f, 0.03f), 0.58f);
    uiRenderer.DrawRectOutline(panelX, panelY, panelWidth, panelHeight, glm::vec3(0.14f, 0.95f, 0.74f), 1.0f);
    uiRenderer.End();

    const float textScale = UILayout::DEBUG_FONT_SCALE * uiScale;
    const float lineHeight = UIUtils::SnapToPixel(fontRenderer.GetLineHeight(textScale));
    const float rowGap = UIUtils::ScaleToScreen(UILayout::DEBUG_ROW_GAP, uiScale);
    const float textX = panelX + padding;
    float currentTop = UIUtils::SnapToPixel(panelY + panelHeight - padding);

    auto drawLine = [&](const std::string& text, const glm::vec3& color) {
        const float textY = UIUtils::SnapToPixel(currentTop - lineHeight);
        fontRenderer.DrawText(textX, textY, textScale, text, color);
        currentTop = UIUtils::SnapToPixel(textY - rowGap);
    };

    const glm::vec3 titleColor(0.18f, 0.96f, 0.78f);
    const glm::vec3 textColor(0.88f, 0.94f, 0.98f);
    const glm::vec3 mutedColor(0.56f, 0.66f, 0.74f);
    const glm::vec3 selectedColor(1.0f, 0.80f, 0.30f);

    const PostProcessConfig& post = state.PostProcess;
    drawLine(std::string("DBG P:") + OnOff(state.PostEffectsEnabled) +
             " B:" + OnOff(state.BloomEnabled) +
             " S:" + OnOff(state.ShadowsEnabled), titleColor);
    drawLine("F1/F2/F3/F4  TAB  <- ->  R", mutedColor);
    drawLine(std::string("> ") + DebugParamName(state.SelectedParameter) +
             " " + DebugParamValue(state.SelectedParameter, post), selectedColor);
    drawLine("EXP " + FormatFloat(post.Exposure) +
             " BLM " + FormatFloat(post.Bloom.Intensity) +
             " THR " + FormatFloat(post.Bloom.Threshold), textColor);
    drawLine("RAD " + FormatFloat(post.Bloom.Radius) +
             " CON " + FormatFloat(post.Contrast) +
             " SAT " + FormatFloat(post.Saturation), textColor);
    drawLine(std::string("W:") + OnOff(state.WireframeEnabled) +
             " L:" + FormatVec3(state.MainLightDirection), mutedColor);
    drawLine("SKY " + FormatFloat(state.SkyboxRotationDegrees), mutedColor);
}
