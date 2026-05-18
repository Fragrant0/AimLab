#include "game_manager.h"
#include "ui_utils.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace UILayout
{
    constexpr float SCORE_MARGIN = 30.0f;
    constexpr float SCORE_COLUMN_GAP = -50.0f;
    constexpr float SCORE_ROW_GAP = 5.0f;
    constexpr float HINT_MARGIN = 30.0f;
    constexpr float HINT_COLUMN_GAP = 15.0f;
    constexpr float HINT_ROW_GAP = 5.0f;
    constexpr float HINT_TOP_OFFSET = 5.0f;
    constexpr int SCORE_RESERVED_DIGITS = 2;

    constexpr float SCORE_FONT_SCALE = 2.15f;
    constexpr float COMBO_FONT_SCALE = 1.75f;
    constexpr float HINT_FONT_SCALE = 1.1f;
    constexpr float BEST_FONT_SCALE = 1.2f;

    constexpr float COMBO_BAR_WIDTH = 100.0f;
    constexpr float COMBO_BAR_HEIGHT = 3.0f;

    constexpr float HIT_MARKER_DURATION = 0.2f;
    constexpr float HIT_MARKER_LEN = 10.0f;
    constexpr float HIT_MARKER_GAP = 5.0f;
    constexpr float HIT_MARKER_THICKNESS = 2.0f;
    constexpr float CROSSHAIR_SIZE = 12.0f;
    constexpr float CROSSHAIR_GAP = 4.0f;
    constexpr float CROSSHAIR_THICKNESS = 2.0f;
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
}

GameManager::GameManager()
    : m_Window(nullptr),
      m_Camera(glm::vec3(0.0f, 0.0f, 3.0f)),
      m_DeltaTime(0.0f),
      m_LastX(SCR_WIDTH / 2.0f),
      m_LastY(SCR_HEIGHT / 2.0f),
      m_FirstMouse(true),
      m_WireframeMode(false),
      m_PlaneVAO(0),
      m_PlaneVBO(0),
      m_SkyboxVAO(0),
      m_SkyboxVBO(0),
      m_SkyboxRotation(0.0f),
      m_Initialized(false),
      m_WireframePressed(false),
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
        m_ShadowMapper->Initialize(2048);
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
    rm.LoadShader("particle", "shaders/particle.vs", "shaders/particle.fs");
    rm.LoadShader("weapon", "shaders/weapon.vs", "shaders/weapon.fs");
    rm.LoadShader("muzzle_flash", "shaders/muzzle_flash.vs", "shaders/muzzle_flash.fs");

    for (int i = 0; i < m_MapManager->GetMapCount(); ++i)
    {
        const MapConfig& map = m_MapManager->GetMap(i);
        rm.LoadTexture(map.FloorTextureName, map.FloorTexturePath.c_str());
    }

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
        pbrShader->setInt("irradianceMap", 6);
        pbrShader->setInt("prefilterMap", 7);
        pbrShader->setInt("brdfLUT", 8);
        pbrShader->setInt("u_AlbedoMap", 0);
        pbrShader->setInt("u_NormalMap", 1);
        pbrShader->setInt("u_MetallicMap", 2);
        pbrShader->setInt("u_RoughnessMap", 3);
        pbrShader->setInt("u_AOMap", 4);
        pbrShader->setInt("u_ARMMap", 5);
        pbrShader->setInt("u_EmissiveMap", 9);
        pbrShader->setInt("u_ShadowMap", 10);
        pbrShader->setFloat("u_ModelBrightness", 1.0f);
        pbrShader->setFloat("u_ModelAmbientIntensity", 0.35f);
        pbrShader->setVec3("u_ModelAmbientColor", glm::vec3(1.0f));
    }
}

