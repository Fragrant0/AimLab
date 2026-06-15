#ifndef WEAPON_H
#define WEAPON_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>

#include "shader.h"
#include "camera.h"
#include "model.h"

struct WeaponVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
};

struct MuzzleFlashParams
{
    glm::vec3 Color = glm::vec3(1.0f, 0.6f, 0.1f);
    float Size = 0.08f;
    float Duration = 0.05f;
    float Intensity = 2.0f;

    float FlickerSpeed = 10.0f;
    float GlowRadius = 0.5f;
    float CoreBrightness = 1.5f;

    float MinSizeMultiplier = 0.4f;
    float MaxSizeMultiplier = 0.6f;

    glm::vec3 PositionOffset = glm::vec3(0.0f, 0.0f, 0.0f);

    void SetColor(float r, float g, float b) { Color = glm::vec3(r, g, b); }
    void SetSize(float size) { Size = glm::max(0.01f, size); }
    void SetDuration(float duration) { Duration = glm::max(0.01f, duration); }
    void SetIntensity(float intensity) { Intensity = glm::max(0.1f, intensity); }
};

class Weapon
{
public:
    Weapon();
    ~Weapon();

    void Initialize();
    void Cleanup();

    void Update(float deltaTime);
    void Render(Shader& weaponShader, Shader* flashShader, const Camera& camera, int screenWidth, int screenHeight, float currentTime);

    void Shoot();
    bool CanShoot() const;

    MuzzleFlashParams& GetMuzzleFlashParams() { return m_FlashParams; }
    const MuzzleFlashParams& GetMuzzleFlashParams() const { return m_FlashParams; }
    void SetMuzzleFlashParams(const MuzzleFlashParams& params) { m_FlashParams = params; }

    void SetMuzzleFlashColor(const glm::vec3& color) { m_FlashParams.Color = color; }
    void SetMuzzleFlashSize(float size) { m_FlashParams.SetSize(size); }
    void SetMuzzleFlashDuration(float duration) { m_FlashParams.SetDuration(duration); }
    void SetMuzzleFlashIntensity(float intensity) { m_FlashParams.SetIntensity(intensity); }

    bool IsMuzzleFlashActive() const { return m_MuzzleFlashTimer > 0.0f; }
    float GetMuzzleFlashLifeRatio() const {
        if (m_FlashParams.Duration <= 0.0f) return 0.0f;
        return 1.0f - (m_MuzzleFlashTimer / m_FlashParams.Duration);
    }

private:
    void GenerateMuzzleFlashGeometry();
    void CreateMuzzleFlashBuffers();
    void RenderMuzzleFlash(Shader& flashShader, const glm::mat4& projection, const glm::mat4& view, const glm::vec3& weaponOffset, float currentTime);

    std::vector<WeaponVertex> m_MuzzleVertices;
    std::vector<unsigned int> m_MuzzleIndices;

    unsigned int m_MuzzleVAO;
    unsigned int m_MuzzleVBO;
    unsigned int m_MuzzleEBO;
    unsigned int m_MuzzleIndexCount;
    bool m_MuzzleBuffersInitialized;

    float m_RecoilOffset;
    float m_RecoilRotation;
    float m_RecoilRecoverySpeed;

    float m_MuzzleFlashTimer;

    float m_ShootCooldown;
    float m_ShootCooldownTimer;

    Model* m_GunModel;

    MuzzleFlashParams m_FlashParams;

    std::mt19937 m_RandomEngine;
    std::uniform_real_distribution<float> m_SizeDist;
};

#endif
