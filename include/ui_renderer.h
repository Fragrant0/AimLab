#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

#include "shader.h"

class UIRenderer
{
public:
    UIRenderer();
    ~UIRenderer();

    bool Initialize(int screenWidth, int screenHeight);
    void Cleanup();

    void SetScreenSize(int screenWidth, int screenHeight);

    void Begin();
    void End();

    void DrawRect(float x, float y, float width, float height, const glm::vec3& color, float alpha = 1.0f);
    void DrawRectOutline(float x, float y, float width, float height, const glm::vec3& color, float lineWidth = 2.0f);
    void DrawLine(float x1, float y1, float x2, float y2, const glm::vec3& color, float lineWidth = 2.0f, float alpha = 1.0f);

    void DrawCrosshair(float centerX, float centerY, float size, float gap, float thickness, const glm::vec3& color);

    glm::mat4 GetProjection() const { return m_Projection; }

private:
    void InitBuffers();
    void UpdateProjection();

    int m_ScreenWidth;
    int m_ScreenHeight;
    glm::mat4 m_Projection;

    unsigned int m_RectVAO;
    unsigned int m_RectVBO;
    unsigned int m_LineVAO;
    unsigned int m_LineVBO;

    Shader* m_UIShader;
    bool m_Initialized;
    bool m_InRenderPass;
};

#endif
