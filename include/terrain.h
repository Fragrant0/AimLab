#ifndef TERRAIN_H
#define TERRAIN_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct TerrainVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class Terrain
{
public:
    Terrain();
    ~Terrain();

    bool GenerateFromHeightmap(const std::string& heightmapPath, float heightScale,
                                int gridSize, float size);

    void GenerateFlat(float size);

    void Cleanup();

    void Render();

    float GetHeightAt(float worldX, float worldZ) const;

    float GetMinX() const { return -m_Size * 0.5f; }
    float GetMaxX() const { return m_Size * 0.5f; }
    float GetMinZ() const { return -m_Size * 0.5f; }
    float GetMaxZ() const { return m_Size * 0.5f; }
    float GetMinY() const { return m_MinHeight; }
    float GetMaxY() const { return m_MaxHeight; }

    bool IsInitialized() const { return m_Initialized; }

private:
    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
    int m_IndexCount;

    std::vector<TerrainVertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    float m_Size;
    float m_HeightScale;
    int m_GridSize;
    float m_MinHeight;
    float m_MaxHeight;
    bool m_Initialized;

    std::vector<float> m_HeightData;
    int m_HeightmapWidth;
    int m_HeightmapHeight;

    float SampleHeightmap(float u, float v) const;
    float SampleHeightmapBilinear(float u, float v) const;
    float GetHeightmapValue(int x, int y) const;
    static float CubicInterpolate(float p0, float p1, float p2, float p3, float t);

    void BuildMesh();
    void CalculateNormals();
    void UploadToGPU();
};

#endif