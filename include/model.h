#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "stb_image.h"
#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <limits>
using namespace std;

inline unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);

class Model
{
public:
    // model data 
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;
    glm::vec3 BoundsMin;
    glm::vec3 BoundsMax;

    // constructor, expects a filepath to a 3D model.
    Model(string const& path, bool gamma = false)
        : gammaCorrection(gamma),
          BoundsMin(glm::vec3(std::numeric_limits<float>::max())),
          BoundsMax(glm::vec3(std::numeric_limits<float>::lowest()))
    {
        loadModel(path);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader& shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    glm::vec3 GetBoundsMin() const { return BoundsMin; }
    glm::vec3 GetBoundsMax() const { return BoundsMax; }
    glm::vec3 GetBoundsSize() const { return BoundsMax - BoundsMin; }

private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const& path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene, glm::mat4(1.0f));

        glm::vec3 size = GetBoundsSize();
        std::cout << "[Model] Loaded " << path << " | meshes: " << meshes.size()
                  << " | cached textures: " << textures_loaded.size()
                  << " | bounds size: (" << size.x << ", " << size.y << ", " << size.z << ")"
                  << std::endl;
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    glm::mat4 ConvertAssimpMatrix(const aiMatrix4x4& matrix)
    {
        glm::mat4 result;
        result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
        result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
        result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
        result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
        return result;
    }

