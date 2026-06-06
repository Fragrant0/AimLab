#include "game_manager.h"
#include "gl_state_guard.h"
#include "render_settings.h"

#include <algorithm>
#include <cmath>
#include <iostream>

GameManager::GameManager()
    : m_Window(nullptr),
      m_Camera(glm::vec3(0.0f, 0.0f, 3.0f)),
      m_DeltaTime(0.0f),
      m_FpsTimer(0.0f),
      m_FpsFrameCount(0),
      m_CurrentFps(0),
      m_LastX(SCR_WIDTH / 2.0f),
      m_LastY(SCR_HEIGHT / 2.0f),
      m_FirstMouse(true),
      m_WireframeMode(false),
      m_SkyboxRotation(0.0f),
      m_Initialized(false),
      m_WireframePressed(false),
      m_DebugOverlayVisible(false),
      m_PostEffectsEnabled(true),
      m_UnderwaterEnabled(false),
      m_IBLEnabled(true),
      m_PCSSEnabled(true),
      m_ShadowsEnabled(true),
      m_DebugF1Pressed(false),
      m_DebugF2Pressed(false),
      m_DebugF3Pressed(false),
      m_DebugF4Pressed(false),
      m_DebugF5Pressed(false),
      m_DebugF6Pressed(false),
      m_DebugF7Pressed(false),
      m_DebugTabPressed(false),
      m_FreeFlyMode(false),
      m_PixelateEnabled(false),
      m_DebugDecreasePressed(false),
      m_DebugIncreasePressed(false),
      m_DebugResetPressed(false),
      m_DebugSelectedParameter(DebugParameter::ShadowBias),
      m_DebugShadowBiasOffset(0.0f),
      m_DebugPixelSizeOffset(0.0f),
      m_HitMarkerTimer(0.0f),
      m_HitMarkerActive(false),
      m_ScreenWidth(SCR_WIDTH),
      m_ScreenHeight(SCR_HEIGHT)
{
}

GameManager::~GameManager()
{
    Cleanup();
}

bool GameManager::Initialize(GLFWwindow* window)
{
    m_Window = window;

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0)
    {
        m_ScreenWidth = framebufferWidth;
        m_ScreenHeight = framebufferHeight;
        m_LastX = framebufferWidth * 0.5f;
        m_LastY = framebufferHeight * 0.5f;
        glViewport(0, 0, framebufferWidth, framebufferHeight);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    try
    {
        m_MapManager = std::make_unique<MapManager>();
        m_MapManager->Initialize();

        LoadResources();
        SetupScene();

        m_TargetManager = std::make_unique<TargetManager>();
        m_TargetManager->Initialize();

        const MapConfig& currentMap = m_MapManager->GetCurrentMap();
        m_UnderwaterEnabled = currentMap.PostProcess.UnderwaterEnabled;
        m_TargetManager->SetTargetWall(
            currentMap.TargetArea.WallDistance,
            currentMap.TargetArea.WallWidth,
            currentMap.TargetArea.WallHeight,
            currentMap.TargetArea.WallCenterY,
            currentMap.TargetArea.Lifetime);

        m_ScoreSystem = std::make_unique<ScoreSystem>();

        m_ParticleSystem = std::make_unique<ParticleSystem>();
        m_ParticleSystem->Initialize();

        m_Weapon = std::make_unique<Weapon>();
        m_Weapon->Initialize();

        m_UIRenderer = std::make_unique<UIRenderer>();
        m_UIRenderer->Initialize(m_ScreenWidth, m_ScreenHeight);

        m_FontRenderer = std::make_unique<FontRenderer>();
        m_FontRenderer->Initialize(m_ScreenWidth, m_ScreenHeight, "resources/fonts/consola.ttf");

        m_HitFeedback = std::make_unique<HitFeedback>();
        m_HitFeedback->Initialize(m_ScreenWidth, m_ScreenHeight);

        m_ScreenShake = std::make_unique<ScreenShake>();

        m_PostProcessRenderer = std::make_unique<PostProcessRenderer>();
        m_PostProcessRenderer->Initialize(m_ScreenWidth, m_ScreenHeight);

        m_ShadowMapper = std::make_unique<ShadowMapper>();
        m_ShadowMapper->Initialize(RenderSettings::kShadowMapSize);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] Initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }

    m_Initialized = true;
    return true;
}

