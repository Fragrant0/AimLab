#include "font_renderer.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cctype>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>

FontRenderer::FontRenderer()
    : m_AtlasTexture(0),
      m_VAO(0),
      m_VBO(0),
      m_ScreenWidth(800),
      m_ScreenHeight(600),
      m_Projection(1.0f),
      m_VertexCount(0),
      m_Initialized(false)
{
    memset(m_CData, 0, sizeof(m_CData));
    memset(m_VertexBuffer, 0, sizeof(m_VertexBuffer));
}

FontRenderer::~FontRenderer()
{
    Cleanup();
}

bool FontRenderer::Initialize(int screenWidth, int screenHeight, const std::string& fontPath)
{
    if (m_Initialized)
        return true;

    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "[FontRenderer] Failed to load font: " << fontPath << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read(reinterpret_cast<char*>(ttfBuffer.data()), size))
    {
        std::cerr << "[FontRenderer] Failed to read font: " << fontPath << std::endl;
        return false;
    }

    unsigned char tempBitmap[ATLAS_W * ATLAS_H];
    stbtt_BakeFontBitmap(ttfBuffer.data(), 0, 48.0f, tempBitmap,
                          ATLAS_W, ATLAS_H, FIRST_CHAR, CHAR_COUNT, m_CData);

    glGenTextures(1, &m_AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, tempBitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(TextVertex), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)offsetof(TextVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)offsetof(TextVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)offsetof(TextVertex, r));
    glBindVertexArray(0);

    m_Shader = std::make_unique<Shader>("shaders/font.vs", "shaders/font.fs");
    m_Projection = glm::ortho(0.0f, static_cast<float>(m_ScreenWidth),
                               0.0f, static_cast<float>(m_ScreenHeight),
                               -1.0f, 1.0f);

    m_Initialized = true;
    std::cout << "[FontRenderer] Initialized with font: " << fontPath << std::endl;
    return true;
}

void FontRenderer::Cleanup()
{
    if (!m_Initialized)
        return;

    if (m_AtlasTexture) { glDeleteTextures(1, &m_AtlasTexture); m_AtlasTexture = 0; }
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    m_Shader.reset();

    m_Initialized = false;
}

void FontRenderer::SetScreenSize(int screenWidth, int screenHeight)
{
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;
    m_Projection = glm::ortho(0.0f, static_cast<float>(m_ScreenWidth),
                               0.0f, static_cast<float>(m_ScreenHeight),
                               -1.0f, 1.0f);
}

void FontRenderer::DrawText(float x, float y, float scale, const std::string& text,
                             const glm::vec3& color, float alpha)
{
    if (!m_Initialized || text.empty())
        return;

    float currentX = x;
    float currentY = static_cast<float>(m_ScreenHeight) - y;

    for (char c : text)
    {
        if (c >= FIRST_CHAR && c < FIRST_CHAR + CHAR_COUNT)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(m_CData, ATLAS_W, ATLAS_H, c - FIRST_CHAR,
                               &currentX, &currentY, &q, 1);

            if (m_VertexCount + 6 <= MAX_VERTICES)
            {
                float y0 = static_cast<float>(m_ScreenHeight) - q.y0;
                float y1 = static_cast<float>(m_ScreenHeight) - q.y1;

                TextVertex* v = &m_VertexBuffer[m_VertexCount];
                v[0] = { q.x0, y0, q.s0, q.t0, color.r, color.g, color.b, alpha };
                v[1] = { q.x1, y0, q.s1, q.t0, color.r, color.g, color.b, alpha };
                v[2] = { q.x1, y1, q.s1, q.t1, color.r, color.g, color.b, alpha };
                v[3] = { q.x0, y0, q.s0, q.t0, color.r, color.g, color.b, alpha };
                v[4] = { q.x1, y1, q.s1, q.t1, color.r, color.g, color.b, alpha };
                v[5] = { q.x0, y1, q.s0, q.t1, color.r, color.g, color.b, alpha };
                m_VertexCount += 6;
            }
        }
    }
}

void FontRenderer::Flush()
{
    if (!m_Initialized || m_VertexCount == 0)
        return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_Shader->use();
    m_Shader->setMat4("projection", m_Projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    m_VertexCount * sizeof(TextVertex), m_VertexBuffer);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    m_VertexCount = 0;
}

float FontRenderer::GetTextWidth(float scale, const std::string& text) const
{
    if (!m_Initialized || text.empty())
        return 0.0f;

    float width = 0.0f;
    for (char c : text)
    {
        if (c >= FIRST_CHAR && c < FIRST_CHAR + CHAR_COUNT)
        {
            width += m_CData[c - FIRST_CHAR].xadvance * scale;
        }
    }
    return width;
}

float FontRenderer::GetCharHeight(float scale) const
{
    return 48.0f * scale;
}

float FontRenderer::GetLineHeight(float scale) const
{
    return 58.0f * scale;
}
