#include "weapon.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <memory>

Weapon::Weapon()
    : m_MuzzleVAO(0),
      m_MuzzleVBO(0),
      m_MuzzleEBO(0),
      m_MuzzleIndexCount(0),
      m_MuzzleBuffersInitialized(false),
      m_RecoilOffset(0.0f),
      m_RecoilRotation(0.0f),
      m_RecoilRecoverySpeed(5.0f),
      m_MuzzleFlashTimer(0.0f),
      m_ShootCooldown(0.2f),
      m_ShootCooldownTimer(0.0f),
      m_RandomEngine(std::random_device{}()),
      m_SizeDist(0.0f, 1.0f)
{
}

Weapon::~Weapon()
{
    Cleanup();
}

void Weapon::Initialize()
{
    m_GunModel = std::make_unique<Model>("resources/objects/gun/gun.obj");

    GenerateMuzzleFlashGeometry();
    CreateMuzzleFlashBuffers();
}

void Weapon::Cleanup()
{
    m_GunModel.reset();

    if (m_MuzzleVAO != 0) {
        glDeleteVertexArrays(1, &m_MuzzleVAO);
        m_MuzzleVAO = 0;
    }
    if (m_MuzzleVBO != 0) {
        glDeleteBuffers(1, &m_MuzzleVBO);
        m_MuzzleVBO = 0;
    }
    if (m_MuzzleEBO != 0) {
        glDeleteBuffers(1, &m_MuzzleEBO);
        m_MuzzleEBO = 0;
    }

    m_MuzzleBuffersInitialized = false;
}

