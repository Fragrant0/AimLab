#include "resource_manager.h"

#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace
{
    using ImageOwner = std::unique_ptr<void, decltype(&stbi_image_free)>;

    struct TextureUploadState
    {
        TextureUploadState()
        {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &Previous2D);
            glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &PreviousCubemap);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &PreviousAlignment);
        }
        ~TextureUploadState()
        {
            glBindTexture(GL_TEXTURE_2D, Previous2D);
            glBindTexture(GL_TEXTURE_CUBE_MAP, PreviousCubemap);
            glPixelStorei(GL_UNPACK_ALIGNMENT, PreviousAlignment);
        }
        GLint Previous2D = 0;
        GLint PreviousCubemap = 0;
        GLint PreviousAlignment = 4;
    };

    GLenum ImageFormat(int components, const std::string& path)
    {
        switch (components)
        {
        case 1: return GL_RED;
        case 2: return GL_RG;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default: throw std::runtime_error("[ResourceManager] Unsupported image channels: " + path);
        }
    }

    std::runtime_error ImageLoadError(const std::string& path)
    {
        const char* reason = stbi_failure_reason();
        return std::runtime_error("[ResourceManager] Failed to load image '" + path + "': " +
            (reason ? reason : "unknown decoder error"));
    }

    void CheckImageDimensions(int width, int height, const std::string& path)
    {
        GLint maxSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (width <= 0 || height <= 0 || width > maxSize || height > maxSize)
            throw std::runtime_error("[ResourceManager] Invalid image dimensions: " + path);
    }

    void CheckImageStorage(GLenum target, int width, int height, const std::string& path)
    {
        GLint actualWidth = 0;
        GLint actualHeight = 0;
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &actualWidth);
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &actualHeight);
        if (actualWidth != width || actualHeight != height)
            throw std::runtime_error("[ResourceManager] Failed to allocate image storage: " + path);
    }

    void DeleteTextureCache(std::map<std::string, unsigned int>& cache) noexcept
    {
        for (const auto& entry : cache)
            if (entry.second) glDeleteTextures(1, &entry.second);
        cache.clear();
    }

    struct TextureOwner
    {
        explicit TextureOwner(unsigned int texture = 0) : ID(texture) {}
        ~TextureOwner() { if (ID) glDeleteTextures(1, &ID); }
        TextureOwner(const TextureOwner&) = delete;
        TextureOwner& operator=(const TextureOwner&) = delete;
        unsigned int Release() { return std::exchange(ID, 0); }
        unsigned int ID;
    };

    // Capture passes must unwind without leaking GL objects or caller state.
    struct IBLCapture
    {
        IBLCapture()
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PreviousDrawFBO);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &PreviousReadFBO);
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &PreviousRBO);
            glGetIntegerv(GL_VIEWPORT, PreviousViewport);
            glGetIntegerv(GL_CURRENT_PROGRAM, &PreviousProgram);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &PreviousVAO);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &PreviousVBO);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &PreviousActiveTexture);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &PreviousTexture2D);
            glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &PreviousCubemap);
        }

        ~IBLCapture()
        {
            if (VAO) glDeleteVertexArrays(1, &VAO);
            if (VBO) glDeleteBuffers(1, &VBO);
            if (FBO) glDeleteFramebuffers(1, &FBO);
            if (RBO) glDeleteRenderbuffers(1, &RBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, PreviousDrawFBO);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, PreviousReadFBO);
            glBindRenderbuffer(GL_RENDERBUFFER, PreviousRBO);
            glViewport(PreviousViewport[0], PreviousViewport[1], PreviousViewport[2], PreviousViewport[3]);
            glUseProgram(PreviousProgram);
            glBindVertexArray(PreviousVAO);
            glBindBuffer(GL_ARRAY_BUFFER, PreviousVBO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, PreviousTexture2D);
            glBindTexture(GL_TEXTURE_CUBE_MAP, PreviousCubemap);
            glActiveTexture(PreviousActiveTexture);
        }

        IBLCapture(const IBLCapture&) = delete;
        IBLCapture& operator=(const IBLCapture&) = delete;

        unsigned int FBO = 0;
        unsigned int RBO = 0;
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        TextureOwner Texture;

    private:
        GLint PreviousDrawFBO = 0;
        GLint PreviousReadFBO = 0;
        GLint PreviousRBO = 0;
        GLint PreviousViewport[4] = {};
        GLint PreviousProgram = 0;
        GLint PreviousVAO = 0;
        GLint PreviousVBO = 0;
        GLint PreviousActiveTexture = GL_TEXTURE0;
        GLint PreviousTexture2D = 0;
        GLint PreviousCubemap = 0;
    };

    void CheckCaptureFramebuffer(const char* stage)
    {
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("[IBL] " + std::string(stage) + " framebuffer incomplete (status " + std::to_string(status) + ").");
    }
}

