#ifndef SPHERE_MESH_H
#define SPHERE_MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include "mesh.h"

class SphereMesh
{
public:
    SphereMesh(float radius = 1.0f, unsigned int sectorCount = 36, unsigned int stackCount = 18);
    ~SphereMesh();

    void Draw(Shader& shader);

    float GetRadius() const { return m_Radius; }
    unsigned int GetSectorCount() const { return m_SectorCount; }
    unsigned int GetStackCount() const { return m_StackCount; }
    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetVBO() const { return m_VBO; }
    unsigned int GetEBO() const { return m_EBO; }
    const std::vector<unsigned int>& GetIndices() const { return m_Indices; }

private:
    void GenerateMesh();
    void SetupBuffers();

    float m_Radius;
    unsigned int m_SectorCount;
    unsigned int m_StackCount;

    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
};

#endif
