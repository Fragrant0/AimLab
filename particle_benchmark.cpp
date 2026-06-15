#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

// Minimal standalone benchmark to measure particle system performance

struct Particle {
    float position[3];
    float velocity[3];
    float life;
    float maxLife;
    float color[4];
    float size;
    bool active;
    int type;
};

constexpr int MAX_PARTICLES = 2000;
constexpr float GRAVITY = -9.8f;
constexpr float DRAG = 0.97f;

// Old Update logic (with type branch)
void UpdateOld(std::vector<Particle>& particles, float deltaTime) {
    float dragFactor = std::pow(DRAG, deltaTime * 60.0f);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = particles[i];
        if (!p.active) continue;

        if (p.type == 1) {
            p.position[0] += p.velocity[0] * deltaTime;
            p.position[1] += p.velocity[1] * deltaTime;
            p.position[2] += p.velocity[2] * deltaTime;
            p.size += deltaTime * 4.0f;
        } else {
            p.velocity[1] += GRAVITY * deltaTime;
            p.velocity[0] *= dragFactor;
            p.velocity[1] *= dragFactor;
            p.velocity[2] *= dragFactor;
            p.position[0] += p.velocity[0] * deltaTime;
            p.position[1] += p.velocity[1] * deltaTime;
            p.position[2] += p.velocity[2] * deltaTime;
        }

        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.active = false;
        }
    }
}

// New Update logic (no type branch, single code path)
void UpdateNew(std::vector<Particle>& particles, float deltaTime) {
    float dragFactor = std::pow(DRAG, deltaTime * 60.0f);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = particles[i];
        if (!p.active) continue;

        p.velocity[1] += GRAVITY * deltaTime;
        p.velocity[0] *= dragFactor;
        p.velocity[1] *= dragFactor;
        p.velocity[2] *= dragFactor;
        p.position[0] += p.velocity[0] * deltaTime;
        p.position[1] += p.velocity[1] * deltaTime;
        p.position[2] += p.velocity[2] * deltaTime;

        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.active = false;
        }
    }
}

// Old instance data update (with type branch)
void UpdateInstanceDataOld(const std::vector<Particle>& particles, std::vector<float>& instanceData) {
    instanceData.clear();
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle& p = particles[i];
        if (!p.active) continue;

        float lifeRatio = p.life / p.maxLife;
        if (lifeRatio < 0.0f) lifeRatio = 0.0f;

        float alphaCurve = lifeRatio * lifeRatio;
        float renderSize = p.size;

        if (p.type == 1) {
            float a = alphaCurve * 0.7f;
            instanceData.push_back(p.position[0]);
            instanceData.push_back(p.position[1]);
            instanceData.push_back(p.position[2]);
            instanceData.push_back(p.color[0]);
            instanceData.push_back(p.color[1]);
            instanceData.push_back(p.color[2]);
            instanceData.push_back(a);
            instanceData.push_back(renderSize);
        } else {
            float sizeCurve = std::sqrt(lifeRatio);
            renderSize = p.size * (0.3f + 0.7f * sizeCurve);
            instanceData.push_back(p.position[0]);
            instanceData.push_back(p.position[1]);
            instanceData.push_back(p.position[2]);
            instanceData.push_back(p.color[0]);
            instanceData.push_back(p.color[1]);
            instanceData.push_back(p.color[2]);
            instanceData.push_back(alphaCurve);
            instanceData.push_back(renderSize);
        }
    }
}

