#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <glad/glad.h>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "shader.h"

class ResourceManager
{
public:
    static ResourceManager& GetInstance();

    Shader* LoadShader(const std::string& name, const char* vertexPath, const char* fragmentPath);
    Shader* GetShader(const std::string& name);

    unsigned int LoadTexture(const std::string& name, const char* path);
    unsigned int GetTexture(const std::string& name);
    void UnloadTexture(const std::string& name);

    unsigned int LoadCubemap(const std::string& name, const std::vector<std::string>& faces);
    unsigned int GetCubemap(const std::string& name);
    void UnloadCubemap(const std::string& name);

    unsigned int LoadHDRTexture(const std::string& name, const char* path);
    unsigned int GetHDRTexture(const std::string& name);
    void UnloadHDRTexture(const std::string& name);

    // Converts an equirectangular HDR image into a cubemap skybox.
    unsigned int LoadHDRSkybox(const std::string& name, const char* hdrPath);
    unsigned int GetHDRSkybox(const std::string& name);
    void UnloadHDRSkybox(const std::string& name);

    // Precomputed image-based lighting resources for an HDR skybox.
    unsigned int GetIrradianceMap(const std::string& skyboxName);
    unsigned int GetPrefilterMap(const std::string& skyboxName);
    unsigned int GetBRDFLUT();

    void Clear();

private:
    ResourceManager() {}
    ~ResourceManager() {}
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // Creates the runtime skybox cubemap used by IBL preprocessing.
    unsigned int CreateCubemapFromHDR(unsigned int hdrTexture, int cubemapSize = 512);

    // Generates irradiance, prefilter, and BRDF lookup textures.
    void PrecomputeIBL(const std::string& skyboxName, unsigned int envCubemap);
    unsigned int CreateIrradianceMap(unsigned int envCubemap);
    unsigned int CreatePrefilterMap(unsigned int envCubemap);
    unsigned int CreateBRDFLUT();

    // Shared cube geometry for cubemap capture passes.
    unsigned int GetCubeVAO();
    void RenderCube();

    std::map<std::string, std::unique_ptr<Shader>> m_Shaders;
    std::map<std::string, unsigned int> m_Textures;
    std::map<std::string, unsigned int> m_Cubemaps;
    std::map<std::string, unsigned int> m_HDRTextures;
    std::map<std::string, unsigned int> m_HDRSkyboxes;

    std::map<std::string, unsigned int> m_IrradianceMaps;
    std::map<std::string, unsigned int> m_PrefilterMaps;
    unsigned int m_BRDFLUT = 0;
    unsigned int m_CubeVAO = 0;
    unsigned int m_CubeVBO = 0;
};

#endif