void GameManager::LoadMapResources()
{
    ResourceManager& rm = ResourceManager::GetInstance();
    const MapConfig& currentMap = m_MapManager->GetCurrentMap();

    if (rm.GetTexture(currentMap.FloorTextureName) == 0)
        rm.LoadTexture(currentMap.FloorTextureName, currentMap.FloorTexturePath.c_str());

    if (currentMap.Skybox.Type == SkyboxType::HDR && !currentMap.Skybox.HDRPath.empty())
    {
        rm.LoadHDRSkybox("skybox", currentMap.Skybox.HDRPath.c_str());
    }
    else
    {
        rm.LoadCubemap("skybox", currentMap.Skybox.Faces);
    }

    for (const auto& prop : currentMap.Props)
    {
        if (!prop.ModelPath.empty() && m_PropModels.find(prop.ModelPath) == m_PropModels.end())
        {
            m_PropModels[prop.ModelPath] = std::make_unique<Model>(prop.ModelPath);
        }
    }
}

void GameManager::UnloadMapResources(int mapIndex)
{
    if (mapIndex < 0 || mapIndex >= m_MapManager->GetMapCount())
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    const MapConfig& oldMap = m_MapManager->GetMap(mapIndex);

    rm.UnloadTexture(oldMap.FloorTextureName);

    if (oldMap.Skybox.Type == SkyboxType::HDR)
    {
        rm.UnloadHDRSkybox("skybox");
        rm.UnloadHDRTexture("skybox_hdr");
    }
    else
    {
        rm.UnloadCubemap("skybox");
    }

    m_PropModels.clear();
}

void GameManager::SetupScene()
{
    float planeVertices[] = {
         5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 0.0f,
        -5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 2.0f,

         5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 2.0f,
         5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   2.0f, 2.0f
    };

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_PlaneVAO);
    glGenBuffers(1, &m_PlaneVBO);
    glBindVertexArray(m_PlaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_PlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_SkyboxVAO);
    glGenBuffers(1, &m_SkyboxVBO);
    glBindVertexArray(m_SkyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_SkyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

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
        m_EcologySystem->Generate(m_Terrain.get());
    }
}

