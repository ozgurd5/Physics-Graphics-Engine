#include "graphics_engine.h"
#include "colormap.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <vector>

// ─── Internal helpers ────────────────────────────────────────────────────────

static std::string ReadFile(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ─── Renderer ────────────────────────────────────────────────────────────────

Renderer::Renderer(int width, int height, const char* title)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwMakeContextCurrent(m_Window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    m_Shader = CompileShader("assets/shaders/smoke.vert", "assets/shaders/smoke.frag");
    CreateQuad();
    // Smoke texture is created on the first UpdateSmoke() call once we know the grid size.
}

Renderer::~Renderer()
{
    glDeleteTextures(1, &m_SmokeTexture);
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_IBO);
    glDeleteProgram(m_Shader);
    glfwTerminate();
}

bool Renderer::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Renderer::UpdateSmoke(const float* smokeData, int width, int height)
{
    if (m_SmokeTexture == 0)
        CreateSmokeTexture(width, height);

    // Build RGBA pixel buffer using the turbo colormap.
    std::vector<RGBA> pixels(width * height);
    for (int i = 0; i < width * height; ++i)
        pixels[i] = turbo_colormap(smokeData[i]);

    glBindTexture(GL_TEXTURE_2D, m_SmokeTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::Draw()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_Shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SmokeTexture);
    glUniform1i(glGetUniformLocation(m_Shader, "u_Smoke"), 0);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

// ─── Private ─────────────────────────────────────────────────────────────────

unsigned int Renderer::CompileShader(const char* vertPath, const char* fragPath)
{
    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);
    const char* vSrc = vertSrc.c_str();
    const char* fSrc = fragSrc.c_str();

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vSrc, nullptr);
    glCompileShader(vert);

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fSrc, nullptr);
    glCompileShader(frag);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

void Renderer::CreateQuad()
{
    // Fullscreen quad in NDC. Each vertex: [x, y, u, v].
    float vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
         1.0f,  1.0f,  1.0f, 1.0f,  // top-right
        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_IBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // location 0: position (xy)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // location 1: tex coord (uv)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Renderer::CreateSmokeTexture(int width, int height)
{
    m_SimWidth  = width;
    m_SimHeight = height;

    glGenTextures(1, &m_SmokeTexture);
    glBindTexture(GL_TEXTURE_2D, m_SmokeTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}