void GameManager::LoadResources()
{
    ResourceManager& rm = ResourceManager::GetInstance();

    rm.LoadShader("pbr_model", "shaders/pbr_model.vs", "shaders/pbr_model.fs");
    rm.LoadShader("plane", "shaders/plane.vs", "shaders/plane.fs");
    rm.LoadShader("terrain", "shaders/terrain.vs", "shaders/terrain.fs");
    rm.LoadShader("skybox", "shaders/skybox.vs", "shaders/skybox.fs");
    rm.LoadShader("sphere", "shaders/sphere.vs", "shaders/sphere.fs");
    rm.LoadShader("ecology_sphere", "shaders/ecology_sphere.vs", "shaders/ecology_sphere.fs");
    rm.LoadShader("ecology_shadow_depth", "shaders/ecology_shadow_depth.vs", "shaders/shadow_depth.fs");
    rm.LoadShader("particle", "shaders/particle.vs", "shaders/particle.fs");
    rm.LoadShader("weapon", "shaders/weapon.vs", "shaders/weapon.fs");
    rm.LoadShader("muzzle_flash", "shaders/muzzle_flash.vs", "shaders/muzzle_flash.fs");

    LoadMapResources();

    Shader* planeShader = rm.GetShader("plane");
    if (planeShader)
    {
        planeShader->use();
        planeShader->setInt("texture1", 0);
    }

    Shader* terrainShader = rm.GetShader("terrain");
    if (terrainShader)
    {
        terrainShader->use();
        terrainShader->setInt("diffuseTexture", 0);
    }

    Shader* skyboxShader = rm.GetShader("skybox");
    if (skyboxShader)
    {
        skyboxShader->use();
        skyboxShader->setInt("skybox", 0);
    }

    Shader* pbrShader = rm.GetShader("pbr_model");
    if (pbrShader)
    {
        pbrShader->use();
        pbrShader->setInt("irradianceMap", TextureUnit::Irradiance);
        pbrShader->setInt("prefilterMap", TextureUnit::Prefilter);
        pbrShader->setInt("brdfLUT", TextureUnit::BRDFLUT);
        pbrShader->setInt("u_AlbedoMap", TextureUnit::Albedo);
        pbrShader->setInt("u_NormalMap", TextureUnit::Normal);
        pbrShader->setInt("u_MetallicMap", TextureUnit::Metallic);
        pbrShader->setInt("u_RoughnessMap", TextureUnit::Roughness);
        pbrShader->setInt("u_AOMap", TextureUnit::AO);
        pbrShader->setInt("u_ARMMap", TextureUnit::ARM);
        pbrShader->setInt("u_EmissiveMap", TextureUnit::Emissive);
        pbrShader->setInt("u_ShadowMap", TextureUnit::Shadow);
        pbrShader->setFloat("u_ModelBrightness", 1.0f);
        pbrShader->setFloat("u_ModelAmbientIntensity", 0.35f);
        pbrShader->setVec3("u_ModelAmbientColor", glm::vec3(1.0f));
    }
}

void GameManager::LoadMapResources()
{
    m_MapResourceLoader.Load(m_MapManager->GetCurrentMap(), m_PropModels);
}

void GameManager::UnloadMapResources(int mapIndex)
{
    if (mapIndex < 0 || mapIndex >= m_MapManager->GetMapCount())
        return;

    m_MapResourceLoader.Unload(m_MapManager->GetMap(mapIndex), m_PropModels);
}

void GameManager::SetupScene()
{
    m_TerrainRenderer.Initialize();
    m_SkyboxRenderer.Initialize();

    SetupTerrain();
    SetupEcology();
}

