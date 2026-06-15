#include "ecology_system.h"

#include <algorithm>
#include <cmath>

#include "terrain.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;
}

EcologySystem::EcologySystem()
    : m_SphereMesh(std::make_unique<SphereMesh>(1.0f, 20, 12)),
      m_RandomEngine(1337u)
{
}

EcologySystem::~EcologySystem()
{
    Clear();
}

void EcologySystem::Generate(const Terrain* terrain)
{
    Clear();
    if (!terrain || !terrain->IsInitialized())
        return;

    GenerateVegetation(terrain);
    GenerateBirds(terrain);
    GenerateGroundAnimals(terrain);
}

void EcologySystem::Update(float deltaTime, const Terrain* terrain)
{
    for (auto& bird : m_Birds)
    {
        bird.OrbitAngle += bird.OrbitSpeed * deltaTime;
        if (bird.OrbitAngle > kTwoPi)
            bird.OrbitAngle -= kTwoPi;

        bird.WingPhase += deltaTime * (7.0f + bird.Scale);
        const float orbitX = std::cos(bird.OrbitAngle) * bird.OrbitRadius;
        const float orbitZ = std::sin(bird.OrbitAngle) * bird.OrbitRadius * 0.7f;
        const float flutter = std::sin(bird.WingPhase * 1.4f) * 0.2f;
        bird.Position = bird.Center + glm::vec3(orbitX, bird.HeightOffset + flutter, orbitZ);
    }

    if (!terrain || !terrain->IsInitialized())
        return;

    for (auto& animal : m_GroundAnimals)
    {
        animal.StateTimer -= deltaTime;
        if (animal.StateTimer <= 0.0f)
        {
            const bool walk = RandomRange(0.0f, 1.0f) > 0.35f;
            animal.State = walk ? AnimalState::Walk : AnimalState::Idle;
            animal.StateTimer = walk ? RandomRange(2.0f, 4.5f) : RandomRange(1.0f, 2.4f);
            if (walk)
            {
                animal.Heading = RandomRange(0.0f, kTwoPi);
            }
        }

        if (animal.State == AnimalState::Walk)
        {
            animal.Heading += RandomRange(-0.8f, 0.8f) * deltaTime;
            const glm::vec2 dir(std::cos(animal.Heading), std::sin(animal.Heading));
            glm::vec3 candidate = animal.Position + glm::vec3(dir.x, 0.0f, dir.y) * animal.MoveSpeed * deltaTime;

            const glm::vec2 offset(candidate.x - animal.Anchor.x, candidate.z - animal.Anchor.z);
            if (glm::length(offset) > animal.RoamRadius)
            {
                animal.Heading += kPi * 0.75f;
                candidate = animal.Position;
            }

            const float candidateSlope = ComputeSlope(terrain, candidate.x, candidate.z);
            if (candidateSlope < 1.2f)
            {
                animal.Position.x = candidate.x;
                animal.Position.z = candidate.z;
            }
        }

        const float terrainHeight = terrain->GetHeightAt(animal.Position.x, animal.Position.z);
        animal.GaitPhase += deltaTime * (animal.State == AnimalState::Walk ? 6.0f : 1.5f);
        animal.Position.y = terrainHeight + 0.18f + std::sin(animal.GaitPhase) * (animal.State == AnimalState::Walk ? 0.03f : 0.01f);
    }
}

void EcologySystem::Render(const Camera& camera, Shader& shader, const glm::mat4& projection, const glm::mat4& view, const glm::vec3& ambientLight)
{
    if (!m_SphereMesh)
        return;

    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);
    shader.setVec3("viewPos", camera.Position);
    shader.setVec3("ambientLight", ambientLight);
    shader.setVec3("lightDir", glm::normalize(glm::vec3(-0.35f, 0.9f, 0.2f)));
    shader.setFloat("ambientStrength", 0.9f);
    shader.setFloat("diffuseStrength", 0.8f);
    shader.setFloat("specularStrength", 0.12f);
    shader.setFloat("emissionStrength", 0.0f);
    shader.setFloat("fresnelStrength", 0.08f);
    shader.setFloat("shininess", 24.0f);
    shader.setFloat("alpha", 1.0f);

    RenderVegetation(shader);
    RenderBirds(shader);
    RenderGroundAnimals(shader);
}

void EcologySystem::RenderDepth(Shader& shader) const
{
    if (!m_SphereMesh)
        return;

    RenderVegetation(shader);
    RenderGroundAnimals(shader);
}

void EcologySystem::Clear()
{
    m_Vegetation.clear();
    m_Birds.clear();
    m_GroundAnimals.clear();
}

