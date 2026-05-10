#version 460 core
in  vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Field;
uniform float u_RangeMin;
uniform float u_RangeMax;

// Turbo colormap polynomial approximation (Google, 2019).
vec3 turbo(float t)
{
    t = clamp(t, 0.0, 1.0);
    float t2 = t  * t;
    float t3 = t2 * t;
    float t4 = t3 * t;
    float t5 = t4 * t;

    float r = 0.13572138 + 4.61539260*t  - 42.66032258*t2 + 132.13108234*t3 - 152.94239396*t4 + 59.28637943*t5;
    float g = 0.09140261 + 2.19418839*t  +  4.84296658*t2 -  14.18503333*t3 +   4.27729857*t4 +  2.82956604*t5;
    float b = 0.10667330 + 12.64194608*t - 60.58204836*t2 + 110.36276771*t3 -  89.90310912*t4 + 27.34824973*t5;
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

void main()
{
    // Swap UV: simulation array layout is smoke[i*y+j], so OpenGL's S axis
    // (row-fast) corresponds to sim j, T to sim i. Sampling with .yx makes
    // screen-X map to sim i (the inlet/wind axis) and screen-Y to sim j.
    float raw = texture(u_Field, v_TexCoord.yx).r;

    float range = u_RangeMax - u_RangeMin;
    float t = range > 1e-6 ? (raw - u_RangeMin) / range : 0.0;

    FragColor = vec4(turbo(t), 1.0);
}
