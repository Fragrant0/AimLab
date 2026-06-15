#include "terrain.h"
#include "stb_image.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>

Terrain::Terrain()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_IndexCount(0),
      m_Size(20.0f), m_HeightScale(1.0f), m_GridSize(100),
      m_MinHeight(0.0f), m_MaxHeight(0.0f), m_Initialized(false),
      m_HeightmapWidth(0), m_HeightmapHeight(0)
{
}

Terrain::~Terrain()
{
    Cleanup();
}

bool Terrain::GenerateFromHeightmap(const std::string& heightmapPath, float heightScale,
                                     int gridSize, float size)
{
    Cleanup();

    m_HeightScale = heightScale;
    m_GridSize = gridSize;
    m_Size = size;

   
    int width = 0;
    int height = 0;
    int channels = 0;
    const bool is16BitHeightmap = stbi_is_16_bit(heightmapPath.c_str()) != 0;
    if (is16BitHeightmap)
    {
        stbi_us* data16 = stbi_load_16(heightmapPath.c_str(), &width, &height, &channels, 1);
        if (!data16)
        {
            std::cout << "Failed to load 16-bit heightmap: " << heightmapPath << std::endl;
            return false;
        }

        m_HeightData.resize(width * height);
        constexpr float kInv16BitRange = 1.0f / 65535.0f;
        for (int i = 0; i < width * height; ++i)
        {
            m_HeightData[i] = static_cast<float>(data16[i]) * kInv16BitRange;
        }
        stbi_image_free(data16);
    }
    else
    {
        unsigned char* data8 = stbi_load(heightmapPath.c_str(), &width, &height, &channels, 1);
        if (!data8)
        {
            std::cout << "Failed to load heightmap: " << heightmapPath << std::endl;
            return false;
        }

        m_HeightData.resize(width * height);
        constexpr float kInv8BitRange = 1.0f / 255.0f;
        for (int i = 0; i < width * height; ++i)
        {
            m_HeightData[i] = static_cast<float>(data8[i]) * kInv8BitRange;
        }
        stbi_image_free(data8);
    }

    m_HeightmapWidth = width;
    m_HeightmapHeight = height;

   
    m_Vertices.clear();
    m_Vertices.reserve((gridSize + 1) * (gridSize + 1));

    float halfSize = size * 0.5f;
    float stepX = size / static_cast<float>(gridSize);
    float stepZ = size / static_cast<float>(gridSize);

    m_MinHeight = FLT_MAX;
    m_MaxHeight = -FLT_MAX;

    for (int z = 0; z <= gridSize; ++z)
    {
        for (int x = 0; x <= gridSize; ++x)
        {
            TerrainVertex vertex;

           
            float worldX = -halfSize + x * stepX;
            float worldZ = -halfSize + z * stepZ;

            
            float u = static_cast<float>(x) / static_cast<float>(gridSize);
            float v = static_cast<float>(z) / static_cast<float>(gridSize);
            float heightValue = SampleHeightmap(u, v);
            float worldY = heightValue * heightScale;

            vertex.Position = glm::vec3(worldX, worldY, worldZ);
            vertex.TexCoords = glm::vec2(
                static_cast<float>(x) / gridSize,
                static_cast<float>(z) / gridSize
            );
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f); // ??????????????

            m_Vertices.push_back(vertex);

            m_MinHeight = std::min(m_MinHeight, worldY);
            m_MaxHeight = std::max(m_MaxHeight, worldY);
        }
    }

    BuildMesh();
    CalculateNormals();
    UploadToGPU();

    m_Initialized = true;
    std::cout << "Terrain generated: " << gridSize << "x" << gridSize 
              << " vertices, height range [" << m_MinHeight << ", " << m_MaxHeight << "]" << std::endl;
    return true;
}

void Terrain::GenerateFlat(float size)
{
    Cleanup();

    m_Size = size;
    m_HeightScale = 0.0f;
    m_GridSize = 1;
    m_MinHeight = 0.0f;
    m_MaxHeight = 0.0f;

    
    float halfSize = size * 0.5f;

    m_Vertices.clear();
    m_Vertices.reserve(4);


    m_Vertices.push_back({ glm::vec3(-halfSize, 0.0f, -halfSize), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) });
   
    m_Vertices.push_back({ glm::vec3(halfSize, 0.0f, -halfSize), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) });
    
    m_Vertices.push_back({ glm::vec3(-halfSize, 0.0f, halfSize), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) });
    
    m_Vertices.push_back({ glm::vec3(halfSize, 0.0f, halfSize), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) });

    m_Indices = { 0, 1, 2, 1, 3, 2 };
    m_IndexCount = 6;

    UploadToGPU();

    m_Initialized = true;
}

void Terrain::Cleanup()
{
    if (m_VAO != 0)
    {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO != 0)
    {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_EBO != 0)
    {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }

    m_Vertices.clear();
    m_Indices.clear();
    m_HeightData.clear();
    m_IndexCount = 0;
    m_Initialized = false;
}

