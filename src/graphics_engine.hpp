#pragma once

struct GLFWwindow;

enum class vis_mode
{
    smoke,
    pressure,
    velocity_magnitude,
    velocity_vectors_only,
    field_plus_vectors,
};

class graphics_engine
{
public:
    graphics_engine(int width, int height, const char* title);
    ~graphics_engine();

    graphics_engine(const graphics_engine&) = delete;
    graphics_engine& operator=(const graphics_engine&) = delete;

    bool should_close() const;
    GLFWwindow* window() const { return glfw_window; }

    // Upload a scalar field to an R32F texture; range used to normalize for the colormap.
    void update_field(const float* data, int width, int height, float range_min, float range_max);

    // Build arrow line segments from staggered velocity field.
    // stride: sample every Nth cell. scale: shaft length per unit velocity, in NDC units.
    void update_arrows(const float* u, const float* v, int width, int height, int stride, float scale);

    // Begin an ImGui frame (call before ImGui widget code).
    void begin_ui();

    // Run field/arrow passes per mode, render UI, swap buffers, poll events.
    void draw(vis_mode mode);

private:
    static unsigned int compile_shader(const char* vert_path, const char* frag_path);
    void create_quad();
    void create_field_texture(int width, int height);
    void create_arrow_buffer();
    void init_imgui();
    void shutdown_imgui();

    GLFWwindow* glfw_window = nullptr;

    unsigned int field_shader = 0;
    unsigned int quad_vao = 0;
    unsigned int quad_vbo = 0;
    unsigned int quad_ibo = 0;
    unsigned int field_texture = 0;
    int field_width = 0;
    int field_height = 0;
    float range_min = 0.0f;
    float range_max = 1.0f;

    unsigned int arrow_shader = 0;
    unsigned int arrow_vao = 0;
    unsigned int arrow_vbo = 0;
    int arrow_vert_count = 0;
    int arrow_vbo_capacity = 0;
};