// New instance data update (no type branch)
void UpdateInstanceDataNew(const std::vector<Particle>& particles, std::vector<float>& instanceData) {
    instanceData.clear();
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle& p = particles[i];
        if (!p.active) continue;

        float lifeRatio = p.life / p.maxLife;
        if (lifeRatio < 0.0f) lifeRatio = 0.0f;
        if (lifeRatio > 1.0f) lifeRatio = 1.0f;

        float alphaCurve = lifeRatio * lifeRatio;
        float sizeCurve = 0.3f + 0.7f * std::sqrt(lifeRatio);
        float renderSize = p.size * sizeCurve;

        instanceData.push_back(p.position[0]);
        instanceData.push_back(p.position[1]);
        instanceData.push_back(p.position[2]);
        instanceData.push_back(p.color[0]);
        instanceData.push_back(p.color[1]);
        instanceData.push_back(p.color[2]);
        instanceData.push_back(alphaCurve);
        instanceData.push_back(renderSize);
    }
}

void InitParticles(std::vector<Particle>& particles, int activeCount) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> lifeDist(0.4f, 1.2f);

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].active = i < activeCount;
        particles[i].type = (i % 10 == 0) ? 1 : 0;
        particles[i].position[0] = dist(rng);
        particles[i].position[1] = dist(rng);
        particles[i].position[2] = dist(rng);
        particles[i].velocity[0] = dist(rng) * 5.0f;
        particles[i].velocity[1] = dist(rng) * 5.0f;
        particles[i].velocity[2] = dist(rng) * 5.0f;
        particles[i].life = lifeDist(rng);
        particles[i].maxLife = particles[i].life;
        particles[i].color[0] = 1.0f;
        particles[i].color[1] = 0.8f;
        particles[i].color[2] = 0.2f;
        particles[i].color[3] = 1.0f;
        particles[i].size = 0.3f;
    }
}

template<typename Func>
double Benchmark(Func&& func, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    const int iterations = 100000;
    const int activeCounts[] = {500, 1000, 1500, 2000};

    std::cout << "=== Particle System Performance Benchmark ===" << std::endl;
    std::cout << "Iterations per test: " << iterations << std::endl;
    std::cout << std::endl;

    for (int activeCount : activeCounts) {
        std::vector<Particle> particlesOld(MAX_PARTICLES);
        std::vector<Particle> particlesNew(MAX_PARTICLES);
        std::vector<float> instanceData;

        InitParticles(particlesOld, activeCount);
        InitParticles(particlesNew, activeCount);

        std::cout << "--- Active Particles: " << activeCount << " ---" << std::endl;

        // Update benchmark
        double oldUpdateTime = Benchmark([&]() {
            UpdateOld(particlesOld, 0.016f);
        }, iterations);

        InitParticles(particlesOld, activeCount);

        double newUpdateTime = Benchmark([&]() {
            UpdateNew(particlesNew, 0.016f);
        }, iterations);

        double updateSpeedup = oldUpdateTime / newUpdateTime;

        std::cout << "Update() - Old: " << oldUpdateTime << " ms, New: " << newUpdateTime << " ms"
                  << " (Speedup: " << updateSpeedup << "x)" << std::endl;

        // InstanceData benchmark
        InitParticles(particlesOld, activeCount);
        InitParticles(particlesNew, activeCount);

        double oldInstanceTime = Benchmark([&]() {
            UpdateInstanceDataOld(particlesOld, instanceData);
        }, iterations);

        double newInstanceTime = Benchmark([&]() {
            UpdateInstanceDataNew(particlesNew, instanceData);
        }, iterations);

        double instanceSpeedup = oldInstanceTime / newInstanceTime;

        std::cout << "UpdateInstanceData() - Old: " << oldInstanceTime << " ms, New: " << newInstanceTime << " ms"
                  << " (Speedup: " << instanceSpeedup << "x)" << std::endl;

        double totalOld = oldUpdateTime + oldInstanceTime;
        double totalNew = newUpdateTime + newInstanceTime;
        double totalSpeedup = totalOld / totalNew;

        std::cout << "Total - Old: " << totalOld << " ms, New: " << totalNew << " ms"
                  << " (Speedup: " << totalSpeedup << "x)" << std::endl;
        std::cout << std::endl;
    }

    return 0;
}