void GameManager::SetupTerrain()
{
    m_Terrain = std::make_unique<Terrain>();

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();

    if (currentMap.Terrain == TerrainType::Heightmap)
    {
        const HeightmapConfig& hm = currentMap.Heightmap;
        if (!m_Terrain->GenerateFromHeightmap(hm.Path, hm.HeightScale,
                                               hm.GridSize, hm.Size))
        {
            std::cout << "Failed to generate terrain from heightmap, falling back to flat" << std::endl;
            m_Terrain->GenerateFlat(20.0f);
        }
    }
    else
    {
        m_Terrain->GenerateFlat(20.0f);
    }
}

void GameManager::SetupEcology()
{
    m_EcologySystem = std::make_unique<EcologySystem>();

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    if (currentMap.EcologyEnabled && currentMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
    {
        m_EcologySystem->Generate(m_Terrain.get(), currentMap.Name);
    }
}

void GameManager::ProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float currentSpeed = m_Camera.MovementSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        m_Camera.MovementSpeed *= 3.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(FORWARD, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(BACKWARD, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(LEFT, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(RIGHT, m_DeltaTime);

    if (m_FreeFlyMode)
    {
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            m_Camera.ProcessKeyboard(UP, m_DeltaTime);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            m_Camera.ProcessKeyboard(DOWN, m_DeltaTime);
    }

    m_Camera.MovementSpeed = currentSpeed;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        if (!m_WireframePressed)
        {
            ToggleWireframe();
            m_WireframePressed = true;
        }
    }
    else
    {
        m_WireframePressed = false;
    }

    static bool s_MouseLeftPressed = false;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (!s_MouseLeftPressed && m_Weapon && m_Weapon->CanShoot())
        {
            Shoot();
            s_MouseLeftPressed = true;
        }
    }
    else
    {
        s_MouseLeftPressed = false;
    }

    static bool s_Key1Pressed = false;
    static bool s_Key2Pressed = false;
    static bool s_Key3Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
        if (!s_Key1Pressed) { SwitchMap(0); s_Key1Pressed = true; }
    }
    else s_Key1Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
        if (!s_Key2Pressed) { SwitchMap(1); s_Key2Pressed = true; }
    }
    else s_Key2Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
        if (!s_Key3Pressed) { SwitchMap(2); s_Key3Pressed = true; }
    }
    else s_Key3Pressed = false;

    HandleDebugInput(window);
}

void GameManager::ToggleWireframe()
{
    m_WireframeMode = !m_WireframeMode;
    std::cout << "[RENDER] Wireframe mode: " << (m_WireframeMode ? "ON" : "OFF") << std::endl;
}

void GameManager::HandleDebugInput(GLFWwindow* window)
{
    auto consumePress = [](bool pressed, bool& latch) {
        if (pressed)
        {
            if (!latch)
            {
                latch = true;
                return true;
            }
            return false;
        }
        latch = false;
        return false;
    };

    if (consumePress(glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS, m_DebugF1Pressed))
        m_DebugOverlayVisible = !m_DebugOverlayVisible;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS, m_DebugF2Pressed))
        m_PostEffectsEnabled = !m_PostEffectsEnabled;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS, m_DebugF3Pressed))
        m_IBLEnabled = !m_IBLEnabled;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS, m_DebugF4Pressed))
        m_PCSSEnabled = !m_PCSSEnabled;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS, m_DebugF5Pressed))
        m_UnderwaterEnabled = !m_UnderwaterEnabled;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS, m_DebugF6Pressed))
        m_FreeFlyMode = !m_FreeFlyMode;

    if (consumePress(glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS, m_DebugF7Pressed))
        m_PixelateEnabled = !m_PixelateEnabled;

    if (consumePress(glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS, m_DebugTabPressed))
    {
        int next = static_cast<int>(m_DebugSelectedParameter) + 1;
        if (next >= static_cast<int>(DebugParameter::Count))
            next = 0;
        m_DebugSelectedParameter = static_cast<DebugParameter>(next);
    }

    const bool decreasePressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
                                 glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                                 glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
    const bool increasePressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
                                 glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                                 glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
    const bool fastStep = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const float stepScale = fastStep ? 5.0f : 1.0f;

    if (consumePress(decreasePressed, m_DebugDecreasePressed))
        AdjustDebugParameter(-stepScale);

    if (consumePress(increasePressed, m_DebugIncreasePressed))
        AdjustDebugParameter(stepScale);

    if (consumePress(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS, m_DebugResetPressed))
        ResetDebugAdjustments();
}