ResourceManager& ResourceManager::GetInstance()
{
    static ResourceManager instance;
    return instance;
}

ResourceManager::MapLoadTransaction ResourceManager::BeginMapLoad()
{
    return MapLoadTransaction(*this);
}

ResourceManager::MapLoadTransaction::MapLoadTransaction(ResourceManager& manager)
    : m_Manager(manager)
{
    if (m_Manager.m_MapLoadActive)
        throw std::logic_error("[ResourceManager] Nested map-load transactions are not supported.");
    m_Manager.m_MapLoadActive = true;
    SwapCaches();
}

void ResourceManager::MapLoadTransaction::SwapCaches() noexcept
{
    m_PreviousCaches[0].swap(m_Manager.m_Textures);
    m_PreviousCaches[1].swap(m_Manager.m_Cubemaps);
    m_PreviousCaches[2].swap(m_Manager.m_HDRTextures);
    m_PreviousCaches[3].swap(m_Manager.m_HDRSkyboxes);
    m_PreviousCaches[4].swap(m_Manager.m_IrradianceMaps);
    m_PreviousCaches[5].swap(m_Manager.m_PrefilterMaps);
}

ResourceManager::MapLoadTransaction::~MapLoadTransaction() noexcept
{
    if (m_Committed)
    {
        for (auto& cache : m_PreviousCaches)
            DeleteTextureCache(cache);
    }
    else
    {
        m_Manager.ClearMapResources();
        SwapCaches();
    }
    m_Manager.m_MapLoadActive = false;
}

Shader* ResourceManager::LoadShader(const std::string& name, const char* vertexPath, const char* fragmentPath)
{
    auto existing = m_Shaders.find(name);
    if (existing != m_Shaders.end())
    {
        return existing->second.get();
    }

    auto shader = std::make_unique<Shader>(vertexPath, fragmentPath);
    Shader* shaderPtr = shader.get();
    m_Shaders[name] = std::move(shader);
    return shaderPtr;
}

Shader* ResourceManager::GetShader(const std::string& name)
{
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end())
    {
        return it->second.get();
    }
    std::cout << "Shader not found: " << name << std::endl;
    return nullptr;
}

unsigned int ResourceManager::LoadTexture(const std::string& name, const char* path)
{
    if (m_Textures.find(name) != m_Textures.end())
    {
        return m_Textures[name];
    }

    if (!path || !*path)
        throw std::runtime_error("[ResourceManager] Empty texture path for: " + name);

    int width = 0, height = 0, nrComponents = 0;
    ImageOwner data(stbi_load(path, &width, &height, &nrComponents, 0), stbi_image_free);
    if (!data)
        throw ImageLoadError(path);
    CheckImageDimensions(width, height, path);
    const GLenum format = ImageFormat(nrComponents, path);
    TextureUploadState state;
    TextureOwner texture;
    glGenTextures(1, &texture.ID);
    if (!texture.ID)
        throw std::runtime_error("[ResourceManager] Failed to allocate texture: " + std::string(path));
    glBindTexture(GL_TEXTURE_2D, texture.ID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data.get());
    CheckImageStorage(GL_TEXTURE_2D, width, height, path);
    if (nrComponents != 1)
        glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nrComponents == 1 ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    m_Textures.emplace(name, texture.ID);
    return texture.Release();
}