void GameManager::ProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(FORWARD, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(BACKWARD, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(LEFT, m_DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(RIGHT, m_DeltaTime);

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
}

void GameManager::ToggleWireframe()
{
    m_WireframeMode = !m_WireframeMode;
    std::cout << "[RENDER] Wireframe mode: " << (m_WireframeMode ? "ON" : "OFF") << std::endl;
}

void GameManager::Update(float deltaTime)
{
    m_DeltaTime = deltaTime;

    ClampPlayerToTerrain();

    if (m_EcologySystem)
        m_EcologySystem->Update(deltaTime, m_Terrain.get());

    if (m_TargetManager)
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
        if (m_HitMarkerTimer >= UILayout::HIT_MARKER_DURATION)
        {
            m_HitMarkerActive = false;
            m_HitMarkerTimer = 0.0f;
        }
    }

    if (m_HitFeedback)
        m_HitFeedback->Update(deltaTime);

    if (m_ScreenShake)
        m_ScreenShake->Update(deltaTime);

    m_SkyboxRotation += deltaTime * 0.008f;
}

void GameManager::Render()
{
    glm::vec3 mainLightDirection = GetRotatedMainLightDirection();
    RenderShadowMap(mainLightDirection);

    if (m_PostProcessRenderer)
        m_PostProcessRenderer->BeginScene();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glPolygonMode(GL_FRONT_AND_BACK, m_WireframeMode ? GL_LINE : GL_FILL);
    RenderScene();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (m_PostProcessRenderer)
    {
        m_PostProcessRenderer->EndScene();
        m_PostProcessRenderer->RenderToScreen(static_cast<float>(glfwGetTime()), m_MapManager->GetCurrentPostProcess());
    }

    RenderUI();
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

glm::mat4 GameManager::BuildPropModelMatrix(const PropConfig& prop, const Model& model) const
{
    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    glm::vec3 propPosition = prop.Position;
    if (currentMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
        propPosition.y = m_Terrain->GetHeightAt(propPosition.x, propPosition.z) + prop.Position.y;

    glm::vec3 finalScale = prop.Scale;
    const glm::vec3 boundsSize = model.GetBoundsSize();
    if (prop.TargetHeight > 0.0f && boundsSize.y > 0.0001f)
    {
        const float normalizeScale = prop.TargetHeight / boundsSize.y;
        finalScale *= normalizeScale;
    }

    propPosition.y -= model.GetBoundsMin().y * finalScale.y;

    glm::mat4 matrix = glm::mat4(1.0f);
    matrix = glm::translate(matrix, propPosition);
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(prop.Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, finalScale);
    return matrix;
}

void GameManager::ApplyPBRLighting(Shader& shader, const glm::vec3& mainLightDirection)
{
    const LightingConfig& lighting = m_MapManager->GetCurrentLighting();
    shader.setVec3("u_MainLight.direction", mainLightDirection);
    shader.setVec3("u_MainLight.color", lighting.MainLight.Color);
    shader.setFloat("u_MainLight.intensity", lighting.MainLight.Intensity);
    shader.setFloat("u_IBLDiffuseIntensity", lighting.IBLDiffuseIntensity);
    shader.setFloat("u_IBLSpecularIntensity", lighting.IBLSpecularIntensity);

    int count = std::min(static_cast<int>(lighting.PointLights.size()), 8);
    shader.setInt("u_PointLightCount", count);
    for (int i = 0; i < count; ++i)
    {
        const PointLightConfig& light = lighting.PointLights[i];
        std::string index = std::to_string(i);
        shader.setVec3("u_PointLightPositions[" + index + "]", light.Position);
        shader.setVec3("u_PointLightColors[" + index + "]", light.Color);
        shader.setFloat("u_PointLightIntensities[" + index + "]", light.Intensity);
        shader.setFloat("u_PointLightRanges[" + index + "]", light.Range);
    }

    shader.setBool("u_ShadowsEnabled", m_ShadowMapper != nullptr);
    shader.setFloat("u_LightSize", lighting.MainLight.AngularSize);
    shader.setFloat("u_ShadowBias", 0.0018f);
    if (m_ShadowMapper)
    {
        shader.setMat4("u_LightSpaceMatrix", m_ShadowMapper->GetLightSpaceMatrix());
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, m_ShadowMapper->GetDepthMap());
        shader.setInt("u_ShadowMap", 10);
    }
}

void GameManager::ApplyTerrainLighting(Shader& shader, const glm::vec3& mainLightDirection)
{
    const LightingConfig& lighting = m_MapManager->GetCurrentLighting();
    shader.setVec3("u_MainLightDirection", mainLightDirection);
    shader.setVec3("u_MainLightColor", lighting.MainLight.Color);
    shader.setFloat("u_MainLightIntensity", lighting.MainLight.Intensity);
    shader.setBool("u_ShadowsEnabled", m_ShadowMapper != nullptr);
    shader.setFloat("u_LightSize", lighting.MainLight.AngularSize);
    shader.setFloat("u_ShadowBias", 0.0025f);
    if (m_ShadowMapper)
    {
        shader.setMat4("u_LightSpaceMatrix", m_ShadowMapper->GetLightSpaceMatrix());
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, m_ShadowMapper->GetDepthMap());
        shader.setInt("u_ShadowMap", 10);
    }
}

void GameManager::RenderShadowMap(const glm::vec3& mainLightDirection)
{
    if (!m_ShadowMapper)
        return;

    m_ShadowMapper->BeginDepthPass(mainLightDirection, m_Camera);
    Shader* depthShader = m_ShadowMapper->GetDepthShader();
    if (depthShader)
    {
        depthShader->use();
        depthShader->setMat4("lightSpaceMatrix", m_ShadowMapper->GetLightSpaceMatrix());
        RenderShadowCasters(*depthShader);
    }
    m_ShadowMapper->EndDepthPass();
}

void GameManager::RenderShadowCasters(Shader& depthShader)
{
    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    if (currentMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
        RenderShadowTerrain(depthShader);
    else
        RenderShadowPlane(depthShader);

    if (m_EcologySystem && m_EcologySystem->IsReady())
        m_EcologySystem->RenderDepth(depthShader);

    RenderShadowProps(depthShader);
}

void GameManager::RenderShadowTerrain(Shader& depthShader)
{
    if (!m_Terrain || !m_Terrain->IsInitialized())
        return;
    depthShader.setMat4("model", glm::mat4(1.0f));
    m_Terrain->Render();
}

void GameManager::RenderShadowPlane(Shader& depthShader)
{
    depthShader.setMat4("model", glm::mat4(1.0f));
    glBindVertexArray(m_PlaneVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void GameManager::RenderShadowProps(Shader& depthShader)
{
    if (m_PropModels.empty())
        return;

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    for (const auto& prop : currentMap.Props)
    {
        auto it = m_PropModels.find(prop.ModelPath);
        if (it == m_PropModels.end() || !it->second)
            continue;

        glm::mat4 model = BuildPropModelMatrix(prop, *it->second);
        depthShader.setMat4("model", model);
        it->second->DrawGeometry();
    }
}

void GameManager::RenderScene()
{
    glm::mat4 projection = glm::perspective(glm::radians(m_Camera.Zoom), (float)m_ScreenWidth / (float)m_ScreenHeight, 0.1f, 100.0f);
    glm::mat4 view = m_Camera.GetViewMatrix();

    if (m_ScreenShake && m_ScreenShake->IsActive())
        view = m_ScreenShake->GetViewMatrix(view);

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    if (currentMap.Terrain == TerrainType::Heightmap && m_Terrain && m_Terrain->IsInitialized())
        RenderTerrain(projection, view);
    else
        RenderPlane(projection, view);

    if (m_EcologySystem && m_EcologySystem->IsReady())
    {
        ResourceManager& rm = ResourceManager::GetInstance();
        Shader* ecologyShader = rm.GetShader("sphere");
        if (ecologyShader)
        {
            m_EcologySystem->Render(m_Camera, *ecologyShader, projection, view, m_MapManager->GetCurrentAmbientLight());
        }
    }

    RenderProps(projection, view);
    RenderSkybox(projection, view);
    RenderTargets(projection, view);
    RenderParticles(projection, view);
    RenderWeapon(projection, view);
}

void GameManager::RenderTerrain(glm::mat4 projection, glm::mat4 view)
{
    if (!m_Terrain || !m_Terrain->IsInitialized())
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* terrainShader = rm.GetShader("terrain");
    if (!terrainShader) return;

    terrainShader->use();
    terrainShader->setMat4("projection", projection);
    terrainShader->setMat4("view", view);
    terrainShader->setVec3("ambientLight", m_MapManager->GetCurrentAmbientLight());
    terrainShader->setVec3("viewPos", m_Camera.Position);
    terrainShader->setMat4("model", glm::mat4(1.0f));
    ApplyTerrainLighting(*terrainShader, GetRotatedMainLightDirection());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rm.GetTexture(m_MapManager->GetCurrentFloorTextureName()));

    m_Terrain->Render();
}

void GameManager::RenderPlane(glm::mat4 projection, glm::mat4 view)
{
    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* planeShader = rm.GetShader("plane");
    if (!planeShader) return;

    planeShader->use();
    planeShader->setMat4("projection", projection);
    planeShader->setMat4("view", view);
    planeShader->setVec3("ambientLight", m_MapManager->GetCurrentAmbientLight());
    planeShader->setVec3("viewPos", m_Camera.Position);
    ApplyTerrainLighting(*planeShader, GetRotatedMainLightDirection());

    glBindVertexArray(m_PlaneVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rm.GetTexture(m_MapManager->GetCurrentFloorTextureName()));
    planeShader->setMat4("model", glm::mat4(1.0f));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void GameManager::RenderProps(glm::mat4 projection, glm::mat4 view)
{
    if (m_PropModels.empty())
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* pbrShader = rm.GetShader("pbr_model");
    if (!pbrShader) return;

    pbrShader->use();
    pbrShader->setVec3("cameraPos", m_Camera.Position);
    pbrShader->setMat4("projection", projection);
    pbrShader->setMat4("view", view);
    ApplyPBRLighting(*pbrShader, GetRotatedMainLightDirection());

    // IBL texture binding
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetIrradianceMap("skybox"));
    pbrShader->setInt("irradianceMap", 6);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetPrefilterMap("skybox"));
    pbrShader->setInt("prefilterMap", 7);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, rm.GetBRDFLUT());
    pbrShader->setInt("brdfLUT", 8);

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();

    for (const auto& prop : currentMap.Props)
    {
        auto it = m_PropModels.find(prop.ModelPath);
        if (it == m_PropModels.end() || !it->second)
            continue;

        glm::mat4 model = BuildPropModelMatrix(prop, *it->second);

        pbrShader->setMat4("model", model);

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        pbrShader->setMat3("normalMatrix", normalMatrix);

        pbrShader->setVec3("u_AlbedoColor", glm::vec3(0.8f, 0.8f, 0.8f));
        pbrShader->setFloat("u_Metallic", 0.0f);
        pbrShader->setFloat("u_Roughness", 0.65f);
        pbrShader->setFloat("u_AO", 1.0f);
        pbrShader->setFloat("u_ModelBrightness", prop.PBRBrightness);
        pbrShader->setFloat("u_ModelAmbientIntensity", prop.PBRAmbientIntensity);
        pbrShader->setVec3("u_ModelAmbientColor", prop.PBRAmbientColor);

        it->second->Draw(*pbrShader);
    }
}

void GameManager::RenderSkybox(glm::mat4 projection, glm::mat4 view)
{
    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* skyboxShader = rm.GetShader("skybox");
    if (!skyboxShader) return;

    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glDepthFunc(GL_LEQUAL);

    skyboxShader->use();
    glm::mat4 skyboxView = glm::mat4(glm::mat3(m_Camera.GetViewMatrix()));
    skyboxShader->setMat4("view", skyboxView);
    skyboxShader->setMat4("projection", projection);
    skyboxShader->setFloat("rotation", m_SkyboxRotation);

    const MapConfig& currentMap = m_MapManager->GetCurrentMap();
    bool isHDR = (currentMap.Skybox.Type == SkyboxType::HDR);
    skyboxShader->setBool("isHDR", isHDR);
    skyboxShader->setFloat("exposure", isHDR ? 1.25f : 1.0f);

    glBindVertexArray(m_SkyboxVAO);
    glActiveTexture(GL_TEXTURE0);

    if (isHDR)
        glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetHDRSkybox("skybox"));
    else
        glBindTexture(GL_TEXTURE_CUBE_MAP, rm.GetCubemap("skybox"));

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(prevDepthFunc);
}

