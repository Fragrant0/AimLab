#include "target_manager.h"
#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <memory>

const float TargetManager::SPAWN_INTERVAL = 2.0f;

const glm::vec3 TargetManager::PRESET_COLORS[6] = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 1.0f, 1.0f)
};

TargetManager::TargetManager()
    : m_SpawnTimer(SPAWN_INTERVAL),
      m_SpawnInterval(SPAWN_INTERVAL),
      m_WallDistance(15.0f),
      m_WallWidth(12.0f),
      m_WallHeight(8.0f),
      m_WallCenterY(2.5f),
      m_TargetLifetime(5.0f),
      m_RandomEngine(static_cast<unsigned int>(time(nullptr))),
      m_RadiusDist(0.3f, 0.6f),
      m_UniformDist(0.0f, 1.0f),
      m_ColorDist(0, 5)
{
}

TargetManager::~TargetManager()
= default;

void TargetManager::Initialize()
{
    for (int i = 0; i < MAX_TARGETS; ++i)
    {
        m_TargetPool.push_back(std::make_unique<TargetSphere>());
    }
}

void TargetManager::Update(float deltaTime, const Camera& camera, const Terrain* terrain)
{
    if (GetLiveTargetCount() < MAX_TARGETS)
    {
        SpawnTargetRandom(camera, terrain);
    }

    for (const auto& target : m_TargetPool)
    {
        if (target && !target->IsFullyDead())
        {
            target->Update(deltaTime);
        }
    }
}

void TargetManager::Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view)
{
    for (const auto& target : m_TargetPool)
    {
        if (target && !target->IsFullyDead())
        {
            target->Render(shader, projection, view);
        }
    }
}

void TargetManager::SpawnTargetRandom(const Camera& camera, const Terrain* terrain)
{
    int index = FindInactiveTarget();
    if (index == -1)
        return;

    float radius = GetRandomRadius();
    glm::vec3 position = GetRandomPosition(camera, terrain, radius);
    glm::vec3 color = GetRandomColor();

    float lifetime = m_TargetLifetime + (m_UniformDist(m_RandomEngine) * 2.0f - 1.0f) * 2.0f;
    lifetime = std::max(2.0f, lifetime);

    m_TargetPool[index]->Activate(radius, position, color, lifetime);
}

void TargetManager::DeactivateTarget(int index)
{
    if (index >= 0 && index < static_cast<int>(m_TargetPool.size()))
    {
        m_TargetPool[index]->Deactivate();
    }
}

int TargetManager::GetActiveTargetCount() const
{
    int count = 0;
    for (const auto& target : m_TargetPool)
    {
        if (target && target->IsActive())
            ++count;
    }
    return count;
}

TargetSphere* TargetManager::GetTarget(int index)
{
    if (index >= 0 && index < static_cast<int>(m_TargetPool.size()))
    {
        return m_TargetPool[index].get();
    }
    return nullptr;
}

void TargetManager::SetTargetWall(float distance, float width, float height, float centerY, float lifetime)
{
    m_WallDistance = distance;
    m_WallWidth = width;
    m_WallHeight = height;
    m_WallCenterY = centerY;
    m_TargetLifetime = lifetime;
}

