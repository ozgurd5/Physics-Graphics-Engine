#include "graphics_engine.h"

extern "C" {
#include "types.h"
#include "core.h"
#include "scenarios.h"
#include "preconditioners.h"
}

int main()
{
    Renderer renderer(600, 600, "Fluid Simulation");

    FluidContext* ctx = fluid_create_context(256, 256, 0.016f, 0.1f, 1.0f, 0.001f, 200, 1e-5f);

    ScenarioParams params;
    Scenario scenario = load_scenario(KARMAN_VORTEX, ctx, &params);
    fluid_setup_physics(ctx, params, solve_pressure_rbgs, PRECOND_IDENTITY);
    scenario.init(ctx, params);

    while (!renderer.ShouldClose())
    {
        for (int step = 0; step < 20; ++step)
            fluid_step(ctx, params, scenario);

        renderer.UpdateSmoke(ctx->smoke, (int)ctx->x, (int)ctx->y);
        renderer.Draw();
    }

    fluid_destroy_context(ctx);
    return 0;
}
