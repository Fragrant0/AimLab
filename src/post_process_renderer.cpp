#include "post_process_renderer.h"
#include "gl_state_guard.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

PostProcessRenderer::PostProcessRenderer()
    : m_Width(1),
      m_Height(1),
      m_SceneFBO(0),
      m_SceneColor(0),
      m_SceneDepthRBO(0),
      m_QuadVAO(0),
      m_QuadVBO(0),
      m_Initialized(false)
{
}

PostProcessRenderer::~PostProcessRenderer()
{
    Cleanup();
}

bool PostProcessRenderer::Initialize(int width, int height)
{
    if (m_Initialized)
        return Resize(width, height);

    try
    {
        if (!CreateSceneTargets(width, height))
        {
            Cleanup();
            return false;
        }

        m_Shader = std::make_unique<Shader>("shaders/post_process.vs", "shaders/post_process.fs");
        if (!CreateQuad())
        {
            Cleanup();
            return false;
        }
    }
    catch (...)
    {
        Cleanup();
        throw;
    }

    m_Initialized = true;
    return true;
}

void PostProcessRenderer::Cleanup()
{
    DestroySceneTargets();

    if (m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    m_Shader.reset();
    m_Initialized = false;
}

bool PostProcessRenderer::Resize(int width, int height)
{
    if (!m_Initialized)
        return false;

    if (width == m_Width && height == m_Height)
        return true;

    return CreateSceneTargets(width, height);
}

void PostProcessRenderer::BeginScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);
    glViewport(0, 0, m_Width, m_Height);
}

void PostProcessRenderer::EndScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
}

void PostProcessRenderer::RenderToScreen(float timeSeconds, const PostProcessConfig& config)
{
    if (!m_Shader)
        return;

    CapabilityGuard depthTest(GL_DEPTH_TEST, false);
    CapabilityGuard blend(GL_BLEND, false);

    glViewport(0, 0, m_Width, m_Height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_Shader->use();
    m_Shader->setInt("sceneTexture", 0);
    m_Shader->setFloat("exposure", std::max(config.Exposure, 0.0f));
    m_Shader->setFloat("gammaValue", config.Gamma);
    m_Shader->setFloat("contrast", std::max(config.Contrast, 0.0f));
    m_Shader->setFloat("saturation", std::max(config.Saturation, 0.0f));
    m_Shader->setFloat("vignetteStrength", std::clamp(config.Vignette, 0.0f, 1.0f));
    m_Shader->setFloat("timeSeconds", timeSeconds);
    m_Shader->setBool("pixelateEnabled", config.PixelateEnabled);
    m_Shader->setFloat("pixelSize", config.PixelSize);
    m_Shader->setVec2("resolution", static_cast<float>(m_Width), static_cast<float>(m_Height));
    m_Shader->setBool("underwaterEnabled", config.UnderwaterEnabled);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneColor);

    RenderFullscreenQuad();
}

bool PostProcessRenderer::CreateSceneTargets(int width, int height)
{
    GLint maxTextureSize = 0;
    GLint maxRenderbufferSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    if (width <= 0 || height <= 0 || width > maxTextureSize || height > maxTextureSize ||
        width > maxRenderbufferSize || height > maxRenderbufferSize)
    {
        std::cerr << "[PostProcess] Invalid scene target dimensions: " << width << "x" << height
                  << " (texture limit " << maxTextureSize << ", renderbuffer limit "
                  << maxRenderbufferSize << ")." << std::endl;
        return false;
    }

    GLint previousDrawFBO = 0;
    GLint previousReadFBO = 0;
    GLint previousTexture = 0;
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFBO);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);

    unsigned int sceneFBO = 0;
    unsigned int sceneColor = 0;
    unsigned int sceneDepthRBO = 0;
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    glGenTextures(1, &sceneColor);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);

    glGenRenderbuffers(1, &sceneDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRBO);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const bool complete = sceneFBO != 0 && sceneColor != 0 && sceneDepthRBO != 0 &&
                          status == GL_FRAMEBUFFER_COMPLETE;

    // When replacing a currently bound target, keep its replacement bound.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, complete && m_SceneFBO != 0 &&
        static_cast<unsigned int>(previousDrawFBO) == m_SceneFBO ? sceneFBO : previousDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, complete && m_SceneFBO != 0 &&
        static_cast<unsigned int>(previousReadFBO) == m_SceneFBO ? sceneFBO : previousReadFBO);
    glBindTexture(GL_TEXTURE_2D, complete && m_SceneColor != 0 &&
        static_cast<unsigned int>(previousTexture) == m_SceneColor ? sceneColor : previousTexture);
    glBindRenderbuffer(GL_RENDERBUFFER, complete && m_SceneDepthRBO != 0 &&
        static_cast<unsigned int>(previousRenderbuffer) == m_SceneDepthRBO ? sceneDepthRBO : previousRenderbuffer);

    if (!complete)
    {
        if (sceneColor) glDeleteTextures(1, &sceneColor);
        if (sceneDepthRBO) glDeleteRenderbuffers(1, &sceneDepthRBO);
        if (sceneFBO) glDeleteFramebuffers(1, &sceneFBO);
        std::cerr << "[PostProcess] Scene framebuffer is incomplete (status 0x"
                  << std::hex << status << std::dec << ")." << std::endl;
        return false;
    }

    DestroySceneTargets();
    m_SceneFBO = sceneFBO;
    m_SceneColor = sceneColor;
    m_SceneDepthRBO = sceneDepthRBO;
    m_Width = width;
    m_Height = height;
    return true;
}

bool PostProcessRenderer::CreateQuad()
{
    const float vertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };

    GLint previousVAO = 0;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    if (!m_QuadVAO || !m_QuadVBO)
    {
        std::cerr << "[PostProcess] Failed to allocate fullscreen quad objects." << std::endl;
        return false;
    }

    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(previousVAO);
    glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
    if (bufferSize != static_cast<GLint>(sizeof(vertices)))
    {
        std::cerr << "[PostProcess] Failed to allocate fullscreen quad storage." << std::endl;
        return false;
    }
    return true;
}

void PostProcessRenderer::RenderFullscreenQuad() const
{
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void PostProcessRenderer::DestroySceneTargets()
{
    if (m_SceneColor) { glDeleteTextures(1, &m_SceneColor); m_SceneColor = 0; }
    if (m_SceneDepthRBO) { glDeleteRenderbuffers(1, &m_SceneDepthRBO); m_SceneDepthRBO = 0; }
    if (m_SceneFBO) { glDeleteFramebuffers(1, &m_SceneFBO); m_SceneFBO = 0; }
}
