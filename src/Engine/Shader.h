#pragma once
#include <glm/mat4x4.hpp>
#include <string>

namespace cubiq {

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void bind() const;

    void setInt(const char* name, int value) const;
    void setFloat(const char* name, float value) const;
    void setVec2(const char* name, const glm::vec2& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setVec4(const char* name, const glm::vec4& value) const;
    void setMat4(const char* name, const glm::mat4& value) const;

private:
    unsigned int m_program = 0;
};

} // namespace cubiq