unsigned int ResourceManager::GetTexture(const std::string& name)
{
    auto it = m_Textures.find(name);
    if (it != m_Textures.end())
    {
        return it->second;
    }
    std::cout << "Texture not found: " << name << std::endl;
    return 0;
}

void ResourceManager::UnloadTexture(const std::string& name)
{
    auto it = m_Textures.find(name);
    if (it != m_Textures.end())
    {
        glDeleteTextures(1, &it->second);
        m_Textures.erase(it);
        std::cout << "[ResourceManager] Unloaded texture: " << name << std::endl;
    }
}

unsigned int ResourceManager::LoadCubemap(const std::string& name, const std::vector<std::string>& faces)
{
    if (faces.size() != 6)
        throw std::runtime_error("[ResourceManager] Cubemap '" + name + "' requires exactly six face paths.");
    if (m_Cubemaps.find(name) != m_Cubemaps.end())
    {
        return m_Cubemaps[name];
    }

    TextureUploadState state;
    TextureOwner texture;
    unsigned int& textureID = texture.ID;
    glGenTextures(1, &textureID);
    if (!textureID)
        throw std::runtime_error("[ResourceManager] Failed to allocate cubemap: " + name);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int faceSize = 0;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        int width = 0, height = 0, nrChannels = 0;
        ImageOwner data(stbi_load(faces[i].c_str(), &width, &height, &nrChannels, STBI_rgb), stbi_image_free);
        if (!data)
            throw ImageLoadError(faces[i]);
        CheckImageDimensions(width, height, faces[i]);
        if (width != height || (i != 0 && width != faceSize))
            throw std::runtime_error("[ResourceManager] Cubemap faces must be equally sized squares: " + faces[i]);
        faceSize = width;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data.get());
        CheckImageStorage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, width, height, faces[i]);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    auto entry = m_Cubemaps.emplace(name, 0).first;
    try
    {
        PrecomputeIBL(name, textureID);
    }
    catch (...)
    {
        m_Cubemaps.erase(entry);
        throw;
    }
    entry->second = texture.Release();
    return entry->second;
}

unsigned int ResourceManager::GetCubemap(const std::string& name)
{
    auto it = m_Cubemaps.find(name);
    if (it != m_Cubemaps.end())
    {
        return it->second;
    }
    std::cout << "Cubemap not found: " << name << std::endl;
    return 0;
}

void ResourceManager::UnloadCubemap(const std::string& name)
{
    auto it = m_Cubemaps.find(name);
    if (it != m_Cubemaps.end())
    {
        glDeleteTextures(1, &it->second);
        m_Cubemaps.erase(it);
        auto irradianceIt = m_IrradianceMaps.find(name);
        if (irradianceIt != m_IrradianceMaps.end())
        {
            glDeleteTextures(1, &irradianceIt->second);
            m_IrradianceMaps.erase(irradianceIt);
        }
        auto prefilterIt = m_PrefilterMaps.find(name);
        if (prefilterIt != m_PrefilterMaps.end())
        {
            glDeleteTextures(1, &prefilterIt->second);
            m_PrefilterMaps.erase(prefilterIt);
        }
        std::cout << "[ResourceManager] Unloaded cubemap: " << name << std::endl;
    }
}