void Terrain::Render()
{
    if (!m_Initialized || m_VAO == 0)
        return;

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

float Terrain::GetHeightAt(float worldX, float worldZ) const
{
    if (!m_Initialized)
        return 0.0f;

    if (m_HeightData.empty())
        return 0.0f;

    
    float halfSize = m_Size * 0.5f;
    float u = (worldX + halfSize) / m_Size;
    float v = (worldZ + halfSize) / m_Size;

    
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));

    return SampleHeightmap(u, v) * m_HeightScale;
}

float Terrain::SampleHeightmap(float u, float v) const
{
    if (m_HeightData.empty())
        return 0.0f;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    if (m_HeightmapWidth < 4 || m_HeightmapHeight < 4)
    {
        return SampleHeightmapBilinear(u, v);
    }

    const float px = u * static_cast<float>(m_HeightmapWidth - 1);
    const float py = v * static_cast<float>(m_HeightmapHeight - 1);

    const int x1 = static_cast<int>(std::floor(px));
    const int y1 = static_cast<int>(std::floor(py));
    const float tx = px - static_cast<float>(x1);
    const float ty = py - static_cast<float>(y1);

    float samples[4];
    for (int row = -1; row <= 2; ++row)
    {
        const int sampleY = y1 + row;
        const float p0 = GetHeightmapValue(x1 - 1, sampleY);
        const float p1 = GetHeightmapValue(x1, sampleY);
        const float p2 = GetHeightmapValue(x1 + 1, sampleY);
        const float p3 = GetHeightmapValue(x1 + 2, sampleY);
        samples[row + 1] = CubicInterpolate(p0, p1, p2, p3, tx);
    }

    return std::clamp(CubicInterpolate(samples[0], samples[1], samples[2], samples[3], ty), 0.0f, 1.0f);
}

float Terrain::SampleHeightmapBilinear(float u, float v) const
{
    if (m_HeightData.empty())
        return 0.0f;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    const float px = u * static_cast<float>(m_HeightmapWidth - 1);
    const float py = v * static_cast<float>(m_HeightmapHeight - 1);

    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const int x1 = std::min(x0 + 1, m_HeightmapWidth - 1);
    const int y1 = std::min(y0 + 1, m_HeightmapHeight - 1);

    const float fx = px - static_cast<float>(x0);
    const float fy = py - static_cast<float>(y0);

    const float h00 = m_HeightData[y0 * m_HeightmapWidth + x0];
    const float h10 = m_HeightData[y0 * m_HeightmapWidth + x1];
    const float h01 = m_HeightData[y1 * m_HeightmapWidth + x0];
    const float h11 = m_HeightData[y1 * m_HeightmapWidth + x1];

    const float h0 = h00 * (1.0f - fx) + h10 * fx;
    const float h1 = h01 * (1.0f - fx) + h11 * fx;
    return h0 * (1.0f - fy) + h1 * fy;
}

float Terrain::GetHeightmapValue(int x, int y) const
{
    if (m_HeightData.empty())
        return 0.0f;

    x = std::clamp(x, 0, m_HeightmapWidth - 1);
    y = std::clamp(y, 0, m_HeightmapHeight - 1);
    return m_HeightData[y * m_HeightmapWidth + x];
}

float Terrain::CubicInterpolate(float p0, float p1, float p2, float p3, float t)
{
    const float a0 = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    const float a1 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    const float a2 = -0.5f * p0 + 0.5f * p2;
    const float a3 = p1;
    return ((a0 * t + a1) * t + a2) * t + a3;
}

void Terrain::BuildMesh()
{
    m_Indices.clear();
    m_Indices.reserve(m_GridSize * m_GridSize * 6);

    for (int z = 0; z < m_GridSize; ++z)
    {
        for (int x = 0; x < m_GridSize; ++x)
        {
            int topLeft = z * (m_GridSize + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (m_GridSize + 1) + x;
            int bottomRight = bottomLeft + 1;

            
            m_Indices.push_back(topLeft);
            m_Indices.push_back(bottomLeft);
            m_Indices.push_back(topRight);

            
            m_Indices.push_back(topRight);
            m_Indices.push_back(bottomLeft);
            m_Indices.push_back(bottomRight);
        }
    }

    m_IndexCount = static_cast<int>(m_Indices.size());
}

void Terrain::CalculateNormals()
{
    for (auto& vertex : m_Vertices)
    {
        vertex.Normal = glm::vec3(0.0f);
    }

    
    for (size_t i = 0; i < m_Indices.size(); i += 3)
    {
        unsigned int i0 = m_Indices[i];
        unsigned int i1 = m_Indices[i + 1];
        unsigned int i2 = m_Indices[i + 2];

        glm::vec3 v0 = m_Vertices[i0].Position;
        glm::vec3 v1 = m_Vertices[i1].Position;
        glm::vec3 v2 = m_Vertices[i2].Position;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

        m_Vertices[i0].Normal += faceNormal;
        m_Vertices[i1].Normal += faceNormal;
        m_Vertices[i2].Normal += faceNormal;
    }

 
    for (auto& vertex : m_Vertices)
    {
        vertex.Normal = glm::normalize(vertex.Normal);
    }
}

void Terrain::UploadToGPU()
{
    if (m_VAO != 0)
    {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
    }

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);


    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(TerrainVertex), m_Vertices.data(), GL_STATIC_DRAW);


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int), m_Indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Position));


    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Normal));


    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, TexCoords));

    glBindVertexArray(0);
}
