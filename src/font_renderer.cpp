#include "font_renderer.h"
#include "gl_state_guard.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cctype>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_set>

namespace
{
    bool CanReadFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    std::string SelectUIFontPath(const std::string& requestedPath)
    {
        const std::vector<std::string> candidates = {
            "resources/fonts/msyh.ttc",
            "resources/fonts/simhei.ttf",
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/simhei.ttf",
            "C:/Windows/Fonts/simsun.ttc",
            requestedPath
        };

        for (const std::string& path : candidates)
        {
            if (!path.empty() && CanReadFile(path))
                return path;
        }

        return requestedPath;
    }

    std::vector<unsigned int> DecodeUTF8(const std::string& text)
    {
        std::vector<unsigned int> result;
        for (size_t i = 0; i < text.size();)
        {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < 0x80)
            {
                result.push_back(c);
                ++i;
            }
            else if ((c & 0xE0) == 0xC0 && i + 1 < text.size())
            {
                result.push_back(((c & 0x1F) << 6) |
                                 (static_cast<unsigned char>(text[i + 1]) & 0x3F));
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < text.size())
            {
                result.push_back(((c & 0x0F) << 12) |
                                 ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(text[i + 2]) & 0x3F));
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0 && i + 3 < text.size())
            {
                result.push_back(((c & 0x07) << 18) |
                                 ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                                 ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(text[i + 3]) & 0x3F));
                i += 4;
            }
            else
            {
                ++i;
            }
        }
        return result;
    }

    std::vector<int> BuildGlyphCodepoints()
    {
        std::vector<int> codepoints;
        std::unordered_set<unsigned int> seen;
        auto add = [&](unsigned int codepoint) {
            if (seen.insert(codepoint).second)
                codepoints.push_back(static_cast<int>(codepoint));
        };

        for (unsigned int c = 32; c <= 126; ++c)
            add(c);

        const std::string debugText =
            "调试面板后处理后效泛光阴影线框选择调整重置"
            "曝光强度阈值半径对比饱和主光方向天空旋转开关左右";
        for (unsigned int codepoint : DecodeUTF8(debugText))
            add(codepoint);

        return codepoints;
    }
}

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

    const std::string resolvedFontPath = SelectUIFontPath(fontPath);
    std::ifstream file(resolvedFontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "[FontRenderer] Failed to load font: " << resolvedFontPath << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read(reinterpret_cast<char*>(ttfBuffer.data()), size))
    {
        std::cerr << "[FontRenderer] Failed to read font: " << resolvedFontPath << std::endl;
        return false;
    }

    std::vector<unsigned char> tempBitmap(ATLAS_W * ATLAS_H, 0);
    std::vector<int> codepoints = BuildGlyphCodepoints();
    std::vector<stbtt_packedchar> packedChars(codepoints.size());

    stbtt_pack_context packContext;
    if (!stbtt_PackBegin(&packContext, tempBitmap.data(), ATLAS_W, ATLAS_H, 0, 1, nullptr))
    {
        std::cerr << "[FontRenderer] Failed to create font atlas." << std::endl;
        return false;
    }
    stbtt_PackSetOversampling(&packContext, 2, 2);
    stbtt_PackSetSkipMissingCodepoints(&packContext, 1);

    stbtt_pack_range range = {};
    range.font_size = 48.0f;
    range.array_of_unicode_codepoints = codepoints.data();
    range.num_chars = static_cast<int>(codepoints.size());
    range.chardata_for_range = packedChars.data();

    const int packed = stbtt_PackFontRanges(&packContext, ttfBuffer.data(), 0, &range, 1);
    stbtt_PackEnd(&packContext);
    if (!packed)
        std::cerr << "[FontRenderer] Font atlas did not pack every requested glyph." << std::endl;

    m_Glyphs.clear();
    for (size_t i = 0; i < codepoints.size(); ++i)
    {
        if (packedChars[i].x1 > packedChars[i].x0 || codepoints[i] == 32)
            m_Glyphs[static_cast<unsigned int>(codepoints[i])] = packedChars[i];
    }

    glGenTextures(1, &m_AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, tempBitmap.data());
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
    std::cout << "[FontRenderer] Initialized with font: " << resolvedFontPath
              << " | glyphs: " << m_Glyphs.size() << std::endl;
    return true;
}

void FontRenderer::Cleanup()
{
    if (!m_Initialized)
        return;

    if (m_AtlasTexture) { glDeleteTextures(1, &m_AtlasTexture); m_AtlasTexture = 0; }
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    m_Glyphs.clear();
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
    if (!m_Initialized || text.empty() || scale <= 0.0f)
        return;

    float currentX = x / scale;
    float currentY = (static_cast<float>(m_ScreenHeight) - y) / scale;

    for (unsigned int codepoint : DecodeUTF8(text))
    {
        auto glyphIt = m_Glyphs.find(codepoint);
        if (glyphIt != m_Glyphs.end())
        {
            stbtt_aligned_quad q;
            stbtt_GetPackedQuad(&glyphIt->second, ATLAS_W, ATLAS_H, 0,
                                &currentX, &currentY, &q, 1);

            if (m_VertexCount + 6 <= MAX_VERTICES)
            {
                float x0 = q.x0 * scale;
                float x1 = q.x1 * scale;
                float y0 = static_cast<float>(m_ScreenHeight) - q.y0 * scale;
                float y1 = static_cast<float>(m_ScreenHeight) - q.y1 * scale;

                TextVertex* v = &m_VertexBuffer[m_VertexCount];
                v[0] = { x0, y0, q.s0, q.t0, color.r, color.g, color.b, alpha };
                v[1] = { x1, y0, q.s1, q.t0, color.r, color.g, color.b, alpha };
                v[2] = { x1, y1, q.s1, q.t1, color.r, color.g, color.b, alpha };
                v[3] = { x0, y0, q.s0, q.t0, color.r, color.g, color.b, alpha };
                v[4] = { x1, y1, q.s1, q.t1, color.r, color.g, color.b, alpha };
                v[5] = { x0, y1, q.s0, q.t1, color.r, color.g, color.b, alpha };
                m_VertexCount += 6;
            }
        }
    }
}

void FontRenderer::Flush()
{
    if (!m_Initialized || m_VertexCount == 0)
        return;

    CapabilityGuard depthTest(GL_DEPTH_TEST, false);
    CapabilityGuard blend(GL_BLEND, true);
    BlendFuncGuard blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    m_VertexCount = 0;
}

float FontRenderer::GetTextWidth(float scale, const std::string& text) const
{
    if (!m_Initialized || text.empty())
        return 0.0f;

    float width = 0.0f;
    for (unsigned int codepoint : DecodeUTF8(text))
    {
        auto glyphIt = m_Glyphs.find(codepoint);
        if (glyphIt != m_Glyphs.end())
            width += glyphIt->second.xadvance * scale;
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