void GameManager::RenderTargets(glm::mat4 projection, glm::mat4 view)
{
    if (!m_TargetManager)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* sphereShader = rm.GetShader("sphere");
    if (!sphereShader) return;

    sphereShader->use();
    sphereShader->setMat4("projection", projection);
    sphereShader->setMat4("view", view);
    sphereShader->setVec3("viewPos", m_Camera.Position);
    sphereShader->setVec3("ambientLight", m_MapManager->GetCurrentAmbientLight());
    sphereShader->setFloat("alpha", 1.0f);
    sphereShader->setVec3("lightDir", glm::vec3(0.5f, 1.0f, 0.3f));
    sphereShader->setFloat("ambientStrength", 0.4f);
    sphereShader->setFloat("diffuseStrength", 0.6f);
    sphereShader->setFloat("specularStrength", 0.3f);
    sphereShader->setFloat("emissionStrength", 0.3f);
    sphereShader->setFloat("fresnelStrength", 0.5f);
    sphereShader->setFloat("shininess", 32.0f);

    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_TargetManager->Render(*sphereShader, projection, view);

    if (!blendEnabled)
        glDisable(GL_BLEND);
}

void GameManager::RenderParticles(glm::mat4 projection, glm::mat4 view)
{
    if (!m_ParticleSystem)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* particleShader = rm.GetShader("particle");
    if (!particleShader) return;

    m_ParticleSystem->Render(*particleShader, projection, view);
}

