#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <map>
#include <memory>

#include "camera.h"
#include "shader.h"
#include "model.h"
#include "resource_manager.h"
#include "target_renderer.h"
#include "target_manager.h"
#include "raycast.h"
#include "score_system.h"
#include "particle_system.h"
#include "particle_renderer.h"
#include "pbr_prop_renderer.h"
#include "skybox_renderer.h"
#include "weapon.h"
#include "weapon_view_renderer.h"
#include "map_manager.h"
#include "map_resource_loader.h"
#include "ui_renderer.h"
#include "font_renderer.h"
#include "hud_renderer.h"
#include "hit_feedback.h"
#include "terrain.h"
#include "terrain_renderer.h"
#include "ecology_system.h"
#include "screen_shake.h"
#include "post_process_renderer.h"
#include "shadow_pass_renderer.h"
#include "shadow_mapper.h"

class GameManager
{
public:
    static const unsigned int SCR_WIDTH = 1600;
    static const unsigned int SCR_HEIGHT = 900;

    GameManager();
    ~GameManager();

    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    bool Initialize(GLFWwindow* window);
    void ProcessInput(GLFWwindow* window);
    void Update(float deltaTime);
    void Render();
    void Cleanup();

    Camera* GetCamera();
    float GetDeltaTime() const;

    void SetMouseFirst(bool first);
    void ProcessMouseMovement(float xpos, float ypos);
    void ProcessMouseScroll(float yoffset);
    void ProcessFramebufferSize(int width, int height);

private:
    void LoadResources();
    void SetupScene();
    void SetupTerrain();
    void SetupEcology();
    void LoadMapResources();
    void UnloadMapResources(int mapIndex);
    void RenderScene(const glm::vec3& mainLightDirection, ShadowMapper* activeShadowMapper);
    void RenderShadowMap(const glm::vec3& mainLightDirection);
    void Shoot();
    void SwitchMap(int mapIndex);
    void ResetGameState();
    void ClampPlayerToTerrain();
    void ToggleWireframe();
    void HandleDebugInput(GLFWwindow* window);
    void AdjustDebugParameter(float direction);
    void ResetDebugAdjustments();
    PostProcessConfig GetEffectivePostProcessConfig() const;
    DebugOverlayState BuildDebugOverlayState(const glm::vec3& mainLightDirection) const;

    glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::mat4& projection, const glm::mat4& view);
    glm::vec3 GetRotatedMainLightDirection() const;

    enum class DebugParameter
    {
        Exposure = 0,
        BloomIntensity,
        BloomThreshold,
        BloomRadius,
        Contrast,
        Saturation,
        Count
    };

    GLFWwindow* m_Window;
    Camera m_Camera;

    float m_DeltaTime;
    float m_LastX;
    float m_LastY;
    bool m_FirstMouse;
    bool m_WireframeMode;

    std::unique_ptr<Terrain> m_Terrain;
    std::unique_ptr<EcologySystem> m_EcologySystem;
    MapResourceLoader m_MapResourceLoader;
    PBRPropRenderer m_PBRPropRenderer;
    SkyboxRenderer m_SkyboxRenderer;
    TerrainRenderer m_TerrainRenderer;
    ShadowPassRenderer m_ShadowPassRenderer;
    HudRenderer m_HudRenderer;
    WeaponViewRenderer m_WeaponViewRenderer;
    TargetRenderer m_TargetRenderer;
    ParticleRenderer m_ParticleRenderer;

    std::map<std::string, std::unique_ptr<Model>> m_PropModels;

    std::unique_ptr<TargetManager> m_TargetManager;
    std::unique_ptr<ScoreSystem> m_ScoreSystem;
    std::unique_ptr<ParticleSystem> m_ParticleSystem;
    std::unique_ptr<Weapon> m_Weapon;
    std::unique_ptr<MapManager> m_MapManager;
    std::unique_ptr<UIRenderer> m_UIRenderer;
    std::unique_ptr<FontRenderer> m_FontRenderer;
    std::unique_ptr<HitFeedback> m_HitFeedback;
    std::unique_ptr<ScreenShake> m_ScreenShake;
    std::unique_ptr<PostProcessRenderer> m_PostProcessRenderer;
    std::unique_ptr<ShadowMapper> m_ShadowMapper;

    float m_SkyboxRotation;
    bool m_Initialized;
    bool m_WireframePressed;
    bool m_DebugOverlayVisible;
    bool m_PostEffectsEnabled;
    bool m_BloomEnabled;
    bool m_ShadowsEnabled;
    bool m_DebugF1Pressed;
    bool m_DebugF2Pressed;
    bool m_DebugF3Pressed;
    bool m_DebugF4Pressed;
    bool m_DebugTabPressed;
    bool m_DebugDecreasePressed;
    bool m_DebugIncreasePressed;
    bool m_DebugResetPressed;
    DebugParameter m_DebugSelectedParameter;
    float m_DebugExposureOffset;
    float m_DebugBloomIntensityOffset;
    float m_DebugBloomThresholdOffset;
    float m_DebugBloomRadiusOffset;
    float m_DebugContrastOffset;
    float m_DebugSaturationOffset;

    float m_HitMarkerTimer;
    bool m_HitMarkerActive;

    int m_ScreenWidth;
    int m_ScreenHeight;
};

#endif
