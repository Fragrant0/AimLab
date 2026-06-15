#ifndef TARGET_MANAGER_H
#define TARGET_MANAGER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <random>

#include "target_sphere.h"
#include "shader.h"
#include "camera.h"

class Terrain;

class TargetManager
{
public:
    static const int MAX_TARGETS = 6;
    static const float SPAWN_INTERVAL;

    TargetManager();
    ~TargetManager();

    void Initialize();
    void Update(float deltaTime, const Camera& camera, const Terrain* terrain);
    void Render(Shader& shader, const glm::mat4& projection, const glm::mat4& view);

    void SpawnTargetRandom(const Camera& camera, const Terrain* terrain);
    void DeactivateTarget(int index);

    int GetActiveTargetCount() const;
    TargetSphere* GetTarget(int index);

    void SetTargetWall(float distance, float width, float height, float centerY, float lifetime);

private:
    std::vector<TargetSphere*> m_TargetPool;

    float m_SpawnTimer;
    float m_SpawnInterval;

    float m_WallDistance;
    float m_WallWidth;
    float m_WallHeight;
    float m_WallCenterY;
    float m_TargetLifetime;

    std::mt19937 m_RandomEngine;
    std::uniform_real_distribution<float> m_RadiusDist;
    std::uniform_real_distribution<float> m_UniformDist;
    std::uniform_int_distribution<int> m_ColorDist;

    static const glm::vec3 PRESET_COLORS[6];

    glm::vec3 GetRandomPosition(const Camera& camera, const Terrain* terrain, float radius);
    float GetRandomRadius();
    glm::vec3 GetRandomColor();
    int FindInactiveTarget();
    int GetLiveTargetCount() const;
    float GetCenteredOffset(float halfExtent);
    bool HasEnoughSpacing(const glm::vec3& position, float radius) const;
    bool IsSpawnPositionVisible(const Camera& camera, const Terrain* terrain, const glm::vec3& position, float radius) const;
};

#endif
