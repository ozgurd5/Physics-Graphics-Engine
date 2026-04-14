#include "graphics_engine.h"
#include "simulation_engine.h"

int main()
{
    Renderer renderer(800, 800, "Fluid Simulation");

    FluidContext* ctx = fluid_create_context(256, 256, 0.016f, 0.1f, 1.0f, 0.001f, 20);

    ScenarioParams params;
    params.inlet_velocity  = 1.0f;
    params.obstacle_x      = (float)(ctx->x / 4);
    params.obstacle_y      = (float)(ctx->y / 2);
    params.obstacle_radius = 10;
    params.length_scale    = 2.0f * (float)params.obstacle_radius * ctx->dx;
    params.target_omega    = 0.0f;

    fluid_setup_physics(ctx, params);
    fluid_init(ctx, params);

    while (!renderer.ShouldClose())
    {
        // Match the original simulation's 20 physics steps per visual frame.
        for (int step = 0; step < 20; ++step)
            fluid_step(ctx, params);

        renderer.UpdateSmoke(ctx->smoke, (int)ctx->x, (int)ctx->y);
        renderer.Draw();
    }

    fluid_destroy_context(ctx);
    return 0;
}