void EcologySystem::GenerateVegetation(const Terrain* terrain)
{
    const float minX = terrain->GetMinX() + 2.0f;
    const float maxX = terrain->GetMaxX() - 2.0f;
    const float minZ = terrain->GetMinZ() + 2.0f;
    const float maxZ = terrain->GetMaxZ() - 2.0f;
    const float heightRange = std::max(terrain->GetMaxY() - terrain->GetMinY(), 1.0f);

    for (float z = minZ; z <= maxZ; z += 2.8f)
    {
        for (float x = minX; x <= maxX; x += 2.8f)
        {
            const float jitterX = RandomRange(-0.9f, 0.9f);
            const float jitterZ = RandomRange(-0.9f, 0.9f);
            const glm::vec3 position(x + jitterX, 0.0f, z + jitterZ);
            const float height = terrain->GetHeightAt(position.x, position.z);
            const float altitude = glm::clamp((height - terrain->GetMinY()) / heightRange, 0.0f, 1.0f);
            const float slope = glm::clamp(ComputeSlope(terrain, position.x, position.z), 0.0f, 1.0f);
            const float chance = RandomRange(0.0f, 1.0f);

            VegetationInstance instance{};
            instance.Position = glm::vec3(position.x, height, position.z);
            instance.Scale = 1.0f;
            instance.Variant = RandomRange(0.0f, 1.0f);

            bool accepted = false;
            if (altitude < 0.35f && slope < 0.45f)
            {
                if (chance < 0.28f)
                {
                    instance.Type = VegetationType::BroadleafTree;
                    instance.Scale = RandomRange(0.8f, 1.4f);
                    accepted = HasVegetationSpacing(instance.Position, 2.8f);
                }
                else if (chance < 0.72f)
                {
                    instance.Type = VegetationType::WildflowerPatch;
                    instance.Scale = RandomRange(0.7f, 1.25f);
                    accepted = HasVegetationSpacing(instance.Position, 1.15f);
                }
            }
            else if (altitude < 0.62f)
            {
                if (slope < 0.6f && chance < 0.2f)
                {
                    instance.Type = VegetationType::BroadleafTree;
                    instance.Scale = RandomRange(0.85f, 1.35f);
                    accepted = HasVegetationSpacing(instance.Position, 2.8f);
                }
                else if (chance < 0.58f)
                {
                    instance.Type = VegetationType::ConiferTree;
                    instance.Scale = RandomRange(0.8f, 1.25f);
                    accepted = HasVegetationSpacing(instance.Position, 2.4f);
                }
                else if (chance < 0.82f)
                {
                    instance.Type = VegetationType::WildflowerPatch;
                    instance.Scale = RandomRange(0.6f, 1.1f);
                    accepted = HasVegetationSpacing(instance.Position, 1.0f);
                }
            }
            else if (chance < 0.5f)
            {
                instance.Type = VegetationType::ConiferTree;
                instance.Scale = RandomRange(0.75f, 1.2f);
                accepted = HasVegetationSpacing(instance.Position, 2.3f);
            }
            else if (chance < 0.88f)
            {
                instance.Type = VegetationType::AlpineMeadow;
                instance.Scale = RandomRange(0.7f, 1.15f);
                accepted = HasVegetationSpacing(instance.Position, 1.1f);
            }

            if (accepted)
            {
                m_Vegetation.push_back(instance);
            }
        }
    }
}

void EcologySystem::GenerateBirds(const Terrain* terrain)
{
    for (int i = 0; i < 6; ++i)
    {
        BirdAgent bird{};
        const glm::vec3 anchor = RandomPointOnTerrain(terrain, 8.0f);
        bird.Center = glm::vec3(anchor.x, anchor.y + RandomRange(8.0f, 15.0f), anchor.z);
        bird.Position = bird.Center;
        bird.Color = glm::mix(glm::vec3(0.18f, 0.16f, 0.12f), glm::vec3(0.85f, 0.85f, 0.82f), RandomRange(0.0f, 1.0f));
        bird.OrbitRadius = RandomRange(3.0f, 7.0f);
        bird.OrbitAngle = RandomRange(0.0f, kTwoPi);
        bird.OrbitSpeed = RandomRange(0.35f, 0.8f);
        bird.HeightOffset = RandomRange(-1.0f, 1.2f);
        bird.WingPhase = RandomRange(0.0f, kTwoPi);
        bird.Scale = RandomRange(0.18f, 0.3f);
        m_Birds.push_back(bird);
    }
}