unsigned int ResourceManager::LoadHDRTexture(const std::string& name, const char* path)
{
    if (m_HDRTextures.find(name) != m_HDRTextures.end())
    {
        return m_HDRTextures[name];
    }

    if (!path || !*path)
        throw std::runtime_error("[ResourceManager] Empty HDR image path for: " + name);

    int width = 0, height = 0, nrComponents = 0;
    const bool isHDR = stbi_is_hdr(path) != 0;
    ImageOwner data(nullptr, stbi_image_free);
    if (isHDR)
    {
        stbi_set_flip_vertically_on_load(true);
        data.reset(stbi_loadf(path, &width, &height, &nrComponents, STBI_rgb));
        stbi_set_flip_vertically_on_load(false);
    }
    else
    {
        data.reset(stbi_load(path, &width, &height, &nrComponents, 0));
    }
    if (!data)
        throw ImageLoadError(path);
    CheckImageDimensions(width, height, path);

    const GLenum format = isHDR ? GL_RGB : ImageFormat(nrComponents, path);
    TextureUploadState state;
    TextureOwner texture;
    glGenTextures(1, &texture.ID);
    if (!texture.ID)
        throw std::runtime_error("[ResourceManager] Failed to allocate HDR texture: " + std::string(path));
    glBindTexture(GL_TEXTURE_2D, texture.ID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, isHDR ? GL_RGB16F : format, width, height, 0, format,
        isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE, data.get());
    CheckImageStorage(GL_TEXTURE_2D, width, height, path);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    std::cout << (isHDR ? "HDR texture loaded: " : "HDR texture loaded (from LDR): ")
              << path << " (" << width << "x" << height << ")" << std::endl;
    m_HDRTextures.emplace(name, texture.ID);
    return texture.Release();
}

unsigned int ResourceManager::GetHDRTexture(const std::string& name)
{
    auto it = m_HDRTextures.find(name);
    if (it != m_HDRTextures.end())
    {
        return it->second;
    }
    std::cout << "HDR texture not found: " << name << std::endl;
    return 0;
}

void ResourceManager::UnloadHDRTexture(const std::string& name)
{
    auto it = m_HDRTextures.find(name);
    if (it != m_HDRTextures.end())
    {
        glDeleteTextures(1, &it->second);
        m_HDRTextures.erase(it);
        std::cout << "[ResourceManager] Unloaded HDR texture: " << name << std::endl;
    }
}

unsigned int ResourceManager::LoadHDRSkybox(const std::string& name, const char* hdrPath)
{
    if (m_HDRSkyboxes.find(name) != m_HDRSkyboxes.end())
    {
        return m_HDRSkyboxes[name];
    }

    const std::string hdrName = name + "_hdr";
    const bool hadHDRTexture = m_HDRTextures.find(hdrName) != m_HDRTextures.end();
    unsigned int hdrTexture = LoadHDRTexture(hdrName, hdrPath);
    try
    {
        TextureOwner cubemap(CreateCubemapFromHDR(hdrTexture, 512));
        auto entry = m_HDRSkyboxes.emplace(name, 0).first;
        try
        {
            PrecomputeIBL(name, cubemap.ID);
        }
        catch (...)
        {
            m_HDRSkyboxes.erase(entry);
            throw;
        }
        entry->second = cubemap.Release();
        return entry->second;
    }
    catch (...)
    {
        if (!hadHDRTexture)
            UnloadHDRTexture(hdrName);
        throw;
    }
}

unsigned int ResourceManager::GetHDRSkybox(const std::string& name)
{
    auto it = m_HDRSkyboxes.find(name);
    if (it != m_HDRSkyboxes.end())
    {
        return it->second;
    }
    std::cout << "HDR skybox not found: " << name << std::endl;
    return 0;
}

void ResourceManager::UnloadHDRSkybox(const std::string& name)
{
    auto it = m_HDRSkyboxes.find(name);
    if (it != m_HDRSkyboxes.end())
    {
        glDeleteTextures(1, &it->second);
        m_HDRSkyboxes.erase(it);
        auto irradianceIt = m_IrradianceMaps.find(name);
        if (irradianceIt != m_IrradianceMaps.end())
        {
            glDeleteTextures(1, &irradianceIt->second);
            m_IrradianceMaps.erase(irradianceIt);
        }
        auto prefilterIt = m_PrefilterMaps.find(name);
        if (prefilterIt != m_PrefilterMaps.end())
        {
            glDeleteTextures(1, &prefilterIt->second);
            m_PrefilterMaps.erase(prefilterIt);
        }
        std::cout << "[ResourceManager] Unloaded HDR skybox: " << name << std::endl;
    }
}

