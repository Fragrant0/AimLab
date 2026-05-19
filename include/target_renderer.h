#ifndef TARGET_RENDERER_H
#define TARGET_RENDERER_H

#include <glm/glm.hpp>

class Camera;
class TargetManager;

class TargetRenderer
{
public:
    void Render(TargetManager* targetManager,
                const Camera& camera,
                const glm::vec3& ambientLight,
                const glm::mat4& projection,
                const glm::mat4& view);
};

#endif