void EcologySystem::GenerateGroundAnimals(const Terrain* terrain)
{
    for (int i = 0; i < 5; ++i)
    {
        GroundAnimal animal{};
        animal.Anchor = RandomPointOnTerrain(terrain, 6.0f);
        animal.Position = animal.Anchor;
        animal.Position.y += 0.18f;
        animal.Color = glm::mix(glm::vec3(0.36f, 0.24f, 0.16f), glm::vec3(0.7f, 0.6f, 0.45f), RandomRange(0.0f, 1.0f));
        animal.Heading = RandomRange(0.0f, kTwoPi);
        animal.MoveSpeed = RandomRange(0.6f, 1.2f);
        animal.RoamRadius = RandomRange(3.5f, 7.0f);
        animal.StateTimer = RandomRange(0.8f, 2.5f);
        animal.GaitPhase = RandomRange(0.0f, kTwoPi);
        animal.Scale = RandomRange(0.35f, 0.55f);
        animal.State = (RandomRange(0.0f, 1.0f) > 0.5f) ? AnimalState::Walk : AnimalState::Idle;
        m_GroundAnimals.push_back(animal);
    }
}

float EcologySystem::ComputeSlope(const Terrain* terrain, float x, float z) const
{
    const float sampleOffset = 0.8f;
    const float hL = terrain->GetHeightAt(x - sampleOffset, z);
    const float hR = terrain->GetHeightAt(x + sampleOffset, z);
    const float hD = terrain->GetHeightAt(x, z - sampleOffset);
    const float hU = terrain->GetHeightAt(x, z + sampleOffset);
    const float dx = (hR - hL) / (sampleOffset * 2.0f);
    const float dz = (hU - hD) / (sampleOffset * 2.0f);
    return std::sqrt(dx * dx + dz * dz);
}

bool EcologySystem::HasVegetationSpacing(const glm::vec3& position, float minDistance) const
{
    const float minDistanceSq = minDistance * minDistance;
    for (const auto& vegetation : m_Vegetation)
    {
        const glm::vec2 delta(position.x - vegetation.Position.x, position.z - vegetation.Position.z);
        if (glm::dot(delta, delta) < minDistanceSq)
            return false;
    }
    return true;
}

glm::vec3 EcologySystem::RandomPointOnTerrain(const Terrain* terrain, float border) const
{
    EcologySystem* self = const_cast<EcologySystem*>(this);
    glm::vec3 point(0.0f);
    point.x = self->RandomRange(terrain->GetMinX() + border, terrain->GetMaxX() - border);
    point.z = self->RandomRange(terrain->GetMinZ() + border, terrain->GetMaxZ() - border);
    point.y = terrain->GetHeightAt(point.x, point.z);
    return point;
}

float EcologySystem::RandomRange(float minValue, float maxValue)
{
    return std::uniform_real_distribution<float>(minValue, maxValue)(m_RandomEngine);
}

void EcologySystem::RenderVegetation(Shader& shader) const
{
    for (const auto& vegetation : m_Vegetation)
    {
        const glm::vec3 base = vegetation.Position;
        switch (vegetation.Type)
        {
        case VegetationType::BroadleafTree:
        {
            const float trunkHeight = 1.5f * vegetation.Scale;
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight * 0.45f, 0.0f), glm::vec3(0.12f, trunkHeight, 0.12f), glm::vec3(0.42f, 0.28f, 0.16f));
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight + 0.5f * vegetation.Scale, 0.0f), glm::vec3(0.75f, 0.7f, 0.75f) * vegetation.Scale, glm::vec3(0.18f, 0.48f, 0.2f));
            RenderSpherePart(shader, base + glm::vec3(-0.35f, trunkHeight + 0.25f * vegetation.Scale, 0.2f), glm::vec3(0.42f, 0.38f, 0.42f) * vegetation.Scale, glm::vec3(0.22f, 0.56f, 0.24f));
            RenderSpherePart(shader, base + glm::vec3(0.32f, trunkHeight + 0.18f * vegetation.Scale, -0.15f), glm::vec3(0.4f, 0.36f, 0.4f) * vegetation.Scale, glm::vec3(0.2f, 0.52f, 0.22f));
            break;
        }
        case VegetationType::WildflowerPatch:
        {
            const glm::vec3 leafColor(0.23f, 0.58f, 0.24f);
            const glm::vec3 bloomA(0.86f, 0.24f, 0.4f);
            const glm::vec3 bloomB(0.92f, 0.82f, 0.24f);
            RenderSpherePart(shader, base + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.4f, 0.08f, 0.4f) * vegetation.Scale, leafColor);
            RenderSpherePart(shader, base + glm::vec3(0.18f, 0.15f, 0.12f), glm::vec3(0.07f, 0.22f, 0.07f) * vegetation.Scale, leafColor);
            RenderSpherePart(shader, base + glm::vec3(0.18f, 0.32f, 0.12f), glm::vec3(0.08f) * vegetation.Scale, bloomA);
            RenderSpherePart(shader, base + glm::vec3(-0.14f, 0.13f, -0.1f), glm::vec3(0.06f, 0.2f, 0.06f) * vegetation.Scale, leafColor);
            RenderSpherePart(shader, base + glm::vec3(-0.14f, 0.29f, -0.1f), glm::vec3(0.075f) * vegetation.Scale, bloomB);
            break;
        }
        case VegetationType::ConiferTree:
        {
            const float trunkHeight = 1.3f * vegetation.Scale;
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight * 0.4f, 0.0f), glm::vec3(0.11f, trunkHeight, 0.11f), glm::vec3(0.38f, 0.26f, 0.15f));
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight + 0.3f, 0.0f), glm::vec3(0.55f, 0.45f, 0.55f) * vegetation.Scale, glm::vec3(0.12f, 0.36f, 0.18f));
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight + 0.78f, 0.0f), glm::vec3(0.42f, 0.36f, 0.42f) * vegetation.Scale, glm::vec3(0.1f, 0.34f, 0.17f));
            RenderSpherePart(shader, base + glm::vec3(0.0f, trunkHeight + 1.1f, 0.0f), glm::vec3(0.28f, 0.28f, 0.28f) * vegetation.Scale, glm::vec3(0.09f, 0.3f, 0.16f));
            break;
        }
        case VegetationType::AlpineMeadow:
        {
            RenderSpherePart(shader, base + glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(0.55f, 0.07f, 0.55f) * vegetation.Scale, glm::vec3(0.34f, 0.58f, 0.26f));
            RenderSpherePart(shader, base + glm::vec3(0.18f, 0.08f, 0.1f), glm::vec3(0.24f, 0.06f, 0.24f) * vegetation.Scale, glm::vec3(0.42f, 0.64f, 0.28f));
            RenderSpherePart(shader, base + glm::vec3(-0.16f, 0.07f, -0.08f), glm::vec3(0.22f, 0.05f, 0.22f) * vegetation.Scale, glm::vec3(0.39f, 0.61f, 0.27f));
            break;
        }
        }
    }
}

