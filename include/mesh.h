#ifndef MESH_H
#define MESH_H

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
using namespace std;

#define MAX_BONE_INFLUENCE 4

struct Vertex {
    // position
    glm::vec3 Position;
    // normal
    glm::vec3 Normal;
    // texCoords
    glm::vec2 TexCoords;
    // tangent
    glm::vec3 Tangent;
    // bitangent
    glm::vec3 Bitangent;
    //bone indexes which will influence this vertex
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    //weights from each bone
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
    unsigned int id;
    string type;
    string path;
};

class Mesh {
public:
    // mesh Data
    vector<Vertex>       vertices;
    vector<unsigned int> indices;
    vector<Texture>      textures;
    unsigned int VAO;

    // constructor
    Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures)
        : vertices(std::move(vertices)),
          indices(std::move(indices)),
          textures(std::move(textures)),
          VAO(0),
          VBO(0),
          EBO(0)
    {
        // now that we have all the required data, set the vertex buffers and its attribute pointers.
        setupMesh();
    }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept
        : vertices(std::move(other.vertices)),
          indices(std::move(other.indices)),
          textures(std::move(other.textures)),
          VAO(other.VAO),
          VBO(other.VBO),
          EBO(other.EBO)
    {
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }

    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            Cleanup();
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
            textures = std::move(other.textures);
            VAO = other.VAO;
            VBO = other.VBO;
            EBO = other.EBO;
            other.VAO = 0;
            other.VBO = 0;
            other.EBO = 0;
        }
        return *this;
    }

    ~Mesh()
    {
        Cleanup();
    }

    // render the mesh
    void Draw(Shader& shader)
    {
        shader.setBool("u_HasAlbedoMap", false);
        shader.setBool("u_HasNormalMap", false);
        shader.setBool("u_HasMetallicMap", false);
        shader.setBool("u_HasRoughnessMap", false);
        shader.setBool("u_HasAOMap", false);
        shader.setBool("u_HasARMMap", false);
        shader.setBool("u_HasEmissiveMap", false);

        // bind appropriate textures
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        unsigned int normalNr = 1;
        unsigned int heightNr = 1;
        unsigned int reflectNr = 1;
        unsigned int nextLegacyUnit = 0;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            int textureUnit = -1;
            // retrieve texture number (the N in diffuse_textureN)
            string number;
            string name = textures[i].type;

            if (name == "texture_albedo" || name == "texture_diffuse")
            {
                textureUnit = 0;
                shader.setInt("u_AlbedoMap", textureUnit);
                shader.setBool("u_HasAlbedoMap", true);
                number = std::to_string(diffuseNr++);
            }
            else if (name == "texture_normal")
            {
                textureUnit = 1;
                shader.setInt("u_NormalMap", textureUnit);
                shader.setBool("u_HasNormalMap", true);
                number = std::to_string(normalNr++);
            }
            else if (name == "texture_metallic")
            {
                textureUnit = 2;
                shader.setInt("u_MetallicMap", textureUnit);
                shader.setBool("u_HasMetallicMap", true);
            }
            else if (name == "texture_roughness")
            {
                textureUnit = 3;
                shader.setInt("u_RoughnessMap", textureUnit);
                shader.setBool("u_HasRoughnessMap", true);
            }
            else if (name == "texture_ao")
            {
                textureUnit = 4;
                shader.setInt("u_AOMap", textureUnit);
                shader.setBool("u_HasAOMap", true);
            }
            else if (name == "texture_arm")
            {
                textureUnit = 5;
                shader.setInt("u_ARMMap", textureUnit);
                shader.setBool("u_HasARMMap", true);
            }
            else if (name == "texture_emissive")
            {
                textureUnit = 9;
                shader.setInt("u_EmissiveMap", textureUnit);
                shader.setBool("u_HasEmissiveMap", true);
            }
            else if (name == "texture_specular")
            {
                textureUnit = nextLegacyUnit++;
                number = std::to_string(specularNr++); // transfer unsigned int to string
            }
            else if (name == "texture_ambient")
            {
                textureUnit = nextLegacyUnit++;
                number = std::to_string(heightNr++); // transfer unsigned int to string
            }
            else
            {
                textureUnit = nextLegacyUnit++;
            }

            textureUnit = std::clamp(textureUnit, 0, 15);

            glActiveTexture(GL_TEXTURE0 + textureUnit); // active proper texture unit before binding

             //   std::cout << (name + number) << i << std::endl;
            
            // now set the sampler to the correct texture unit
            if (!number.empty())
                glUniform1i(glGetUniformLocation(shader.ID, (name + number).c_str()), textureUnit);

            // and finally bind the texture
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }


        // draw mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // always good practice to set everything back to defaults once configured.
        glActiveTexture(GL_TEXTURE0);
    }

    void DrawGeometry() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    // render data 
    unsigned int VBO, EBO;

    void Cleanup()
    {
        if (EBO) { glDeleteBuffers(1, &EBO); EBO = 0; }
        if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
        if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    }

    // initializes all the buffer objects/arrays
    void setupMesh()
    {
        // create buffers/arrays
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        // load data into vertex buffers
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // A great thing about structs is that their memory layout is sequential for all its items.
        // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
        // again translates to 3/2 floats which translates to a byte array.
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // set the vertex attribute pointers
        // vertex Positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // vertex normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        // vertex tangent
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
        // vertex bitangent
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
        // ids
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

        // weights
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
        glBindVertexArray(0);
    }
};
#endif