void GameManager::AdjustDebugParameter(float direction)
{
    const PostProcessConfig base = m_MapManager ? m_MapManager->GetCurrentPostProcess() : PostProcessConfig();
    auto adjustOffset = [direction](float baseValue, float& offset, float step, float minValue, float maxValue) {
        const float value = std::clamp(baseValue + offset + direction * step, minValue, maxValue);
        offset = value - baseValue;
    };

    switch (m_DebugSelectedParameter)
    {
    case DebugParameter::ShadowBias:
        adjustOffset(RenderSettings::kDefaultShadowBias, m_DebugShadowBiasOffset, 0.0002f, 0.0001f, 0.02f);
        break;
    case DebugParameter::PixelSize:
        adjustOffset(base.PixelSize, m_DebugPixelSizeOffset, 1.0f, 1.0f, 32.0f);
        break;
    case DebugParameter::Count:
        break;
    }
}

void GameManager::ResetDebugAdjustments()
{
    m_PostEffectsEnabled = true;
    m_UnderwaterEnabled = m_MapManager ? m_MapManager->GetCurrentMap().PostProcess.UnderwaterEnabled : false;
    m_IBLEnabled = true;
    m_PCSSEnabled = true;
    m_ShadowsEnabled = true;
    m_FreeFlyMode = false;
    m_PixelateEnabled = false;
    m_DebugShadowBiasOffset = 0.0f;
    m_DebugPixelSizeOffset = 0.0f;
}

PostProcessConfig GameManager::GetEffectivePostProcessConfig() const
{
    PostProcessConfig config = m_MapManager ? m_MapManager->GetCurrentPostProcess() : PostProcessConfig();

    config.PixelSize = std::clamp(config.PixelSize + m_DebugPixelSizeOffset, 1.0f, 32.0f);
    if (!m_PostEffectsEnabled)
    {
        config.PixelateEnabled = false;
        config.UnderwaterEnabled = false;
    }
    else
    {
        config.PixelateEnabled = m_PixelateEnabled;
        config.UnderwaterEnabled = m_UnderwaterEnabled;
    }

    return config;
}

float GameManager::GetEffectiveShadowBias() const
{
    return std::clamp(RenderSettings::kDefaultShadowBias + m_DebugShadowBiasOffset, 0.0001f, 0.02f);
}

DebugOverlayState GameManager::BuildDebugOverlayState(const glm::vec3& mainLightDirection) const
{
    DebugOverlayState state;
    state.Visible = m_DebugOverlayVisible;
    state.PostEffectsEnabled = m_PostEffectsEnabled;
    state.PostProcess = GetEffectivePostProcessConfig();
    state.IBLEnabled = m_IBLEnabled;
    state.PCSSEnabled = m_PCSSEnabled;
    state.PixelateEnabled = m_PostEffectsEnabled && m_PixelateEnabled;
    state.UnderwaterEnabled = m_PostEffectsEnabled && m_UnderwaterEnabled;
    state.WireframeEnabled = m_WireframeMode;
    state.FreeFlyEnabled = m_FreeFlyMode;
    state.FPS = m_CurrentFps;
    state.SelectedParameter = static_cast<int>(m_DebugSelectedParameter);
    state.MainLightDirection = mainLightDirection;
    state.SkyboxRotationDegrees = glm::degrees(m_SkyboxRotation);
    state.ShadowBias = GetEffectiveShadowBias();
    if (m_MapManager)
    {
        const LightingConfig& lighting = m_MapManager->GetCurrentLighting();
        state.MainLightIntensity = lighting.MainLight.Intensity;
        state.AmbientIntensity = lighting.AmbientIntensity;
        state.IBLDiffuseIntensity = lighting.IBLDiffuseIntensity;
        state.IBLSpecularIntensity = lighting.IBLSpecularIntensity;
        state.PointLightCount = static_cast<int>(lighting.PointLights.size());
    }
    return state;
}

