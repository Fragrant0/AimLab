#ifndef ECOLOGY_SYSTEM_H
#define ECOLOGY_SYSTEM_H

#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <vector>

#include "camera.h"
#include "shader.h"
#include "sphere_mesh.h"

class Terrain;

class EcologySystem
{
public:
    EcologySystem();
    ~EcologySystem();

    void Generate(const Terrain* terrain, const std::string& mapName);
    void Update(float deltaTime, const Terrain* terrain);
    void Render(const Camera& camera, Shader& shader, const glm::mat4& projection, const glm::mat4& view, const glm::vec3& ambientLight);
    void RenderDepth(Shader& shader) const;

    bool IsReady() const { return !m_Vegetation.empty() || !m_Birds.empty() || !m_GroundAnimals.empty(); }

private:
    enum class VegetationType
    {
        BroadleafTree,
        WildflowerPatch,
        ConiferTree,
        AlpineMeadow
    };

    enum class AnimalState
    {
        Idle,
        Walk
    };

    struct VegetationInstance
    {
        VegetationType Type;
        glm::vec3 Position;
        float Scale;
        float Variant;
    };

    struct BirdAgent
    {
        glm::vec3 Center;
        glm::vec3 Position;
        glm::vec3 Color;
        float OrbitRadius;
        float OrbitAngle;
        float OrbitSpeed;
        float HeightOffset;
        float WingPhase;
        float Scale;
    };

    struct GroundAnimal
    {
        glm::vec3 Anchor;
        glm::vec3 Position;
        glm::vec3 Color;
        float Heading;
        float MoveSpeed;
        float RoamRadius;
        float StateTimer;
        float GaitPhase;
        float Scale;
        AnimalState State;
    };

    struct SphereInstance
    {
        glm::mat4 model;
        glm::vec3 color;
    };

    void Clear();
    void GenerateVegetation(const Terrain* terrain);
    void GenerateBirds(const Terrain* terrain);
    void GenerateGroundAnimals(const Terrain* terrain);

    float ComputeSlope(const Terrain* terrain, float x, float z) const;
    bool HasVegetationSpacing(const glm::vec3& position, float minDistance) const;
    glm::vec3 RandomPointOnTerrain(const Terrain* terrain, float border) const;
    float RandomRange(float minValue, float maxValue);

    void CollectVegetationInstances();
    void SetupInstancingBuffers();
    void UpdateDynamicInstances();
    void AddSphereInstance(std::vector<SphereInstance>& list, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& color, float yawDegrees = 0.0f) const;
    void SetupVAO(unsigned int& vao, unsigned int& vbo, const std::vector<SphereInstance>& instances, bool isStatic);

    void RenderVegetation(Shader& shader) const;
    void RenderBirds(Shader& shader) const;
    void RenderGroundAnimals(Shader& shader) const;

    std::vector<VegetationInstance> m_Vegetation;
    std::vector<BirdAgent> m_Birds;
    std::vector<GroundAnimal> m_GroundAnimals;

    std::vector<SphereInstance> m_VegInstances;
    std::vector<SphereInstance> m_BirdInstances;
    std::vector<SphereInstance> m_AnimalInstances;

    unsigned int m_VegVAO;
    unsigned int m_VegVBO;
    unsigned int m_BirdVAO;
    unsigned int m_BirdVBO;
    unsigned int m_AnimalVAO;
    unsigned int m_AnimalVBO;

    std::unique_ptr<SphereMesh> m_SphereMesh;
    std::mt19937 m_RandomEngine;
    std::string m_MapName;
};

#endif
