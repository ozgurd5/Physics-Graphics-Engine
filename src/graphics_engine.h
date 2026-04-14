#pragma once

struct GLFWwindow;

class Renderer
{
public:
    Renderer(int width, int height, const char* title);
    ~Renderer();

    bool ShouldClose() const;

    // Convert smoke float array to RGBA using turbo colormap and upload to GPU.
    void UpdateSmoke(const float* smokeData, int width, int height);

    // Clear, draw the fullscreen quad, swap buffers, poll events.
    void Draw();

private:
    static unsigned int CompileShader(const char* vertPath, const char* fragPath);
    void CreateQuad();
    void CreateSmokeTexture(int width, int height);

    GLFWwindow*  m_Window       = nullptr;
    unsigned int m_Shader       = 0;
    unsigned int m_VAO          = 0;
    unsigned int m_VBO          = 0;
    unsigned int m_IBO          = 0;
    unsigned int m_SmokeTexture = 0;
    int          m_SimWidth     = 0;
    int          m_SimHeight    = 0;
};
