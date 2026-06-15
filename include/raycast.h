#ifndef RAYCAST_H
#define RAYCAST_H

#include <glm/glm.hpp>

#include "target_manager.h"

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
};

class Raycast
{
public:
    static Ray ScreenToWorldRay(const glm::vec3& cameraPos, const glm::vec3& cameraFront);
    static TargetSphere* RaycastAgainstTargets(const Ray& ray, TargetManager* targetManager, float& hitT);
};

#endif
