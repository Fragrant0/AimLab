#include "post_process_renderer.h"
#include "gl_state_guard.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

PostProcessRenderer::PostProcessRenderer()
    : m_Width(1),
      m_Height(1),
      m_BloomWidth(1),
      m_BloomHeight(1),
      m_SceneFBO(0),
      m_SceneColor(0),
      m_SceneDepthRBO(0),
      m_BloomFBO{0, 0},
      m_BloomColor{0, 0},
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
        return true;

    m_Width = width;
    m_Height = height;

    m_Shader = std::make_unique<Shader>("shaders/post_process.vs", "shaders/post_process.fs");
    m_BloomExtractShader = std::make_unique<Shader>("shaders/post_process.vs", "shaders/bloom_extract.fs");
    m_BloomBlurShader = std::make_unique<Shader>("shaders/post_process.vs", "shaders/bloom_blur.fs");
    CreateSceneTargets();
    CreateBloomTargets();
    CreateQuad();

    m_Initialized = true;
    return true;
}

void PostProcessRenderer::Cleanup()
{
    DestroySceneTargets();
    DestroyBloomTargets();

    if (m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    m_BloomBlurShader.reset();
    m_BloomExtractShader.reset();
    m_Shader.reset();
    m_Initialized = false;
}

void PostProcessRenderer::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    m_Width = width;
    m_Height = height;

    if (!m_Initialized)
        return;

    DestroySceneTargets();
    DestroyBloomTargets();
    CreateSceneTargets();
    CreateBloomTargets();
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

    unsigned int bloomTexture = 0;
    float bloomIntensity = 0.0f;
    if (config.Bloom.Enabled && config.Bloom.Intensity > 0.0001f)
    {
        bloomTexture = RenderBloomTexture(config);
        bloomIntensity = bloomTexture != 0 ? config.Bloom.Intensity : 0.0f;
    }

    glViewport(0, 0, m_Width, m_Height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_Shader->use();
    m_Shader->setInt("sceneTexture", 0);
    m_Shader->setInt("bloomTexture", 1);
    m_Shader->setFloat("exposure", config.Exposure);
    m_Shader->setFloat("gammaValue", config.Gamma);
    m_Shader->setFloat("contrast", config.Contrast);
    m_Shader->setFloat("saturation", config.Saturation);
    m_Shader->setFloat("vignette", config.Vignette);
    m_Shader->setFloat("chromaticAberration", config.ChromaticAberration);
    m_Shader->setFloat("filmGrain", config.FilmGrain);
    m_Shader->setFloat("bloomIntensity", bloomIntensity);
    m_Shader->setFloat("timeSeconds", timeSeconds);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneColor);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture != 0 ? bloomTexture : m_BloomColor[0]);

    RenderFullscreenQuad();
}

void PostProcessRenderer::CreateSceneTargets()
{
    glGenFramebuffers(1, &m_SceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);

    glGenTextures(1, &m_SceneColor);
    glBindTexture(GL_TEXTURE_2D, m_SceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneColor, 0);

    glGenRenderbuffers(1, &m_SceneDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_SceneDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_SceneDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "[PostProcess] Scene framebuffer is incomplete." << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessRenderer::CreateBloomTargets()
{
    m_BloomWidth = std::max(1, m_Width);
    m_BloomHeight = std::max(1, m_Height);

    glGenFramebuffers(2, m_BloomFBO);
    glGenTextures(2, m_BloomColor);

    for (int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_BloomColor[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_BloomWidth, m_BloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomColor[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "[PostProcess] Bloom framebuffer " << i << " is incomplete." << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessRenderer::CreateQuad()
{
    const float vertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

unsigned int PostProcessRenderer::RenderBloomTexture(const PostProcessConfig& config)
{
    if (!m_BloomExtractShader || !m_BloomBlurShader || m_BloomColor[0] == 0 || m_BloomColor[1] == 0)
        return 0;

    glViewport(0, 0, m_BloomWidth, m_BloomHeight);

    m_BloomExtractShader->use();
    m_BloomExtractShader->setInt("sceneTexture", 0);
    m_BloomExtractShader->setFloat("threshold", config.Bloom.Threshold);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneColor);
    glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    RenderFullscreenQuad();

    const float radius = std::clamp(config.Bloom.Radius, 0.0f, 1.0f);
    const int blurPairs = std::clamp(2 + static_cast<int>(std::round(radius * 4.0f)), 2, 6);
    const float sampleRadius = 1.0f + radius * 1.5f;

    unsigned int sourceTexture = m_BloomColor[0];
    m_BloomBlurShader->use();
    m_BloomBlurShader->setInt("image", 0);
    m_BloomBlurShader->setVec2("texelSize", 1.0f / static_cast<float>(m_BloomWidth),
                               1.0f / static_cast<float>(m_BloomHeight));
    m_BloomBlurShader->setFloat("sampleRadius", sampleRadius);

    for (int pass = 0; pass < blurPairs * 2; ++pass)
    {
        const bool horizontal = (pass % 2) == 0;
        const int targetIndex = horizontal ? 1 : 0;

        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[targetIndex]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        m_BloomBlurShader->setVec2("direction", horizontal ? 1.0f : 0.0f,
                                   horizontal ? 0.0f : 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        RenderFullscreenQuad();
        sourceTexture = m_BloomColor[targetIndex];
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return sourceTexture;
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

void PostProcessRenderer::DestroyBloomTargets()
{
    if (m_BloomColor[0] || m_BloomColor[1])
    {
        glDeleteTextures(2, m_BloomColor);
        m_BloomColor[0] = 0;
        m_BloomColor[1] = 0;
    }
    if (m_BloomFBO[0] || m_BloomFBO[1])
    {
        glDeleteFramebuffers(2, m_BloomFBO);
        m_BloomFBO[0] = 0;
        m_BloomFBO[1] = 0;
    }
}