void EcologySystem::RenderBirds(Shader& shader) const
{
    for (const auto& bird : m_Birds)
    {
        const float wingLift = std::sin(bird.WingPhase) * 55.0f;
        const float yaw = glm::degrees(std::atan2(std::cos(bird.OrbitAngle), -std::sin(bird.OrbitAngle)));
        const glm::vec3 bodyPos = bird.Position;
        RenderSpherePart(shader, bodyPos, glm::vec3(0.22f, 0.12f, 0.34f) * bird.Scale, bird.Color, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(0.0f, 0.04f, 0.16f) * bird.Scale, glm::vec3(0.12f) * bird.Scale, bird.Color * 1.05f, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(-0.18f, 0.02f, 0.02f) * bird.Scale, glm::vec3(0.32f, 0.05f, 0.14f) * bird.Scale, bird.Color * 0.95f, yaw + wingLift);
        RenderSpherePart(shader, bodyPos + glm::vec3(0.18f, 0.02f, 0.02f) * bird.Scale, glm::vec3(0.32f, 0.05f, 0.14f) * bird.Scale, bird.Color * 0.95f, yaw - wingLift);
    }
}

void EcologySystem::RenderGroundAnimals(Shader& shader) const
{
    for (const auto& animal : m_GroundAnimals)
    {
        const float yaw = glm::degrees(animal.Heading);
        const float legSwing = std::sin(animal.GaitPhase) * (animal.State == AnimalState::Walk ? 0.12f : 0.03f);
        const glm::vec3 bodyPos = animal.Position + glm::vec3(0.0f, 0.18f * animal.Scale, 0.0f);

        RenderSpherePart(shader, bodyPos, glm::vec3(0.42f, 0.24f, 0.24f) * animal.Scale, animal.Color, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(0.0f, 0.08f, 0.25f) * animal.Scale, glm::vec3(0.16f, 0.14f, 0.14f) * animal.Scale, animal.Color * 1.04f, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(-0.14f, -0.12f + legSwing, 0.1f) * animal.Scale, glm::vec3(0.07f, 0.18f, 0.07f) * animal.Scale, animal.Color * 0.85f, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(0.14f, -0.12f - legSwing, 0.1f) * animal.Scale, glm::vec3(0.07f, 0.18f, 0.07f) * animal.Scale, animal.Color * 0.85f, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(-0.14f, -0.12f - legSwing, -0.08f) * animal.Scale, glm::vec3(0.07f, 0.18f, 0.07f) * animal.Scale, animal.Color * 0.85f, yaw);
        RenderSpherePart(shader, bodyPos + glm::vec3(0.14f, -0.12f + legSwing, -0.08f) * animal.Scale, glm::vec3(0.07f, 0.18f, 0.07f) * animal.Scale, animal.Color * 0.85f, yaw);
    }
}

void EcologySystem::RenderSpherePart(Shader& shader, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& color, float yawDegrees) const
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    shader.setMat4("model", model);
    shader.setVec3("sphereColor", color);
    m_SphereMesh->Draw(shader);
}
