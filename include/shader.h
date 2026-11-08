#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

class Shader
{
public:
    unsigned int ID = 0;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    ~Shader()
    {
        if (ID != 0)
        {
            glDeleteProgram(ID);
            ID = 0;
        }
    }

    Shader(const char* vertexPath, const char* fragmentPath)
    {
        const std::string vertexCode = ReadSource(vertexPath, "VERTEX");
        const std::string fragmentCode = ReadSource(fragmentPath, "FRAGMENT");
        const std::string programPaths = std::string(vertexPath) + " + " + fragmentPath;
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        unsigned int vertex = 0;
        unsigned int fragment = 0;
        unsigned int program = 0;

        try
        {
            vertex = glCreateShader(GL_VERTEX_SHADER);
            if (vertex == 0)
                throw std::runtime_error("[Shader] VERTEX creation failed (" + std::string(vertexPath) + "): OpenGL returned 0.");
            glShaderSource(vertex, 1, &vShaderCode, nullptr);
            glCompileShader(vertex);
            checkCompileErrors(vertex, "VERTEX", vertexPath);

            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            if (fragment == 0)
                throw std::runtime_error("[Shader] FRAGMENT creation failed (" + std::string(fragmentPath) + "): OpenGL returned 0.");
            glShaderSource(fragment, 1, &fShaderCode, nullptr);
            glCompileShader(fragment);
            checkCompileErrors(fragment, "FRAGMENT", fragmentPath);

            program = glCreateProgram();
            if (program == 0)
                throw std::runtime_error("[Shader] PROGRAM creation failed (" + programPaths + "): OpenGL returned 0.");
            glAttachShader(program, vertex);
            glAttachShader(program, fragment);
            glLinkProgram(program);
            checkCompileErrors(program, "PROGRAM", programPaths);
        }
        catch (...)
        {
            if (program != 0) glDeleteProgram(program);
            if (fragment != 0) glDeleteShader(fragment);
            if (vertex != 0) glDeleteShader(vertex);
            throw;
        }

        glDetachShader(program, vertex);
        glDetachShader(program, fragment);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        ID = program;
    }

    void use() const
    {
        glUseProgram(ID);
    }

    void setBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }

    void setInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec2(const std::string& name, const glm::vec2& value) const
    {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec2(const std::string& name, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
    }

    void setVec3(const std::string& name, const glm::vec3& value) const
    {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }

    void setVec4(const std::string& name, const glm::vec4& value) const
    {
        glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec4(const std::string& name, float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }

    void setMat2(const std::string& name, const glm::mat2& mat) const
    {
        glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    void setMat3(const std::string& name, const glm::mat3& mat) const
    {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

private:
    static std::string ReadSource(const char* path, const char* stage)
    {
        if (!path || *path == '\0')
            throw std::runtime_error("[Shader] " + std::string(stage) + " file read failed: empty path.");

        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            file.open(path, std::ios::binary);
            std::ostringstream source;
            source.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
            source << file.rdbuf();
            file.close();
            return source.str();
        }
        catch (const std::ios_base::failure& error)
        {
            throw std::runtime_error("[Shader] " + std::string(stage) + " file read failed (" + path + "): " + error.what());
        }
    }

    static void checkCompileErrors(GLuint shader, const std::string& type, const std::string& paths)
    {
        const bool isProgram = (type == "PROGRAM");
        GLint success = GL_FALSE;
        if (isProgram)
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
        else
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_TRUE)
            return;

        GLint logLength = 0;
        if (isProgram)
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        else
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::string infoLog(static_cast<std::size_t>(logLength > 0 ? logLength : 1), '\0');
        GLsizei written = 0;
        if (isProgram)
            glGetProgramInfoLog(shader, static_cast<GLsizei>(infoLog.size()), &written, infoLog.data());
        else
            glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), &written, infoLog.data());
        infoLog.resize(static_cast<std::size_t>(written > 0 ? written : 0));
        if (infoLog.empty())
            infoLog = "OpenGL returned no diagnostic log.";

        throw std::runtime_error("[Shader] " + type + (isProgram ? " link failed (" : " compile failed (") + paths + "):\n" + infoLog);
    }
};
#endif
