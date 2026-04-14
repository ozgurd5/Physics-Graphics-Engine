#pragma once

#include <cstdint>

struct RGBA { uint8_t r, g, b, a; };

// Turbo colormap polynomial approximation (Google, 2019).
// Input t in [0, 1] maps dark-blue (0) -> green (0.5) -> orange-red (1).
static inline RGBA turbo_colormap(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float t2 = t  * t;
    const float t3 = t2 * t;
    const float t4 = t3 * t;
    const float t5 = t4 * t;

    float r = 0.13572138f + 4.61539260f*t  - 42.66032258f*t2 + 132.13108234f*t3 - 152.94239396f*t4 + 59.28637943f*t5;
    float g = 0.09140261f + 2.19418839f*t  +  4.84296658f*t2 -  14.18503333f*t3 +   4.27729857f*t4 +  2.82956604f*t5;
    float b = 0.10667330f + 12.64194608f*t - 60.58204836f*t2 + 110.36276771f*t3 -  89.90310912f*t4 + 27.34824973f*t5;

    auto clamp_byte = [](float x) -> uint8_t {
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        return static_cast<uint8_t>(x * 255.0f + 0.5f);
    };

    return { clamp_byte(r), clamp_byte(g), clamp_byte(b), 255 };
}
