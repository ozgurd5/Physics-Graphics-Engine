#pragma once

struct GLFWwindow;

enum class VisMode
{
    Smoke,
    Pressure,
    VelocityMagnitude,
    VelocityVectorsOnly,
    FieldPlusVectors,
};

class Renderer
{
public:
    Renderer(int width, int height, const char* title);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool        ShouldClose() const;
    GLFWwindow* Window() const { return m_Window; }

    // Upload a scalar field to an R32F texture; range used to normalize for the colormap.
    void UpdateField(const float* data, int w, int h, float rangeMin, float rangeMax);

    // Build arrow line segments from staggered velocity field.
    // stride: sample every Nth cell. scale: shaft length per unit velocity, in NDC units.
    void UpdateArrows(const float* u, const float* v, int w, int h, int stride, float scale);

    // Begin an ImGui frame (call before ImGui widget code).
    void BeginUI();

    // Run field/arrow passes per mode, render UI, swap buffers, poll events.
    void Draw(VisMode mode);

private:
    static unsigned int CompileShader(const char* vertPath, const char* fragPath);
    void                CreateQuad();
    void                CreateFieldTexture(int w, int h);
    void                CreateArrowBuffer();
    void                InitImGui();
    void                ShutdownImGui();

    GLFWwindow*  m_Window         = nullptr;

    unsigned int m_FieldShader    = 0;
    unsigned int m_QuadVAO        = 0;
    unsigned int m_QuadVBO        = 0;
    unsigned int m_QuadIBO        = 0;
    unsigned int m_FieldTex       = 0;
    int          m_FieldW         = 0;
    int          m_FieldH         = 0;
    float        m_RangeMin       = 0.0f;
    float        m_RangeMax       = 1.0f;

    unsigned int m_ArrowShader    = 0;
    unsigned int m_ArrowVAO       = 0;
    unsigned int m_ArrowVBO       = 0;
    int          m_ArrowVertCount = 0;
    int          m_ArrowVBOCap    = 0;
};
