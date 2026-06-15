#include "ui_renderer.h"

#include <algorithm>

UIRenderer::UIRenderer()
    : m_ScreenWidth(800),
      m_ScreenHeight(600),
      m_Projection(glm::mat4(1.0f)),
      m_RectVAO(0),
      m_RectVBO(0),
      m_LineVAO(0),
      m_LineVBO(0),
      m_UIShader(nullptr),
      m_Initialized(false),
      m_InRenderPass(false)
{
}

UIRenderer::~UIRenderer()
{
    Cleanup();
}

bool UIRenderer::Initialize(int screenWidth, int screenHeight)
{
    if (m_Initialized)
        return true;

    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    m_UIShader = new Shader("shaders/ui.vs", "shaders/ui.fs");
    if (!m_UIShader)
        return false;

    InitBuffers();
    UpdateProjection();

    m_Initialized = true;
    return true;
}

void UIRenderer::Cleanup()
{
    if (!m_Initialized)
        return;

    if (m_RectVAO) { glDeleteVertexArrays(1, &m_RectVAO); m_RectVAO = 0; }
    if (m_RectVBO) { glDeleteBuffers(1, &m_RectVBO); m_RectVBO = 0; }
    if (m_LineVAO) { glDeleteVertexArrays(1, &m_LineVAO); m_LineVAO = 0; }
    if (m_LineVBO) { glDeleteBuffers(1, &m_LineVBO); m_LineVBO = 0; }

    delete m_UIShader;
    m_UIShader = nullptr;

    m_Initialized = false;
    m_InRenderPass = false;
}

void UIRenderer::SetScreenSize(int screenWidth, int screenHeight)
{
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;
    UpdateProjection();
}

void UIRenderer::UpdateProjection()
{
    m_Projection = glm::ortho(0.0f, static_cast<float>(m_ScreenWidth),
                              0.0f, static_cast<float>(m_ScreenHeight),
                              -1.0f, 1.0f);
}

void UIRenderer::InitBuffers()
{
    glGenVertexArrays(1, &m_RectVAO);
    glGenBuffers(1, &m_RectVBO);
    glBindVertexArray(m_RectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_RectVBO);
    glBufferData(GL_ARRAY_BUFFER, 36 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_LineVAO);
    glGenBuffers(1, &m_LineVBO);
    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void UIRenderer::Begin()
{
    if (!m_Initialized || !m_UIShader)
        return;

    m_InRenderPass = true;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_UIShader->use();
    m_UIShader->setMat4("projection", m_Projection);
}

void UIRenderer::End()
{
    m_InRenderPass = false;

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void UIRenderer::DrawRect(float x, float y, float width, float height, const glm::vec3& color, float alpha)
{
    if (!m_Initialized || !m_UIShader)
        return;

    float x2 = x + width;
    float y2 = y + height;

    float vertices[36] = {
        x,  y,  0.0f, color.r, color.g, color.b,
        x2, y,  0.0f, color.r, color.g, color.b,
        x2, y2, 0.0f, color.r, color.g, color.b,
        x,  y,  0.0f, color.r, color.g, color.b,
        x2, y2, 0.0f, color.r, color.g, color.b,
        x,  y2, 0.0f, color.r, color.g, color.b
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_RectVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    m_UIShader->setFloat("alpha", alpha);

    glBindVertexArray(m_RectVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UIRenderer::DrawRectOutline(float x, float y, float width, float height, const glm::vec3& color, float lineWidth)
{
    if (!m_Initialized)
        return;

    float x2 = x + width;
    float y2 = y + height;

    GLfloat prevLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    glLineWidth(lineWidth);

    DrawLine(x, y2, x2, y2, color);
    DrawLine(x, y, x2, y, color);
    DrawLine(x, y, x, y2, color);
    DrawLine(x2, y, x2, y2, color);

    glLineWidth(prevLineWidth);
}

void UIRenderer::DrawLine(float x1, float y1, float x2, float y2, const glm::vec3& color, float lineWidth, float alpha)
{
    if (!m_Initialized || !m_UIShader)
        return;

    GLfloat prevLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    glLineWidth(lineWidth);

    float vertices[12] = {
        x1, y1, 0.0f, color.r, color.g, color.b,
        x2, y2, 0.0f, color.r, color.g, color.b
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    m_UIShader->setFloat("alpha", alpha);

    glBindVertexArray(m_LineVAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);

    glLineWidth(prevLineWidth);
}

void UIRenderer::DrawCrosshair(float centerX, float centerY, float size, float gap, float thickness, const glm::vec3& color)
{
    DrawRect(centerX - thickness * 0.5f, centerY + gap, thickness, size, color);
    DrawRect(centerX - thickness * 0.5f, centerY - gap - size, thickness, size, color);
    DrawRect(centerX - gap - size, centerY - thickness * 0.5f, size, thickness, color);
    DrawRect(centerX + gap, centerY - thickness * 0.5f, size, thickness, color);

    float centerDot = std::max(2.0f, thickness);
    DrawRect(centerX - centerDot * 0.5f, centerY - centerDot * 0.5f, centerDot, centerDot, color);
}
