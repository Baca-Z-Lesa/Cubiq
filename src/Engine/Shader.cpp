#include "Engine/Shader.h"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cubiq {

namespace {
    std::string readFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
            throw std::runtime_error("Nenasiel sa subor: " + path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    GLuint compile(GLenum type, const std::string& source, const std::string& path)
    {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            glDeleteShader(shader);
            throw std::runtime_error("Chyba v shaderi " + path + ":\n" + log);
        }
        return shader;
    }
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    GLuint vs = compile(GL_VERTEX_SHADER, readFile(vertexPath), vertexPath);
    GLuint fs = compile(GL_FRAGMENT_SHADER, readFile(fragmentPath), fragmentPath);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string("Chyba pri linkovani shadera:\n") + log);
    }
}

Shader::~Shader() { glDeleteProgram(m_program); }

void Shader::bind() const { glUseProgram(m_program); }

void Shader::setInt(const char* name, int value) const
{
    glUniform1i(glGetUniformLocation(m_program, name), value);
}

void Shader::setFloat(const char* name, float value) const
{
    glUniform1f(glGetUniformLocation(m_program, name), value);
}

void Shader::setVec2(const char* name, const glm::vec2& value) const
{
    glUniform2fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(value));
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(value));
}

void Shader::setVec4(const char* name, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(value));
}

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_program, name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace cubiq
