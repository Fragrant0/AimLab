#ifndef SKYBOX_RENDERER_H
#define SKYBOX_RENDERER_H

#include <glm/glm.hpp>

struct MapConfig;

class SkyboxRenderer
{
public:
    SkyboxRenderer();
    ~SkyboxRenderer();

    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;

    bool Initialize();
    void Render(const MapConfig& map, const glm::mat4& projection, const glm::mat4& view, float rotation);
    void Cleanup();

private:
    unsigned int m_VAO;
    unsigned int m_VBO;
};

#endif
