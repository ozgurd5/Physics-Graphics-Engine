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

constexpr float arrowhead_ratio = 0.3f;
constexpr float arrowhead_half_spread = 0.5f;
constexpr float min_arrow_length = 1e-6f;

static std::string read_file(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

graphics_engine::graphics_engine(int width, int height, const char* title)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfw_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwMakeContextCurrent(glfw_window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    field_shader = compile_shader("assets/shaders/field.vert", "assets/shaders/field.frag");
    arrow_shader = compile_shader("assets/shaders/arrow.vert", "assets/shaders/arrow.frag");
    create_quad();
    create_arrow_buffer();
    init_imgui();
}

graphics_engine::~graphics_engine()
{
    shutdown_imgui();
    glDeleteTextures(1, &field_texture);
    glDeleteVertexArrays(1, &quad_vao);
    glDeleteBuffers(1, &quad_vbo);
    glDeleteBuffers(1, &quad_ibo);
    glDeleteVertexArrays(1, &arrow_vao);
    glDeleteBuffers(1, &arrow_vbo);
    glDeleteProgram(field_shader);
    glDeleteProgram(arrow_shader);
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
}

bool graphics_engine::should_close() const
{
    return glfwWindowShouldClose(glfw_window);
}

void graphics_engine::update_field(const float* data, int width, int height, float range_min, float range_max)
{
    if (field_texture == 0 || width != field_width || height != field_height)
        create_field_texture(width, height);

    this->range_min = range_min;
    this->range_max = range_max;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, field_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void graphics_engine::update_arrows(const float* u, const float* v, int width, int height, int stride, float scale)
{
    if (stride < 1) stride = 1;

    std::vector<float> verts;
    verts.reserve(((size_t)(width / stride) * (height / stride)) * 12);

    const size_t u_stride = (size_t)height;
    const size_t v_stride = (size_t)height + 1;

    for (int i = stride / 2; i < width; i += stride)
    {
        for (int j = stride / 2; j < height; j += stride)
        {
            // Cell-centered velocity from staggered grid (matches IX_U / IX_V layout in fluid_physics).
            float u_center = 0.5f * (u[(size_t)i * u_stride + j] + u[(size_t)(i + 1) * u_stride + j]);
            float v_center = 0.5f * (v[(size_t)i * v_stride + j] + v[(size_t)i * v_stride + (j + 1)]);

            // After the UV swap in field.frag, screen-X = sim-i and screen-Y = sim-j.
            float base_x = 2.0f * ((float)i + 0.5f) / (float)width - 1.0f;
            float base_y = 2.0f * ((float)j + 0.5f) / (float)height - 1.0f;

            float delta_x = u_center * scale;
            float delta_y = v_center * scale;
            float tip_x = base_x + delta_x;
            float tip_y = base_y + delta_y;

            // Shaft
            verts.push_back(base_x);
            verts.push_back(base_y);
            verts.push_back(tip_x);
            verts.push_back(tip_y);

            // Arrowhead — two wing line segments
            float len = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (len > min_arrow_length)
            {
                float inv_len = 1.0f / len;
                float dir_x = delta_x * inv_len;
                float dir_y = delta_y * inv_len;
                float head_len = arrowhead_ratio * len;
                float perp_x = -dir_y;
                float perp_y = dir_x;

                float arrowhead_base_x = tip_x - dir_x * head_len;
                float arrowhead_base_y = tip_y - dir_y * head_len;
                float wing_offset_x = perp_x * head_len * arrowhead_half_spread;
                float wing_offset_y = perp_y * head_len * arrowhead_half_spread;

                float wing_a_x = arrowhead_base_x + wing_offset_x;
                float wing_a_y = arrowhead_base_y + wing_offset_y;
                float wing_b_x = arrowhead_base_x - wing_offset_x;
                float wing_b_y = arrowhead_base_y - wing_offset_y;

                verts.push_back(tip_x);
                verts.push_back(tip_y);
                verts.push_back(wing_a_x);
                verts.push_back(wing_a_y);
                verts.push_back(tip_x);
                verts.push_back(tip_y);
                verts.push_back(wing_b_x);
                verts.push_back(wing_b_y);
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

    int vert_count = (int)(verts.size() / 2);
    int byte_size = (int)(verts.size() * sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, arrow_vbo);
    if (vert_count > arrow_vbo_capacity)
    {
        glBufferData(GL_ARRAY_BUFFER, byte_size, verts.data(), GL_DYNAMIC_DRAW);
        arrow_vbo_capacity = vert_count;
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, verts.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    arrow_vert_count = vert_count;
}

void graphics_engine::begin_ui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void graphics_engine::draw(vis_mode mode)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const bool draw_field = (mode == vis_mode::smoke ||
                             mode == vis_mode::pressure ||
                             mode == vis_mode::velocity_magnitude ||
                             mode == vis_mode::field_plus_vectors);
    const bool draw_arrows = (mode == vis_mode::velocity_vectors_only ||
                              mode == vis_mode::field_plus_vectors);

    if (draw_field && field_texture != 0)
    {
        glUseProgram(field_shader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, field_texture);
        glUniform1i(glGetUniformLocation(field_shader, "u_field"), 0);
        glUniform1f(glGetUniformLocation(field_shader, "u_range_min"), range_min);
        glUniform1f(glGetUniformLocation(field_shader, "u_range_max"), range_max);

        glBindVertexArray(quad_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    if (draw_arrows && arrow_vert_count > 0)
    {
        glUseProgram(arrow_shader);
        glBindVertexArray(arrow_vao);
        glDrawArrays(GL_LINES, 0, arrow_vert_count);
        glBindVertexArray(0);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(glfw_window);
    glfwPollEvents();
}

unsigned int graphics_engine::compile_shader(const char* vert_path, const char* frag_path)
{
    std::string vert_source = read_file(vert_path);
    std::string frag_source = read_file(frag_path);
    const char* vert_chars = vert_source.c_str();
    const char* frag_chars = frag_source.c_str();

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vert_chars, nullptr);
    glCompileShader(vert);

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &frag_chars, nullptr);
    glCompileShader(frag);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

void graphics_engine::create_quad()
{
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &quad_vbo);
    glGenBuffers(1, &quad_ibo);

    glBindVertexArray(quad_vao);

    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void graphics_engine::create_field_texture(int width, int height)
{
    if (field_texture != 0)
        glDeleteTextures(1, &field_texture);

    field_width = width;
    field_height = height;

    glGenTextures(1, &field_texture);
    glBindTexture(GL_TEXTURE_2D, field_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void graphics_engine::create_arrow_buffer()
{
    glGenVertexArrays(1, &arrow_vao);
    glGenBuffers(1, &arrow_vbo);

    glBindVertexArray(arrow_vao);
    glBindBuffer(GL_ARRAY_BUFFER, arrow_vbo);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void graphics_engine::init_imgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void graphics_engine::shutdown_imgui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
