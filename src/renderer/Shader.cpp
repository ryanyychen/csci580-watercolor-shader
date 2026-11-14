#include "Shader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <glm/gtc/type_ptr.hpp>

namespace
{
// Compile one GLSL stage and include its filename in any driver error message.
GLuint compile(GLenum type, const std::filesystem::path &path)
{
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Cannot open shader: " + path.string());

    std::ostringstream contents;
    contents << file.rdbuf();
    const std::string source = contents.str();
    const char *data = source.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &data, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error(path.string() + ":\n" + log);
    }
    return shader;
}
} // namespace

Shader::Shader(const std::filesystem::path &vertex, const std::filesystem::path &fragment)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vertex);
    GLuint fs = 0;
    try
    {
        fs = compile(GL_FRAGMENT_SHADER, fragment);
    }
    catch (...)
    {
        // The vertex stage must also be released if fragment compilation fails.
        glDeleteShader(vs);
        throw;
    }

    // Linking checks stage interfaces and creates the executable GPU program.
    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    // Attached stages can be deleted after linking; the program retains what it needs.
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        GLint length = 0;
        glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetProgramInfoLog(id_, length, nullptr, log.data());
        glDeleteProgram(id_);
        id_ = 0;
        throw std::runtime_error("Shader link failed:\n" + log);
    }
}

Shader::~Shader()
{
    if (id_)
        glDeleteProgram(id_);
}

void Shader::use() const
{
    glUseProgram(id_);
}

// GLM stores matrices column-major, so upload them without transposing (GL_FALSE).
void Shader::set(const char *name, const glm::mat4 &v) const
{
    glUniformMatrix4fv(glGetUniformLocation(id_, name), 1, GL_FALSE, glm::value_ptr(v));
}

void Shader::set(const char *name, const glm::vec3 &v) const
{
    glUniform3fv(glGetUniformLocation(id_, name), 1, glm::value_ptr(v));
}

void Shader::set(const char *name, float v) const
{
    glUniform1f(glGetUniformLocation(id_, name), v);
}

void Shader::set(const char *name, int v) const
{
    glUniform1i(glGetUniformLocation(id_, name), v);
}
