#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

#include "shader.h"
#include <stb_truetype.h>

class FontRenderer
{
public:
    FontRenderer();
    ~FontRenderer();

    bool Initialize(int screenWidth, int screenHeight, const std::string& fontPath = "");
    void Cleanup();
    void SetScreenSize(int screenWidth, int screenHeight);

    void DrawText(float x, float y, float scale, const std::string& text,
                  const glm::vec3& color, float alpha = 1.0f);
    void Flush();

    float GetTextWidth(float scale, const std::string& text) const;
    float GetCharHeight(float scale) const;
    float GetLineHeight(float scale) const;

    glm::mat4 GetProjection() const { return m_Projection; }

private:
    static const int ATLAS_W = 512;
    static const int ATLAS_H = 512;
    static const int FIRST_CHAR = 32;
    static const int CHAR_COUNT = 96;

    stbtt_bakedchar m_CData[CHAR_COUNT];

    unsigned int m_AtlasTexture;
    unsigned int m_VAO;
    unsigned int m_VBO;
    Shader* m_Shader;

    int m_ScreenWidth;
    int m_ScreenHeight;
    glm::mat4 m_Projection;

    struct TextVertex
    {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    static const int MAX_VERTICES = 16384;
    TextVertex m_VertexBuffer[MAX_VERTICES];
    int m_VertexCount;
    bool m_Initialized;
};

#endif
