#include "graphics_engine.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

static std::string ReadFile(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

Renderer::Renderer(int width, int height, const char* title)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwMakeContextCurrent(m_Window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    m_FieldShader = CompileShader("assets/shaders/field.vert", "assets/shaders/field.frag");
    m_ArrowShader = CompileShader("assets/shaders/arrow.vert", "assets/shaders/arrow.frag");
    CreateQuad();
    CreateArrowBuffer();
    InitImGui();
}

Renderer::~Renderer()
{
    ShutdownImGui();
    glDeleteTextures(1, &m_FieldTex);
    glDeleteVertexArrays(1, &m_QuadVAO);
    glDeleteBuffers(1, &m_QuadVBO);
    glDeleteBuffers(1, &m_QuadIBO);
    glDeleteVertexArrays(1, &m_ArrowVAO);
    glDeleteBuffers(1, &m_ArrowVBO);
    glDeleteProgram(m_FieldShader);
    glDeleteProgram(m_ArrowShader);
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

bool Renderer::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Renderer::UpdateField(const float* data, int w, int h, float rangeMin, float rangeMax)
{
    if (m_FieldTex == 0 || w != m_FieldW || h != m_FieldH)
        CreateFieldTexture(w, h);

    m_RangeMin = rangeMin;
    m_RangeMax = rangeMax;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, m_FieldTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_FLOAT, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::UpdateArrows(const float* u, const float* v, int w, int h, int stride, float scale)
{
    if (stride < 1) stride = 1;

    std::vector<float> verts;
    verts.reserve(((size_t)(w / stride) * (h / stride)) * 12);

    const size_t u_stride = (size_t)h;
    const size_t v_stride = (size_t)h + 1;

    for (int i = stride / 2; i < w; i += stride)
    {
        for (int j = stride / 2; j < h; j += stride)
        {
            // Cell-centered velocity from staggered grid (matches IX_U / IX_V layout in fluid_physics).
            float u_c = 0.5f * (u[(size_t)i * u_stride + j] + u[(size_t)(i + 1) * u_stride + j]);
            float v_c = 0.5f * (v[(size_t)i * v_stride + j] + v[(size_t)i * v_stride + (j + 1)]);

            // After the UV swap in field.frag, screen-X = sim-i and screen-Y = sim-j.
            float base_x = 2.0f * ((float)i + 0.5f) / (float)w - 1.0f;
            float base_y = 2.0f * ((float)j + 0.5f) / (float)h - 1.0f;

            float dx    = u_c * scale;
            float dy    = v_c * scale;
            float tip_x = base_x + dx;
            float tip_y = base_y + dy;

            // Shaft
            verts.push_back(base_x); verts.push_back(base_y);
            verts.push_back(tip_x);  verts.push_back(tip_y);

            // Arrowhead — two wing line segments
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1e-6f)
            {
                float inv_len  = 1.0f / len;
                float nx       = dx * inv_len;
                float ny       = dy * inv_len;
                float head_len = 0.3f * len;
                float px       = -ny;
                float py       = nx;

                float w1x = tip_x - nx * head_len + px * head_len * 0.5f;
                float w1y = tip_y - ny * head_len + py * head_len * 0.5f;
                float w2x = tip_x - nx * head_len - px * head_len * 0.5f;
                float w2y = tip_y - ny * head_len - py * head_len * 0.5f;

                verts.push_back(tip_x); verts.push_back(tip_y);
                verts.push_back(w1x);   verts.push_back(w1y);
                verts.push_back(tip_x); verts.push_back(tip_y);
                verts.push_back(w2x);   verts.push_back(w2y);
            }
            else
            {
                for (int k = 0; k < 4; ++k)
                {
                    verts.push_back(tip_x);
                    verts.push_back(tip_y);
                }
            }
        }
    }

    int vertCount = (int)(verts.size() / 2);
    int byteSize  = (int)(verts.size() * sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, m_ArrowVBO);
    if (vertCount > m_ArrowVBOCap)
    {
        glBufferData(GL_ARRAY_BUFFER, byteSize, verts.data(), GL_DYNAMIC_DRAW);
        m_ArrowVBOCap = vertCount;
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, verts.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_ArrowVertCount = vertCount;
}

void Renderer::BeginUI()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Renderer::Draw(VisMode mode)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const bool drawField  = (mode == VisMode::Smoke ||
                             mode == VisMode::Pressure ||
                             mode == VisMode::VelocityMagnitude ||
                             mode == VisMode::FieldPlusVectors);
    const bool drawArrows = (mode == VisMode::VelocityVectorsOnly ||
                             mode == VisMode::FieldPlusVectors);

    if (drawField && m_FieldTex != 0)
    {
        glUseProgram(m_FieldShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_FieldTex);
        glUniform1i(glGetUniformLocation(m_FieldShader, "u_Field"), 0);
        glUniform1f(glGetUniformLocation(m_FieldShader, "u_RangeMin"), m_RangeMin);
        glUniform1f(glGetUniformLocation(m_FieldShader, "u_RangeMax"), m_RangeMax);

        glBindVertexArray(m_QuadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    if (drawArrows && m_ArrowVertCount > 0)
    {
        glUseProgram(m_ArrowShader);
        glBindVertexArray(m_ArrowVAO);
        glDrawArrays(GL_LINES, 0, m_ArrowVertCount);
        glBindVertexArray(0);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

unsigned int Renderer::CompileShader(const char* vertPath, const char* fragPath)
{
    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);
    const char* vSrc    = vertSrc.c_str();
    const char* fSrc    = fragSrc.c_str();

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
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_QuadIBO);

    glBindVertexArray(m_QuadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Renderer::CreateFieldTexture(int w, int h)
{
    if (m_FieldTex != 0)
        glDeleteTextures(1, &m_FieldTex);

    m_FieldW = w;
    m_FieldH = h;

    glGenTextures(1, &m_FieldTex);
    glBindTexture(GL_TEXTURE_2D, m_FieldTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::CreateArrowBuffer()
{
    glGenVertexArrays(1, &m_ArrowVAO);
    glGenBuffers(1, &m_ArrowVBO);

    glBindVertexArray(m_ArrowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_ArrowVBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void Renderer::ShutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
