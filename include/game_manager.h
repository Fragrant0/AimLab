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
#include "target_manager.h"
#include "raycast.h"
#include "score_system.h"
#include "particle_system.h"
#include "pbr_prop_renderer.h"
#include "weapon.h"
#include "map_manager.h"
#include "ui_renderer.h"
#include "font_renderer.h"
#include "hit_feedback.h"
#include "terrain.h"
#include "ecology_system.h"
#include "screen_shake.h"
#include "post_process_renderer.h"
#include "shadow_mapper.h"

class GameManager
{
public:
    static const unsigned int SCR_WIDTH = 1920;
    static const unsigned int SCR_HEIGHT = 1080;

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
    void RenderScene();
    void RenderSkybox(glm::mat4 projection, glm::mat4 view);
    void RenderTerrain(glm::mat4 projection, glm::mat4 view);
    void RenderPlane(glm::mat4 projection, glm::mat4 view);
    void RenderProps(glm::mat4 projection, glm::mat4 view);
    void RenderTargets(glm::mat4 projection, glm::mat4 view);
    void RenderParticles(glm::mat4 projection, glm::mat4 view);
    void RenderWeapon(glm::mat4 projection, glm::mat4 view);
    void RenderUI();
    void RenderCenterOverlay(float uiScale);
    void RenderScoreboard(float uiScale);
    void RenderHintPanel(float uiScale);
    void RenderShadowMap(const glm::vec3& mainLightDirection);
    void RenderShadowCasters(Shader& depthShader);
    void RenderShadowTerrain(Shader& depthShader);
    void RenderShadowPlane(Shader& depthShader);
    void RenderShadowProps(Shader& depthShader);
    void Shoot();
    void SwitchMap(int mapIndex);
    void ResetGameState();
    void ClampPlayerToTerrain();
    void ToggleWireframe();
    float GetUIScale() const;

    glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::mat4& projection, const glm::mat4& view);
    glm::vec3 GetRotatedMainLightDirection() const;
    void ApplyTerrainLighting(Shader& shader, const glm::vec3& mainLightDirection);

    GLFWwindow* m_Window;
    Camera m_Camera;

    float m_DeltaTime;
    float m_LastX;
    float m_LastY;
    bool m_FirstMouse;
    bool m_WireframeMode;

    unsigned int m_PlaneVAO;
    unsigned int m_PlaneVBO;
    unsigned int m_SkyboxVAO;
    unsigned int m_SkyboxVBO;

    std::unique_ptr<Terrain> m_Terrain;
    std::unique_ptr<EcologySystem> m_EcologySystem;
    PBRPropRenderer m_PBRPropRenderer;

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

    float m_HitMarkerTimer;
    bool m_HitMarkerActive;

    int m_ScreenWidth;
    int m_ScreenHeight;
};

#endif