unsigned int ResourceManager::CreateCubemapFromHDR(unsigned int hdrTexture, int cubemapSize)
{
    Shader* hdrToCubemapShader = LoadShader("hdr_to_cubemap", "shaders/hdr_to_cubemap.vs", "shaders/hdr_to_cubemap.fs");
    IBLCapture capture;
    unsigned int& captureFBO = capture.FBO;
    unsigned int& captureRBO = capture.RBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    if (!captureFBO || !captureRBO)
        throw std::runtime_error("[IBL] HDR capture framebuffer allocation failed.");

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubemapSize, cubemapSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Allocate the target cubemap faces before the capture pass.
    unsigned int& envCubemap = capture.Texture.ID;
    glGenTextures(1, &envCubemap);
    if (!envCubemap)
        throw std::runtime_error("[IBL] HDR cubemap allocation failed.");
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, cubemapSize, cubemapSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    hdrToCubemapShader->use();
    hdrToCubemapShader->setInt("equirectangularMap", 0);
    hdrToCubemapShader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int& skyboxVAO = capture.VAO;
    unsigned int& skyboxVBO = capture.VBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    if (!skyboxVAO || !skyboxVBO)
        throw std::runtime_error("[IBL] HDR capture geometry allocation failed.");
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glViewport(0, 0, cubemapSize, cubemapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        hdrToCubemapShader->setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        CheckCaptureFramebuffer("HDR cubemap");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);

    return capture.Texture.Release();
}

unsigned int ResourceManager::GetCubeVAO()
{
    if (m_CubeVAO != 0)
        return m_CubeVAO;

    float vertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_CubeVAO);
    glGenBuffers(1, &m_CubeVBO);
    glBindVertexArray(m_CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    return m_CubeVAO;
}

void ResourceManager::RenderCube()
{
    glBindVertexArray(GetCubeVAO());
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void ResourceManager::PrecomputeIBL(const std::string& skyboxName, unsigned int envCubemap)
{
    std::cout << "[IBL] Precomputing IBL for: " << skyboxName << std::endl;

    TextureOwner irradiance(CreateIrradianceMap(envCubemap));
    TextureOwner prefilter(CreatePrefilterMap(envCubemap));
    TextureOwner brdf(m_BRDFLUT == 0 ? CreateBRDFLUT() : 0);

    // Allocate both cache entries before transferring any ownership.
    const auto irradianceEntry = m_IrradianceMaps.emplace(skyboxName, 0);
    try
    {
        const auto prefilterEntry = m_PrefilterMaps.emplace(skyboxName, 0);
        if (irradianceEntry.first->second)
            glDeleteTextures(1, &irradianceEntry.first->second);
        if (prefilterEntry.first->second)
            glDeleteTextures(1, &prefilterEntry.first->second);
        irradianceEntry.first->second = irradiance.Release();
        prefilterEntry.first->second = prefilter.Release();
        if (brdf.ID)
            m_BRDFLUT = brdf.Release();
    }
    catch (...)
    {
        if (irradianceEntry.second)
            m_IrradianceMaps.erase(irradianceEntry.first);
        throw;
    }

    std::cout << "[IBL] Precomputation complete." << std::endl;
}

unsigned int ResourceManager::CreateIrradianceMap(unsigned int envCubemap)
{
    Shader* irradianceShader = LoadShader("irradiance", "shaders/hdr_to_cubemap.vs", "shaders/irradiance.fs");
    IBLCapture capture;
    unsigned int& captureFBO = capture.FBO;
    unsigned int& captureRBO = capture.RBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    if (!captureFBO || !captureRBO)
        throw std::runtime_error("[IBL] Irradiance framebuffer allocation failed.");

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    unsigned int& irradianceMap = capture.Texture.ID;
    glGenTextures(1, &irradianceMap);
    if (!irradianceMap)
        throw std::runtime_error("[IBL] Irradiance cubemap allocation failed.");
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    irradianceShader->use();
    irradianceShader->setInt("environmentMap", 0);
    irradianceShader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, 32, 32);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradianceShader->setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        CheckCaptureFramebuffer("irradiance");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return capture.Texture.Release();
}