void GameManager::Update(float deltaTime)
{
    m_DeltaTime = deltaTime;

    m_FpsTimer += deltaTime;
    m_FpsFrameCount++;
    if (m_FpsTimer >= 0.5f)
    {
        m_CurrentFps = static_cast<int>(std::round(m_FpsFrameCount / m_FpsTimer));
        m_FpsTimer = 0.0f;
        m_FpsFrameCount = 0;
    }

    ClampPlayerToTerrain();

    if (m_EcologySystem)
        m_EcologySystem->Update(deltaTime, m_Terrain.get());

    if (m_TargetManager && !m_FreeFlyMode)
        m_TargetManager->Update(deltaTime, m_Camera, m_Terrain.get());

    if (m_ScoreSystem)
        m_ScoreSystem->Update(deltaTime);

    if (m_ParticleSystem)
        m_ParticleSystem->Update(deltaTime);

    if (m_Weapon)
        m_Weapon->Update(deltaTime);

    if (m_HitMarkerActive)
    {
        m_HitMarkerTimer += deltaTime;
        if (m_HitMarkerTimer >= HUDSettings::kHitMarkerDuration)
        {
            m_HitMarkerActive = false;
            m_HitMarkerTimer = 0.0f;
        }
    }

    if (m_HitFeedback)
        m_HitFeedback->Update(deltaTime);

    if (m_ScreenShake)
        m_ScreenShake->Update(deltaTime);

    m_SkyboxRotation += deltaTime * RenderSettings::kSkyboxRotationSpeed;
}

void GameManager::Render()
{
    glm::vec3 mainLightDirection = GetRotatedMainLightDirection();
    if (m_ShadowsEnabled)
        RenderShadowMap(mainLightDirection);

    ShadowMapper* activeShadowMapper = m_ShadowsEnabled ? m_ShadowMapper.get() : nullptr;
    PostProcessConfig postProcessConfig = GetEffectivePostProcessConfig();

    if (m_PostProcessRenderer)
        m_PostProcessRenderer->BeginScene();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    RenderScene(mainLightDirection, activeShadowMapper);

    if (m_PostProcessRenderer)
    {
        m_PostProcessRenderer->EndScene();
        m_PostProcessRenderer->RenderToScreen(static_cast<float>(glfwGetTime()), postProcessConfig);
    }

    if (m_UIRenderer && m_ScoreSystem && m_MapManager && m_FontRenderer)
    {
        DebugOverlayState debugOverlay = BuildDebugOverlayState(mainLightDirection);
        m_HudRenderer.Render(*m_UIRenderer,
                             *m_FontRenderer,
                             *m_ScoreSystem,
                             m_HitFeedback.get(),
                             m_MapManager->GetCurrentMap().Name,
                             m_ScreenWidth,
                             m_ScreenHeight,
                             m_HitMarkerActive,
                             m_HitMarkerTimer,
                             &debugOverlay);
    }
}

glm::vec3 GameManager::GetRotatedMainLightDirection() const
{
    if (!m_MapManager)
        return glm::normalize(glm::vec3(-0.35f, -0.9f, -0.25f));

    const DirectionalLightConfig& mainLight = m_MapManager->GetCurrentLighting().MainLight;
    glm::vec3 direction = glm::normalize(mainLight.Direction);
    if (mainLight.SyncWithSkyboxRotation)
    {
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f),
                                         m_SkyboxRotation + mainLight.SkyboxRotationOffset,
                                         glm::vec3(0.0f, 1.0f, 0.0f));
        direction = glm::normalize(glm::mat3(rotation) * direction);
    }
    return direction;
}