void GameManager::RenderWeapon(glm::mat4 projection, glm::mat4 view)
{
    if (!m_Weapon)
        return;

    ResourceManager& rm = ResourceManager::GetInstance();
    Shader* weaponShader = rm.GetShader("weapon");
    Shader* flashShader = rm.GetShader("muzzle_flash");
    if (!weaponShader) return;

    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glClear(GL_DEPTH_BUFFER_BIT);

    float currentTime = static_cast<float>(glfwGetTime());
    m_Weapon->Render(*weaponShader, flashShader, m_Camera, m_ScreenWidth, m_ScreenHeight, currentTime);

    glDepthFunc(prevDepthFunc);
}

float GameManager::GetUIScale() const
{
    return UIUtils::GetUIScale(m_ScreenWidth, m_ScreenHeight);
}

void GameManager::RenderCenterOverlay(float uiScale)
{
    if (!m_UIRenderer)
        return;

    float w = static_cast<float>(m_ScreenWidth);
    float h = static_cast<float>(m_ScreenHeight);
    float centerX = UIUtils::SnapToPixel(w * 0.5f);
    float centerY = UIUtils::SnapToPixel(h * 0.5f);

    float crosshairSize = std::max(6.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_SIZE, uiScale));
    float crosshairGap = std::max(2.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_GAP, uiScale));
    float crosshairThickness = std::max(1.0f, UIUtils::ScaleToScreen(UILayout::CROSSHAIR_THICKNESS, uiScale));

    m_UIRenderer->Begin();
    m_UIRenderer->DrawCrosshair(centerX, centerY, crosshairSize, crosshairGap,
                                crosshairThickness, kCrosshairColor);

    if (m_HitMarkerActive)
    {
        float alpha = 1.0f - m_HitMarkerTimer / UILayout::HIT_MARKER_DURATION;
        float markerLength = std::max(6.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_LEN, uiScale));
        float markerGap = std::max(3.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_GAP, uiScale));
        float markerWidth = std::max(1.0f, UIUtils::ScaleToScreen(UILayout::HIT_MARKER_THICKNESS, uiScale));

        auto drawMarker = [&](float dx, float dy) {
            m_UIRenderer->DrawLine(centerX - dx * markerGap - dx * markerLength, centerY - dy * markerGap - dy * markerLength,
                                   centerX - dx * markerGap, centerY - dy * markerGap, kHitMarkerColor, markerWidth, alpha);
        };

        drawMarker(-1.0f, -1.0f);
        drawMarker(1.0f, -1.0f);
        drawMarker(-1.0f, 1.0f);
        drawMarker(1.0f, 1.0f);
    }

    m_UIRenderer->End();
}

