#include "sphere_mesh.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SphereMesh::SphereMesh(float radius, unsigned int sectorCount, unsigned int stackCount)
    : m_Radius(radius),
      m_SectorCount(sectorCount),
      m_StackCount(stackCount),
      m_VAO(0),
      m_VBO(0),
      m_EBO(0)
{
    GenerateMesh();
    SetupBuffers();
}

SphereMesh::~SphereMesh()
{
    if (m_VAO != 0)
        glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO != 0)
        glDeleteBuffers(1, &m_VBO);
    if (m_EBO != 0)
        glDeleteBuffers(1, &m_EBO);
}

void SphereMesh::GenerateMesh()
{
    m_Vertices.clear();
    m_Indices.clear();

    float x, y, z, xy;
    float nx, ny, nz, lengthInv = 1.0f / m_Radius;
    float s, t;

    float sectorStep = 2.0f * static_cast<float>(M_PI) / m_SectorCount;
    float stackStep = static_cast<float>(M_PI) / m_StackCount;
    float sectorAngle, stackAngle;

    for (unsigned int i = 0; i <= m_StackCount; ++i)
    {
        stackAngle = static_cast<float>(M_PI) / 2.0f - i * stackStep;
        xy = m_Radius * cosf(stackAngle);
        z = m_Radius * sinf(stackAngle);

        for (unsigned int j = 0; j <= m_SectorCount; ++j)
        {
            sectorAngle = j * sectorStep;

            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);

            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;

            s = static_cast<float>(j) / m_SectorCount;
            t = static_cast<float>(i) / m_StackCount;

            Vertex vertex;
            vertex.Position = glm::vec3(x, y, z);
            vertex.Normal = glm::vec3(nx, ny, nz);
            vertex.TexCoords = glm::vec2(s, t);
            vertex.Tangent = glm::vec3(0.0f);
            vertex.Bitangent = glm::vec3(0.0f);
            for (int k = 0; k < MAX_BONE_INFLUENCE; ++k)
            {
                vertex.m_BoneIDs[k] = 0;
                vertex.m_Weights[k] = 0.0f;
            }

            m_Vertices.push_back(vertex);
        }
    }

    unsigned int k1, k2;
    for (unsigned int i = 0; i < m_StackCount; ++i)
    {
        k1 = i * (m_SectorCount + 1);
        k2 = k1 + m_SectorCount + 1;

        for (unsigned int j = 0; j < m_SectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                m_Indices.push_back(k1);
                m_Indices.push_back(k2);
                m_Indices.push_back(k1 + 1);
            }

            if (i != (m_StackCount - 1))
            {
                m_Indices.push_back(k1 + 1);
                m_Indices.push_back(k2);
                m_Indices.push_back(k2 + 1);
            }
        }
    }
}

void SphereMesh::SetupBuffers()
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), &m_Vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int), &m_Indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

    glBindVertexArray(0);
}

void SphereMesh::Draw(Shader& shader)
{
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