void GameManager::RenderShadowMap(const glm::vec3& mainLightDirection)
{
    if (!m_ShadowMapper)
        return;

    m_ShadowPassRenderer.Render(*m_ShadowMapper,
                                m_MapManager->GetCurrentMap(),
                                m_Terrain.get(),
                                m_EcologySystem.get(),
                                m_TerrainRenderer,
                                m_PBRPropRenderer,
                                m_PropModels,
                                m_Camera,
                                mainLightDirection);
}

void GameManager::RenderScene(const glm::vec3& mainLightDirection, ShadowMapper* activeShadowMapper)
{
    glm::mat4 projection = glm::perspective(glm::radians(m_Camera.Zoom), (float)m_ScreenWidth / (float)m_ScreenHeight, 0.1f, 100.0f);
    glm::mat4 view = m_Camera.GetViewMatrix();

    if (m_ScreenShake && m_ScreenShake->IsActive())
        view = m_ScreenShake->GetViewMatrix(view);

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    const float shadowBias = GetEffectiveShadowBias();
    {
        PolygonModeGuard worldPolygonMode(m_WireframeMode ? GL_LINE : GL_FILL);

        if (currentMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
        {
            m_TerrainRenderer.RenderHeightmap(*m_Terrain,
                                              m_MapManager->GetCurrentFloorTextureName(),
                                              m_MapManager->GetCurrentAmbientLight(),
                                              m_Camera.Position,
                                              projection,
                                              view,
                                              m_MapManager->GetCurrentLighting(),
                                              mainLightDirection,
                                              activeShadowMapper,
                                              m_PCSSEnabled,
                                              shadowBias);
        }
        else
        {
            m_TerrainRenderer.RenderPlane(m_MapManager->GetCurrentFloorTextureName(),
                                          m_MapManager->GetCurrentAmbientLight(),
                                          m_Camera.Position,
                                          projection,
                                          view,
                                          m_MapManager->GetCurrentLighting(),
                                          mainLightDirection,
                                          activeShadowMapper,
                                          m_PCSSEnabled,
                                          shadowBias);
        }

        if (m_EcologySystem && m_EcologySystem->IsReady())
        {
            ResourceManager& rm = ResourceManager::GetInstance();
            Shader* ecologyShader = rm.GetShader("ecology_sphere");
            if (ecologyShader)
            {
                m_EcologySystem->Render(m_Camera, *ecologyShader, projection, view, m_MapManager->GetCurrentAmbientLight());
            }
        }

        m_PBRPropRenderer.Render(currentMap,
                                 m_PropModels,
                                 m_Terrain.get(),
                                 m_Camera,
                                 projection,
                                 view,
                                 mainLightDirection,
                                 activeShadowMapper,
                                 m_IBLEnabled,
                                 m_PCSSEnabled,
                                 shadowBias);

        if (!m_FreeFlyMode)
        {
            m_TargetRenderer.Render(m_TargetManager.get(),
                                    m_Camera,
                                    m_MapManager->GetCurrentAmbientLight(),
                                    projection,
                                    view);
        }
    }

    m_SkyboxRenderer.Render(currentMap, projection, m_Camera.GetViewMatrix(), m_SkyboxRotation);
    m_ParticleRenderer.Render(m_ParticleSystem.get(), projection, view);
    if (!m_FreeFlyMode)
    {
        m_WeaponViewRenderer.Render(m_Weapon.get(), m_Camera, m_ScreenWidth, m_ScreenHeight);
    }
}

void GameManager::Cleanup()
{
    if (!m_Initialized)
        return;

    m_TerrainRenderer.Cleanup();
    m_SkyboxRenderer.Cleanup();

    m_PropModels.clear();

    if (m_ParticleSystem) m_ParticleSystem->Cleanup();
    if (m_Weapon) m_Weapon->Cleanup();
    if (m_Terrain) m_Terrain->Cleanup();
    if (m_UIRenderer) m_UIRenderer->Cleanup();
    if (m_FontRenderer) m_FontRenderer->Cleanup();
    if (m_PostProcessRenderer) m_PostProcessRenderer->Cleanup();
    if (m_ShadowMapper) m_ShadowMapper->Cleanup();

    m_Terrain.reset();
    m_EcologySystem.reset();
    m_TargetManager.reset();
    m_ScoreSystem.reset();
    m_ParticleSystem.reset();
    m_Weapon.reset();
    m_MapManager.reset();
    m_UIRenderer.reset();
    m_FontRenderer.reset();
    m_HitFeedback.reset();
    m_PostProcessRenderer.reset();
    m_ShadowMapper.reset();

    ResourceManager::GetInstance().Clear();

    m_Initialized = false;
}

void GameManager::Shoot()
{
    if (!m_TargetManager || !m_ScoreSystem || !m_Weapon)
        return;

    m_Weapon->Shoot();

    Ray ray = Raycast::ScreenToWorldRay(m_Camera.Position, m_Camera.Front);

    float t = 0.0f;
    TargetSphere* hitTarget = Raycast::RaycastAgainstTargets(ray, m_TargetManager.get(), t);

    if (hitTarget)
    {
        glm::vec3 hitPos = ray.origin + ray.direction * t;
        float hitDistance = t;
        float precision = 1.0f - glm::length(hitPos - hitTarget->GetPosition()) / hitTarget->GetRadius();
        precision = glm::clamp(precision, 0.0f, 1.0f);

        hitTarget->OnHit();

        m_HitMarkerActive = true;
        m_HitMarkerTimer = 0.0f;

        HitResult result = m_ScoreSystem->OnHit(hitDistance, precision);

        if (m_ParticleSystem)
            m_ParticleSystem->EmitExplosion(hitPos, glm::vec3(1.0f, 0.75f, 0.15f), 22, 1.5f, 4.5f);

        if (m_ScreenShake)
        {
            float shakeIntensity = 0.3f + result.precision * 0.4f;
            if (result.combo > 1)
                shakeIntensity += result.combo * 0.08f;
            m_ScreenShake->Trigger(shakeIntensity, 0.20f);
        }

        if (m_HitFeedback)
        {
            glm::mat4 proj = glm::perspective(glm::radians(m_Camera.Zoom),
                                               static_cast<float>(m_ScreenWidth) / m_ScreenHeight,
                                               0.1f, 100.0f);
            glm::mat4 view = m_Camera.GetViewMatrix();
            glm::vec2 screenPos = WorldToScreen(hitPos, proj, view);

            if (screenPos.x >= 0.0f && screenPos.y >= 0.0f)
            {
                float glY = static_cast<float>(m_ScreenHeight) - screenPos.y;
                float offsetX = 80.0f;
                float offsetY = 20.0f;
                m_HitFeedback->AddHitFeedback(screenPos.x + offsetX, glY + offsetY,
                                               result.points, result.precision, result.combo);
            }
        }
    }
}

glm::vec2 GameManager::WorldToScreen(const glm::vec3& worldPos, const glm::mat4& projection, const glm::mat4& view)
{
    glm::vec4 clipPos = projection * view * glm::vec4(worldPos, 1.0f);
    if (clipPos.w <= 0.0f)
        return glm::vec2(-1.0f, -1.0f);

    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    float screenX = (ndc.x + 1.0f) * 0.5f * m_ScreenWidth;
    float screenY = (1.0f - ndc.y) * 0.5f * m_ScreenHeight;
    return glm::vec2(screenX, screenY);
}

Camera* GameManager::GetCamera() { return &m_Camera; }

float GameManager::GetDeltaTime() const { return m_DeltaTime; }

void GameManager::SetMouseFirst(bool first) { m_FirstMouse = first; }

void GameManager::ProcessMouseMovement(float xpos, float ypos)
{
    if (m_FirstMouse)
    {
        m_LastX = xpos;
        m_LastY = ypos;
        m_FirstMouse = false;
    }

    float xoffset = xpos - m_LastX;
    float yoffset = m_LastY - ypos;

    m_LastX = xpos;
    m_LastY = ypos;

    m_Camera.ProcessMouseMovement(xoffset, yoffset);
}

void GameManager::ProcessMouseScroll(float yoffset)
{
    m_Camera.ProcessMouseScroll(yoffset);
}

void GameManager::ProcessFramebufferSize(int width, int height)
{
    glViewport(0, 0, width, height);
    m_ScreenWidth = width;
    m_ScreenHeight = height;
    if (m_UIRenderer)
        m_UIRenderer->SetScreenSize(width, height);
    if (m_FontRenderer)
        m_FontRenderer->SetScreenSize(width, height);
    if (m_HitFeedback)
        m_HitFeedback->SetScreenSize(width, height);
    if (m_PostProcessRenderer)
        m_PostProcessRenderer->Resize(width, height);
}

void GameManager::SwitchMap(int mapIndex)
{
    if (!m_MapManager || mapIndex == m_MapManager->GetCurrentMapIndex())
        return;

    int oldMapIndex = m_MapManager->GetCurrentMapIndex();
    UnloadMapResources(oldMapIndex);

    m_MapManager->SwitchToMap(mapIndex);

    const MapConfig& newMap = m_MapManager->GetCurrentMap();
    m_UnderwaterEnabled = newMap.PostProcess.UnderwaterEnabled;

    LoadMapResources();
    SetupTerrain();
    SetupEcology();

    glm::vec3 startPos = newMap.PlayerStartPos;
    if (newMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
    {
        float terrainHeight = m_Terrain->GetHeightAt(startPos.x, startPos.z);
        startPos.y = terrainHeight + 1.5f;
    }
    m_Camera.Position = startPos;

    if (m_TargetManager)
    {
        for (int i = 0; i < TargetManager::MAX_TARGETS; ++i)
        {
            TargetSphere* target = m_TargetManager->GetTarget(i);
            if (target && !target->IsFullyDead())
                target->Deactivate();
        }
        m_TargetManager->SetTargetWall(
            newMap.TargetArea.WallDistance,
            newMap.TargetArea.WallWidth,
            newMap.TargetArea.WallHeight,
            newMap.TargetArea.WallCenterY,
            newMap.TargetArea.Lifetime);
    }

    ResetGameState();

    std::cout << "[MAP SWITCH] " << newMap.Name << " | Spawn: ("
              << newMap.PlayerStartPos.x << ", " << newMap.PlayerStartPos.y << ", "
              << newMap.PlayerStartPos.z << ")" << std::endl;
}

void GameManager::ClampPlayerToTerrain()
{
    if (m_FreeFlyMode)
        return;

    if (!m_Terrain || !m_Terrain->IsInitialized())
    {
        if (m_Camera.Position.y < 1.0f)
            m_Camera.Position.y = 1.0f;
        return;
    }

    const float playerEyeHeight = 1.5f;
    float minX = m_Terrain->GetMinX();
    float maxX = m_Terrain->GetMaxX();
    float minZ = m_Terrain->GetMinZ();
    float maxZ = m_Terrain->GetMaxZ();

    float margin = 0.5f;
    m_Camera.Position.x = std::max(minX + margin, std::min(maxX - margin, m_Camera.Position.x));
    m_Camera.Position.z = std::max(minZ + margin, std::min(maxZ - margin, m_Camera.Position.z));

    float terrainHeight = m_Terrain->GetHeightAt(m_Camera.Position.x, m_Camera.Position.z);
    m_Camera.Position.y = terrainHeight + playerEyeHeight;
}

void GameManager::ResetGameState()
{
    if (m_ScoreSystem)
        m_ScoreSystem->Reset();
    if (m_HitFeedback)
        m_HitFeedback->Clear();
}