void GameManager::RenderScoreboard(float uiScale)
{
    if (!m_FontRenderer || !m_ScoreSystem)
        return;

    const float w = static_cast<float>(m_ScreenWidth);
    const float h = static_cast<float>(m_ScreenHeight);
    const float margin = UIUtils::ScaleToScreen(UILayout::SCORE_MARGIN, uiScale);
    const float columnGap = UIUtils::ScaleToScreen(UILayout::SCORE_COLUMN_GAP, uiScale);
    const float rowGap = UIUtils::ScaleToScreen(UILayout::SCORE_ROW_GAP, uiScale);

    std::vector<ScoreboardRow> rows;
    rows.reserve(2);

    const int bestScore = m_ScoreSystem->GetBestScore();
    rows.push_back({"BEST", std::to_string(bestScore), UILayout::BEST_FONT_SCALE * uiScale,
                    kBestTextColor, kBestTextColor});

    rows.push_back({"SCORE", std::to_string(m_ScoreSystem->GetScore()), UILayout::SCORE_FONT_SCALE * uiScale,
                    kScoreTextColor, kScoreTextColor});

    float comboScale = UILayout::COMBO_FONT_SCALE * uiScale;
    float maxLabelWidth = 0.0f;
    float maxValueWidth = 0.0f;
    const std::string reservedValueText(UILayout::SCORE_RESERVED_DIGITS, '0');
    for (const auto& row : rows)
    {
        maxLabelWidth = std::max(maxLabelWidth, m_FontRenderer->GetTextWidth(row.scale, row.label));
        maxValueWidth = std::max(maxValueWidth, m_FontRenderer->GetTextWidth(row.scale, reservedValueText));
    }

    std::string comboText;
    float comboTextWidth = 0.0f;
    float comboBarWidth = UIUtils::ScaleToScreen(UILayout::COMBO_BAR_WIDTH, uiScale);
    if (m_ScoreSystem->GetCombo() > 0)
    {
        comboText = "X" + std::to_string(m_ScoreSystem->GetCombo());
        comboTextWidth = m_FontRenderer->GetTextWidth(comboScale, comboText);
    }

    float panelWidth = std::max(maxLabelWidth + columnGap + maxValueWidth,
                                std::max(comboTextWidth, comboBarWidth));
    float availableWidth = std::max(1.0f, w - margin * 2.0f);
    if (panelWidth > availableWidth)
    {
        float shrink = availableWidth / panelWidth;
        for (auto& row : rows)
            row.scale *= shrink;
        comboScale *= shrink;
        comboBarWidth *= shrink;

        maxLabelWidth = 0.0f;
        maxValueWidth = 0.0f;
        for (const auto& row : rows)
        {
            maxLabelWidth = std::max(maxLabelWidth, m_FontRenderer->GetTextWidth(row.scale, row.label));
            maxValueWidth = std::max(maxValueWidth, m_FontRenderer->GetTextWidth(row.scale, reservedValueText));
        }

        if (!comboText.empty())
            comboTextWidth = m_FontRenderer->GetTextWidth(comboScale, comboText);
    }

    const float valueRightX = UIUtils::SnapToPixel(w - margin);
    const float labelRightX = UIUtils::SnapToPixel(valueRightX - maxValueWidth - columnGap);
    const float labelLeftX = UIUtils::SnapToPixel(labelRightX - maxLabelWidth);
    const float valueLeftX = UIUtils::SnapToPixel(valueRightX - maxValueWidth);
    float currentTop = UIUtils::SnapToPixel(h - margin);

    for (const auto& row : rows)
    {
        float lineHeight = UIUtils::SnapToPixel(m_FontRenderer->GetLineHeight(row.scale));
        float textY = UIUtils::SnapToPixel(currentTop - lineHeight);

        m_FontRenderer->DrawText(labelLeftX, textY,
                                 row.scale, row.label, row.labelColor);
        m_FontRenderer->DrawText(valueLeftX, textY,
                                 row.scale, row.value, row.valueColor);

        currentTop = UIUtils::SnapToPixel(textY - rowGap);
    }

    if (comboText.empty())
        return;

    float comboLineHeight = UIUtils::SnapToPixel(m_FontRenderer->GetLineHeight(comboScale));
    float comboY = UIUtils::SnapToPixel(currentTop - comboLineHeight);
    float comboX = valueLeftX;

    m_FontRenderer->DrawText(comboX, comboY, comboScale, comboText, kComboTextColor);

    float comboBarHeight = std::max(2.0f, UIUtils::ScaleToScreen(UILayout::COMBO_BAR_HEIGHT, uiScale));
    float comboVisualWidth = comboBarWidth;
    float valueColumnCenterX = valueLeftX + maxValueWidth * 0.5f;
    float comboBarX = UIUtils::SnapToPixel(valueColumnCenterX - comboVisualWidth * 0.5f);
    comboBarX = std::max(margin, std::min(comboBarX, w - margin - comboVisualWidth));
    float comboBarY = UIUtils::SnapToPixel(comboY - comboBarHeight - std::max(2.0f, rowGap * 0.5f));
    float comboProgress = 1.0f - m_ScoreSystem->GetComboTimer() / ScoreSystem::COMBO_TIMEOUT;
    comboProgress = glm::clamp(comboProgress, 0.0f, 1.0f);

    m_UIRenderer->Begin();
    m_UIRenderer->DrawRect(comboBarX, comboBarY, comboVisualWidth, comboBarHeight, kComboBarBackgroundColor);
    m_UIRenderer->DrawRect(comboBarX, comboBarY, comboVisualWidth * comboProgress, comboBarHeight,
                           kComboBarFillColor);
    m_UIRenderer->End();
}