glm::vec3 TargetManager::GetRandomPosition(const Camera& camera, const Terrain* terrain, float radius)
{
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const float halfWidth = m_WallWidth * 0.5f;
    const float halfHeight = m_WallHeight * 0.5f;

    glm::vec3 horizontalForward(camera.Front.x, 0.0f, camera.Front.z);
    if (glm::length(horizontalForward) < 0.001f)
    {
        horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    else
    {
        horizontalForward = glm::normalize(horizontalForward);
    }
    const glm::vec3 horizontalRight = glm::normalize(glm::cross(horizontalForward, worldUp));

    glm::vec3 fallback = camera.Position + horizontalForward * std::max(m_WallDistance * 0.8f, 6.0f);
    if (terrain && terrain->IsInitialized())
    {
        const float fallbackTerrainHeight = terrain->GetHeightAt(fallback.x, fallback.z);
        fallback.y = std::max(camera.Position.y + 1.5f, fallbackTerrainHeight + 2.0f);
    }
    else
    {
        fallback.y = camera.Position.y + 1.5f;
    }

    for (int attempt = 0; attempt < 36; ++attempt)
    {
        const float centeredX = GetCenteredOffset(halfWidth * 0.65f);
        const float centeredY = GetCenteredOffset(halfHeight * 0.55f);
        const float uniformX = (m_UniformDist(m_RandomEngine) * 2.0f - 1.0f) * halfWidth * 0.9f;
        const float uniformY = (m_UniformDist(m_RandomEngine) * 2.0f - 1.0f) * halfHeight * 0.8f;
        const float offsetX = centeredX * 0.45f + uniformX * 0.55f;
        const float offsetY = centeredY * 0.4f + uniformY * 0.6f;
        const float distanceScale = 0.85f + m_UniformDist(m_RandomEngine) * 0.35f;
        const float radiusPadding = radius + 0.2f;

        glm::vec3 position = camera.Position + horizontalForward * (m_WallDistance * distanceScale) + horizontalRight * offsetX;

        float localTerrainHeight = 0.0f;
        if (terrain && terrain->IsInitialized())
        {
            localTerrainHeight = terrain->GetHeightAt(position.x, position.z);
        }

        const float minVisibleHeight = std::max(camera.Position.y - 1.0f, localTerrainHeight + radiusPadding);
        const float maxVisibleHeight = std::max(minVisibleHeight + 0.5f, localTerrainHeight + m_WallCenterY + halfHeight + radiusPadding);
        position.y = glm::clamp(localTerrainHeight + m_WallCenterY + offsetY, minVisibleHeight, maxVisibleHeight);

        if (IsSpawnPositionVisible(camera, terrain, position, radius) && HasEnoughSpacing(position, radius))
        {
            return position;
        }
    }

    return fallback;
}

float TargetManager::GetRandomRadius()
{
    return m_RadiusDist(m_RandomEngine);
}

glm::vec3 TargetManager::GetRandomColor()
{
    int colorIndex = m_ColorDist(m_RandomEngine);
    return PRESET_COLORS[colorIndex];
}

int TargetManager::FindInactiveTarget()
{
    for (int i = 0; i < static_cast<int>(m_TargetPool.size()); ++i)
    {
        if (m_TargetPool[i]->IsFullyDead())
        {
            return i;
        }
    }
    return -1;
}

int TargetManager::GetLiveTargetCount() const
{
    int count = 0;
    for (const auto& target : m_TargetPool)
    {
        if (target && !target->IsFullyDead())
            ++count;
    }
    return count;
}

float TargetManager::GetCenteredOffset(float halfExtent)
{
    const float r1 = m_UniformDist(m_RandomEngine) * 2.0f - 1.0f;
    const float r2 = m_UniformDist(m_RandomEngine) * 2.0f - 1.0f;
    const float r3 = m_UniformDist(m_RandomEngine) * 2.0f - 1.0f;
    return ((r1 + r2 + r3) / 3.0f) * halfExtent;
}

bool TargetManager::HasEnoughSpacing(const glm::vec3& position, float radius) const
{
    for (const auto& target : m_TargetPool)
    {
        if (!target || target->IsFullyDead())
            continue;

        const float existingRadius = target->GetRadius();
        const float minDistance = radius + existingRadius + 1.0f;
        if (glm::distance(position, target->GetPosition()) < minDistance)
            return false;
    }

    return true;
}

bool TargetManager::IsSpawnPositionVisible(const Camera& camera, const Terrain* terrain, const glm::vec3& position, float radius) const
{
    if (!terrain || !terrain->IsInitialized())
        return true;

    const float localTerrainHeight = terrain->GetHeightAt(position.x, position.z);
    if (position.y - radius <= localTerrainHeight + 0.15f)
        return false;

    const glm::vec3 toTarget = position - camera.Position;
    const float distance = glm::length(toTarget);
    if (distance < 0.001f)
        return false;

    const int sampleCount = std::max(6, static_cast<int>(distance / 1.5f));
    for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex)
    {
        const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
        const glm::vec3 samplePos = camera.Position + toTarget * t;
        const float terrainHeight = terrain->GetHeightAt(samplePos.x, samplePos.z);
        if (terrainHeight + 0.2f >= samplePos.y - radius * 0.35f)
            return false;
    }

    return true;
}