unsigned int ResourceManager::CreatePrefilterMap(unsigned int envCubemap)
{
    Shader* prefilterShader = LoadShader("prefilter", "shaders/hdr_to_cubemap.vs", "shaders/prefilter.fs");
    IBLCapture capture;
    unsigned int& captureFBO = capture.FBO;
    unsigned int& captureRBO = capture.RBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    if (!captureFBO || !captureRBO)
        throw std::runtime_error("[IBL] Prefilter framebuffer allocation failed.");

    unsigned int& prefilterMap = capture.Texture.ID;
    glGenTextures(1, &prefilterMap);
    if (!prefilterMap)
        throw std::runtime_error("[IBL] Prefilter cubemap allocation failed.");
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    prefilterShader->use();
    prefilterShader->setInt("environmentMap", 0);
    prefilterShader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        unsigned int mipWidth = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader->setFloat("roughness", roughness);

        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilterShader->setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
            CheckCaptureFramebuffer("prefilter");
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return capture.Texture.Release();
}

unsigned int ResourceManager::CreateBRDFLUT()
{
    Shader* brdfShader = LoadShader("brdf", "shaders/brdf.vs", "shaders/brdf.fs");
    IBLCapture capture;
    unsigned int& captureFBO = capture.FBO;
    unsigned int& captureRBO = capture.RBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    if (!captureFBO || !captureRBO)
        throw std::runtime_error("[IBL] BRDF framebuffer allocation failed.");

    unsigned int& brdfLUTTexture = capture.Texture.ID;
    glGenTextures(1, &brdfLUTTexture);
    if (!brdfLUTTexture)
        throw std::runtime_error("[IBL] BRDF texture allocation failed.");
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 256, 256, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 256, 256);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);
    CheckCaptureFramebuffer("BRDF LUT");

    glViewport(0, 0, 256, 256);
    brdfShader->use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Full-screen quad for the BRDF integration pass.
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    unsigned int& quadVAO = capture.VAO;
    unsigned int& quadVBO = capture.VBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    if (!quadVAO || !quadVBO)
        throw std::runtime_error("[IBL] BRDF quad allocation failed.");
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    return capture.Texture.Release();
}

unsigned int ResourceManager::GetIrradianceMap(const std::string& skyboxName)
{
    auto it = m_IrradianceMaps.find(skyboxName);
    if (it != m_IrradianceMaps.end())
        return it->second;
    return 0;
}

unsigned int ResourceManager::GetPrefilterMap(const std::string& skyboxName)
{
    auto it = m_PrefilterMaps.find(skyboxName);
    if (it != m_PrefilterMaps.end())
        return it->second;
    return 0;
}

unsigned int ResourceManager::GetBRDFLUT()
{
    return m_BRDFLUT;
}

void ResourceManager::ClearMapResources() noexcept
{
    DeleteTextureCache(m_Textures);
    DeleteTextureCache(m_Cubemaps);
    DeleteTextureCache(m_HDRTextures);
    DeleteTextureCache(m_HDRSkyboxes);
    DeleteTextureCache(m_IrradianceMaps);
    DeleteTextureCache(m_PrefilterMaps);
}

void ResourceManager::Clear()
{
    m_Shaders.clear();
    ClearMapResources();

    if (m_BRDFLUT != 0)
    {
        glDeleteTextures(1, &m_BRDFLUT);
        m_BRDFLUT = 0;
    }

    if (m_CubeVAO != 0)
    {
        glDeleteVertexArrays(1, &m_CubeVAO);
        m_CubeVAO = 0;
    }
    if (m_CubeVBO != 0)
    {
        glDeleteBuffers(1, &m_CubeVBO);
        m_CubeVBO = 0;
    }
}