void GameManager::RenderHintPanel(float uiScale)
{
    if (!m_FontRenderer)
        return;

    std::string mapName = m_MapManager ? m_MapManager->GetCurrentMap().Name : "UNKNOWN";
    std::string mapKey = "MAP";
    const HintRow kHintRows[] = {
        {mapKey.c_str(), mapName.c_str(), glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(1.0f, 0.82f, 0.24f)},
        {"WASD", "MOVE", glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(1.0f, 0.82f, 0.24f)},
        {"Space", "WIREFRAME", glm::vec3(0.96f, 0.97f, 0.99f), glm::vec3(0.72f, 0.84f, 1.0f)}
    };

    const float h = static_cast<float>(m_ScreenHeight);
    const float margin = UIUtils::ScaleToScreen(UILayout::HINT_MARGIN, uiScale);
    const float topOffset = UIUtils::ScaleToScreen(UILayout::HINT_TOP_OFFSET, uiScale);
    const float columnGap = UIUtils::ScaleToScreen(UILayout::HINT_COLUMN_GAP, uiScale);
    const float rowGap = UIUtils::ScaleToScreen(UILayout::HINT_ROW_GAP, uiScale);
    const float textScale = UILayout::HINT_FONT_SCALE * uiScale;

    float maxKeyWidth = 0.0f;
    for (const auto& row : kHintRows)
        maxKeyWidth = std::max(maxKeyWidth, m_FontRenderer->GetTextWidth(textScale, row.key));

    const float keyX = margin;
    const float actionX = UIUtils::SnapToPixel(keyX + maxKeyWidth + columnGap);
    float currentTop = UIUtils::SnapToPixel(h - margin - topOffset);

    for (const auto& row : kHintRows)
    {
        float lineHeight = UIUtils::SnapToPixel(m_FontRenderer->GetLineHeight(textScale));
        float textY = UIUtils::SnapToPixel(currentTop - lineHeight);

        m_FontRenderer->DrawText(keyX, textY, textScale, row.key, row.keyColor);
        m_FontRenderer->DrawText(actionX, textY, textScale, row.action, row.actionColor);

        currentTop = UIUtils::SnapToPixel(textY - rowGap);
    }
}

void GameManager::RenderUI()
{
    if (!m_UIRenderer || !m_ScoreSystem || !m_MapManager || !m_FontRenderer)
        return;

    float uiScale = GetUIScale();

    RenderCenterOverlay(uiScale);
    RenderScoreboard(uiScale);
    RenderHintPanel(uiScale);

    if (m_HitFeedback)
        m_HitFeedback->Render(*m_FontRenderer);

    m_FontRenderer->Flush();
}

void GameManager::Cleanup()
{
    if (!m_Initialized)
        return;

    if (m_PlaneVAO) { glDeleteVertexArrays(1, &m_PlaneVAO); m_PlaneVAO = 0; }
    if (m_SkyboxVAO) { glDeleteVertexArrays(1, &m_SkyboxVAO); m_SkyboxVAO = 0; }
    if (m_PlaneVBO) { glDeleteBuffers(1, &m_PlaneVBO); m_PlaneVBO = 0; }
    if (m_SkyboxVBO) { glDeleteBuffers(1, &m_SkyboxVBO); m_SkyboxVBO = 0; }

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
            m_ParticleSystem->EmitExplosion(hitPos, glm::vec3(1.0f, 0.8f, 0.2f), 12, 1.0f, 3.0f);

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

