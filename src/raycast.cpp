#include "raycast.h"

#include <limits>

Ray Raycast::ScreenToWorldRay(const glm::vec3& cameraPos, const glm::vec3& cameraFront)
{
    Ray ray;
    ray.origin = cameraPos;
    ray.direction = glm::normalize(cameraFront);
    return ray;
}

TargetSphere* Raycast::RaycastAgainstTargets(const Ray& ray, TargetManager* targetManager, float& hitT)
{
    if (!targetManager)
        return nullptr;

    TargetSphere* closestTarget = nullptr;
    float closestT = std::numeric_limits<float>::max();

    for (int i = 0; i < TargetManager::MAX_TARGETS; ++i)
    {
        TargetSphere* target = targetManager->GetTarget(i);
        if (!target || !target->IsActive())
            continue;

        float t = 0.0f;
        if (target->CheckCollision(ray.origin, ray.direction, t))
        {
            if (t < closestT && t > 0.0f)
            {
                closestT = t;
                closestTarget = target;
            }
        }
    }

    if (closestTarget)
        hitT = closestT;

    return closestTarget;
}
