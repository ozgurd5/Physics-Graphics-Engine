#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t x;
    size_t y;
    size_t num_cells;
    float dt;
    float dx;
    float dens;
    float visc;
    float reynolds;
    float omega;
    int iter_count;
    float* u;
    float* v;
    float* p;
    float* div;
    float* smoke;
    uint8_t* solid;
    float* u_prev;
    float* v_prev;
    float* smoke_prev;
} FluidContext;

typedef struct {
    float inlet_velocity;
    float length_scale;
    float target_omega;
    float obstacle_x;
    float obstacle_y;
    size_t obstacle_radius;
} ScenarioParams;

FluidContext* fluid_create_context(size_t res_x, size_t res_y, float dt, float dx, float dens, float visc, int iters);
void fluid_setup_physics(FluidContext* ctx, ScenarioParams p);
void fluid_init(FluidContext* ctx, ScenarioParams p);
void fluid_step(FluidContext* ctx, ScenarioParams p);
void fluid_destroy_context(FluidContext* ctx);

#ifdef __cplusplus
}
#endif