    void processNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
    {
        glm::mat4 nodeTransform = parentTransform * ConvertAssimpMatrix(node->mTransformation);

        // process each mesh located at the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene, nodeTransform));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene, nodeTransform);
        }

    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        glm::mat3 normalTransform = glm::inverseTranspose(glm::mat3(transform));

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vector = glm::vec3(transform * glm::vec4(vector, 1.0f));
            vertex.Position = vector;
            BoundsMin = glm::min(BoundsMin, vector);
            BoundsMax = glm::max(BoundsMax, vector);
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = glm::normalize(normalTransform * vector);
            }
            // texture coordinates
            if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                if (mesh->HasTangentsAndBitangents())
                {
                    // tangent
                    vector.x = mesh->mTangents[i].x;
                    vector.y = mesh->mTangents[i].y;
                    vector.z = mesh->mTangents[i].z;
                    vertex.Tangent = glm::normalize(normalTransform * vector);
                    // bitangent
                    vector.x = mesh->mBitangents[i].x;
                    vector.y = mesh->mBitangents[i].y;
                    vector.z = mesh->mBitangents[i].z;
                    vertex.Bitangent = glm::normalize(normalTransform * vector);
                }
                else
                {
                    vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                    vertex.Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
                vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                vertex.Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            for (int boneIndex = 0; boneIndex < MAX_BONE_INFLUENCE; ++boneIndex)
            {
                vertex.m_BoneIDs[boneIndex] = -1;
                vertex.m_Weights[boneIndex] = 0.0f;
            }

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. albedo/base color maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_albedo", true);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        vector<Texture> baseColorMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_albedo", true);
        textures.insert(textures.end(), baseColorMaps.begin(), baseColorMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        std::vector<Texture> heightNormalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), heightNormalMaps.begin(), heightNormalMaps.end());
        // 4. height maps
        std::vector<Texture> metallicMaps = loadMaterialTextures(material, aiTextureType_METALNESS, "texture_metallic");
        textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());
        std::vector<Texture> roughnessMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "texture_roughness");
        textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());
        std::vector<Texture> ambientMaps = loadMaterialTextures(material, aiTextureType_AMBIENT_OCCLUSION, "texture_ao");
        textures.insert(textures.end(), ambientMaps.begin(), ambientMaps.end());
        std::vector<Texture> lightMaps = loadMaterialTextures(material, aiTextureType_LIGHTMAP, "texture_ao");
        textures.insert(textures.end(), lightMaps.begin(), lightMaps.end());
        std::vector<Texture> emissiveMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_emissive", true);
        textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());

        AddLocalPBRTextureFallbacks(textures);

        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures);
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    bool IsMetalRoughnessName(const string& path)
    {
        string lower = ToLower(path);
        return lower.find("metalrough") != string::npos ||
               lower.find("metal_rough") != string::npos ||
               lower.find("metallicrough") != string::npos ||
               (lower.find("metal") != string::npos && lower.find("rough") != string::npos);
    }

    string ResolveTextureType(const string& typeName, const string& path)
    {
        if ((typeName == "texture_metallic" || typeName == "texture_roughness") && IsMetalRoughnessName(path))
            return "texture_arm";
        return typeName;
    }

    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, bool srgb = false)
    {
        vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    if (textures_loaded[j].id != 0)
                        textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if (!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory, srgb);
                texture.type = ResolveTextureType(typeName, str.C_Str());
                texture.path = str.C_Str();
                if (texture.id != 0)
                {
                    textures.push_back(texture);
                    textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
                }
            }
        }
        return textures;
    }

    bool HasTextureType(const vector<Texture>& textures, const string& typeName)
    {
        return std::any_of(textures.begin(), textures.end(), [&](const Texture& texture) {
            return texture.id != 0 && texture.type == typeName;
        });
    }

    string ToLower(string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    void AddTextureIfPresent(vector<Texture>& textures, const std::filesystem::path& filePath,
                             const string& typeName, bool srgb)
    {
        const string pathString = filePath.generic_string();
        for (const Texture& loaded : textures_loaded)
        {
            if (loaded.id != 0 && loaded.path == pathString)
            {
                textures.push_back(loaded);
                return;
            }
        }

        Texture texture;
        texture.id = TextureFromFile(filePath.filename().string().c_str(), this->directory, srgb);
        texture.type = typeName;
        texture.path = pathString;
        if (texture.id != 0)
        {
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }

    void AddLocalPBRTextureFallbacks(vector<Texture>& textures)
    {
        namespace fs = std::filesystem;
        fs::path dir(directory);
        if (!fs::exists(dir))
            return;

        for (const auto& entry : fs::recursive_directory_iterator(dir))
        {
            if (!entry.is_regular_file())
                continue;

            const string filename = ToLower(entry.path().filename().string());
            const string extension = ToLower(entry.path().extension().string());
            if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".tga")
                continue;

            if (!HasTextureType(textures, "texture_arm") &&
                (filename.find("arm") != string::npos || IsMetalRoughnessName(filename)))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_arm", false);
            }
            else if (!HasTextureType(textures, "texture_albedo") &&
                     (filename.find("diff") != string::npos || filename.find("albedo") != string::npos ||
                      filename.find("basecolor") != string::npos || filename.find("base_color") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_albedo", true);
            }
            else if (!HasTextureType(textures, "texture_normal") &&
                     (filename.find("nor") != string::npos || filename.find("normal") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_normal", false);
            }
            else if (!HasTextureType(textures, "texture_metallic") &&
                     (filename.find("metal") != string::npos || filename.find("mtl") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_metallic", false);
            }
            else if (!HasTextureType(textures, "texture_roughness") &&
                     (filename.find("rough") != string::npos || filename.find("rgh") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_roughness", false);
            }
            else if (!HasTextureType(textures, "texture_ao") &&
                     (filename.find("ao") != string::npos || filename.find("occlusion") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_ao", false);
            }
            else if (!HasTextureType(textures, "texture_emissive") &&
                     (filename.find("emissive") != string::npos || filename.find("emit") != string::npos))
            {
                AddTextureIfPresent(textures, entry.path(), "texture_emissive", true);
            }
        }
    }
};


unsigned int TextureFromFile(const char* path, const string& directory, bool gamma)
{
    namespace fs = std::filesystem;

    string requestedPath = string(path);
    string filename = directory + '/' + requestedPath;
    string loadedFilename = filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (!data)
    {
        const fs::path request(requestedPath);
        const fs::path modelDir(directory);
        const string stem = request.stem().string();
        std::vector<string> extensions = { request.extension().string(), ".png", ".jpg", ".jpeg", ".tga" };

        for (const string& extension : extensions)
        {
            if (extension.empty())
                continue;

            const fs::path candidate = modelDir / request.parent_path() / (stem + extension);
            if (candidate.generic_string() == filename)
                continue;

            data = stbi_load(candidate.generic_string().c_str(), &width, &height, &nrComponents, 0);
            if (data)
            {
                loadedFilename = candidate.generic_string();
                std::cout << "[Model] Texture fallback: " << path << " -> " << loadedFilename << std::endl;
                break;
            }
        }

        if (!data)
        {
            for (const auto& entry : fs::recursive_directory_iterator(modelDir))
            {
                if (!entry.is_regular_file())
                    continue;

                const fs::path candidate = entry.path();
                if (candidate.stem().string() != stem)
                    continue;

                data = stbi_load(candidate.generic_string().c_str(), &width, &height, &nrComponents, 0);
                if (data)
                {
                    loadedFilename = candidate.generic_string();
                    std::cout << "[Model] Texture fallback: " << path << " -> " << loadedFilename << std::endl;
                    break;
                }
            }
        }
    }

    if (data)
    {
        GLenum format;
        GLenum internalFormat;
        if (nrComponents == 1)
        {
            format = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 2)
        {
            format = GL_RG;
            internalFormat = GL_RG;
        }
        else if (nrComponents == 3)
        {
            format = GL_RGB;
            internalFormat = gamma ? GL_SRGB : GL_RGB;
        }
        else if (nrComponents == 4)
        {
            format = GL_RGBA;
            internalFormat = gamma ? GL_SRGB_ALPHA : GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    return textureID;
}
#endif
