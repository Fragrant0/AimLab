#ifndef POST_PROCESS_RENDERER_H
#define POST_PROCESS_RENDERER_H

#include <glad/glad.h>

#include <memory>

#include "lighting.h"
#include "shader.h"

class PostProcessRenderer
{
public:
    PostProcessRenderer();
    ~PostProcessRenderer();

    bool Initialize(int width, int height);
    void Cleanup();
    void Resize(int width, int height);

    void BeginScene();
    void EndScene();
    void RenderToScreen(float timeSeconds, const PostProcessConfig& config);

private:
    void CreateSceneTargets();
    void CreateQuad();
    void DestroySceneTargets();

    int m_Width;
    int m_Height;
    unsigned int m_SceneFBO;
    unsigned int m_SceneColor;
    unsigned int m_SceneDepthRBO;
    unsigned int m_QuadVAO;
    unsigned int m_QuadVBO;
    std::unique_ptr<Shader> m_Shader;
    bool m_Initialized;
};

#endif