void Weapon::GenerateMuzzleFlashGeometry()
{
    m_MuzzleVertices.clear();
    m_MuzzleIndices.clear();

    float size = m_FlashParams.Size;
    unsigned int baseIndex = static_cast<unsigned int>(m_MuzzleVertices.size());

    m_MuzzleVertices.push_back({ glm::vec3(-size, -size, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
    m_MuzzleVertices.push_back({ glm::vec3( size, -size, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
    m_MuzzleVertices.push_back({ glm::vec3( size,  size, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
    m_MuzzleVertices.push_back({ glm::vec3(-size,  size, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });

    m_MuzzleIndices.push_back(baseIndex + 0);
    m_MuzzleIndices.push_back(baseIndex + 1);
    m_MuzzleIndices.push_back(baseIndex + 2);
    m_MuzzleIndices.push_back(baseIndex + 0);
    m_MuzzleIndices.push_back(baseIndex + 2);
    m_MuzzleIndices.push_back(baseIndex + 3);

    m_MuzzleIndexCount = 6;
}

void Weapon::CreateMuzzleFlashBuffers()
{
    glGenVertexArrays(1, &m_MuzzleVAO);
    glGenBuffers(1, &m_MuzzleVBO);
    glGenBuffers(1, &m_MuzzleEBO);

    glBindVertexArray(m_MuzzleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_MuzzleVBO);
    glBufferData(GL_ARRAY_BUFFER, m_MuzzleVertices.size() * sizeof(WeaponVertex),
                 m_MuzzleVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_MuzzleEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_MuzzleIndices.size() * sizeof(unsigned int),
                 m_MuzzleIndices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WeaponVertex),
                          (void*)offsetof(WeaponVertex, Position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(WeaponVertex),
                          (void*)offsetof(WeaponVertex, Normal));

    glBindVertexArray(0);

    m_MuzzleBuffersInitialized = true;
}

void Weapon::Update(float deltaTime)
{
    if (m_ShootCooldownTimer > 0.0f)
    {
        m_ShootCooldownTimer -= deltaTime;
        if (m_ShootCooldownTimer < 0.0f)
            m_ShootCooldownTimer = 0.0f;
    }

    if (m_MuzzleFlashTimer > 0.0f)
    {
        m_MuzzleFlashTimer -= deltaTime;
        if (m_MuzzleFlashTimer < 0.0f)
            m_MuzzleFlashTimer = 0.0f;
    }

    if (m_RecoilOffset > 0.0f)
    {
        m_RecoilOffset -= m_RecoilRecoverySpeed * deltaTime;
        if (m_RecoilOffset < 0.0f)
            m_RecoilOffset = 0.0f;
    }

    if (m_RecoilRotation > 0.0f)
    {
        m_RecoilRotation -= m_RecoilRecoverySpeed * 60.0f * deltaTime;
        if (m_RecoilRotation < 0.0f)
            m_RecoilRotation = 0.0f;
    }
}

void Weapon::Shoot()
{
    if (!CanShoot())
        return;

    m_ShootCooldownTimer = m_ShootCooldown;
    m_MuzzleFlashTimer = m_FlashParams.Duration;

    m_RecoilOffset = 0.08f;
    m_RecoilRotation = 6.0f;
}

bool Weapon::CanShoot() const
{
    return m_ShootCooldownTimer <= 0.0f;
}

void Weapon::Render(Shader& weaponShader, Shader* flashShader, const Camera& camera, int screenWidth, int screenHeight, float currentTime)
{
    if (!m_GunModel)
        return;

    weaponShader.use();

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                            (float)screenWidth / (float)screenHeight,
                                            0.1f, 100.0f);
    weaponShader.setMat4("projection", projection);

    glm::mat4 view = glm::mat4(1.0f);
    weaponShader.setMat4("view", view);

    glm::vec3 weaponOffset(0.25f, -0.20f, -0.90f);

    weaponOffset.z += m_RecoilOffset;
    weaponOffset.y += m_RecoilOffset * 0.35f;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, weaponOffset);

    model = glm::rotate(model, glm::radians(m_RecoilRotation), glm::vec3(1.0f, 0.0f, 0.0f));

    model = glm::rotate(model, glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(5.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    model = glm::scale(model, glm::vec3(0.01f));

    weaponShader.setMat4("model", model);

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    weaponShader.setMat3("normalMatrix", normalMatrix);

    weaponShader.setVec3("weaponColor", glm::vec3(0.15f, 0.15f, 0.18f));
    weaponShader.setFloat("shininess", 128.0f);
    weaponShader.setVec3("viewPos", camera.Position);

    m_GunModel->Draw(weaponShader);

    if (m_MuzzleFlashTimer > 0.0f && flashShader && m_MuzzleBuffersInitialized)
    {
        RenderMuzzleFlash(*flashShader, projection, view, weaponOffset, currentTime);
    }
}

void Weapon::RenderMuzzleFlash(Shader& flashShader, const glm::mat4& projection,
                               const glm::mat4& view, const glm::vec3& weaponOffset,
                               float currentTime)
{
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDst);
    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    GLboolean depthWriteMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteMask);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);

    flashShader.use();
    flashShader.setMat4("projection", projection);
    flashShader.setMat4("view", view);

    glm::mat4 flashModel = glm::mat4(1.0f);
    glm::vec3 flashOffset = weaponOffset;

    flashOffset.z -= 0.75f;
    flashOffset.y += 0.12f;
    flashOffset.x -= 0.03f;

    flashOffset += m_FlashParams.PositionOffset;

    flashModel = glm::translate(flashModel, flashOffset);

    float sizeVariation = m_FlashParams.MinSizeMultiplier +
        (m_FlashParams.MaxSizeMultiplier - m_FlashParams.MinSizeMultiplier) *
        m_SizeDist(m_RandomEngine);

    float lifeRatio = GetMuzzleFlashLifeRatio();
    float sizeScale = sizeVariation * (1.0f - lifeRatio * 0.3f);

    flashModel = glm::scale(flashModel, glm::vec3(sizeScale));

    flashShader.setMat4("model", flashModel);

    flashShader.setVec3("flashColor", m_FlashParams.Color);
    flashShader.setFloat("intensity", m_FlashParams.Intensity);
    flashShader.setFloat("time", currentTime);
    flashShader.setFloat("lifeRatio", lifeRatio);

    glBindVertexArray(m_MuzzleVAO);
    glDrawElements(GL_TRIANGLES, m_MuzzleIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDepthMask(depthWriteMask ? GL_TRUE : GL_FALSE);
    glDepthFunc(prevDepthFunc);

    if (!blendEnabled)
        glDisable(GL_BLEND);
    glBlendFunc(blendSrc, blendDst);

    if (!depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
}
