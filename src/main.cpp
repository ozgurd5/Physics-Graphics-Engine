#include "graphics_engine.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include <omp.h>

extern "C" {
#include "types.h"
#include "core.h"
#include "scenarios.h"
#include "preconditioners.h"
}

constexpr float pi = 3.14159265358979323846f;

struct RuntimeControls
{
    vis_mode mode = vis_mode::smoke;
    ScenarioType scenarioType = KARMAN_VORTEX;
    int substepsPerFrame = 5;
    int arrowStride = 8;
    float arrowScale = 0.04f;
    bool autoOmega = true;
    bool requestReset = false;
    bool requestRebuildScenario = false;

    // 0 = PCG, 1 = RBGS, 2 = SOR
    int          solverIdx              = 0;
    // 0 = Identity, 1 = Jacobi, 2 = Multigrid (placeholder)
    int          precondIdx             = 1;
};

PressureSolver pick_solver(int idx)
{
    switch (idx)
    {
        case 1: return solve_pressure_rbgs;
        case 2: return solve_pressure_sor;
        default: return solve_pressure_pcg;
    }
}

PrecondType pick_precond(int idx)
{
    switch (idx)
    {
        case 0: return PRECOND_IDENTITY;
        case 2: return PRECOND_MULTIGRID;
        default: return PRECOND_JACOBI;
    }
}

void scan_min_max(const float* data, size_t n, float& outMin, float& outMax)
{
    outMin = std::numeric_limits<float>::infinity();
    outMax = -std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < n; ++i)
    {
        float v = data[i];
        if (v < outMin) outMin = v;
        if (v > outMax) outMax = v;
    }
    if (!std::isfinite(outMin) || !std::isfinite(outMax))
    {
        outMin = 0.0f;
        outMax = 1.0f;
    }
}

void compute_velocity_magnitude(const FluidContext* ctx, float* out, float& outMin, float& outMax)
{
    outMin = std::numeric_limits<float>::infinity();
    outMax = -std::numeric_limits<float>::infinity();

    const size_t y      = ctx->y;
    const size_t y_plus = y + 1;

    for (size_t i = 0; i < ctx->x; ++i)
    {
        for (size_t j = 0; j < y; ++j)
        {
            float u_c = 0.5f * (ctx->u[i * y + j] + ctx->u[(i + 1) * y + j]);
            float v_c = 0.5f * (ctx->v[i * y_plus + j] + ctx->v[i * y_plus + (j + 1)]);
            float mag = std::sqrt(u_c * u_c + v_c * v_c);
            out[i * y + j] = mag;
            if (mag < outMin) outMin = mag;
            if (mag > outMax) outMax = mag;
        }
    }
    if (!std::isfinite(outMin) || !std::isfinite(outMax) || outMax <= outMin)
    {
        outMin = 0.0f;
        outMax = 1.0f;
    }
}

Scenario reload_scenario(FluidContext* ctx, ScenarioParams* p, ScenarioType type,
                         PressureSolver solver, PrecondType precond)
{
    const size_t u_count = (ctx->x + 1) * ctx->y;
    const size_t v_count = ctx->x * (ctx->y + 1);
    const size_t cells   = ctx->num_cells;

    std::memset(ctx->u, 0, u_count * sizeof(float));
    std::memset(ctx->v, 0, v_count * sizeof(float));
    std::memset(ctx->p, 0, cells * sizeof(float));
    std::memset(ctx->div, 0, cells * sizeof(float));
    std::memset(ctx->smoke, 0, cells * sizeof(float));
    std::memset(ctx->solid, 0, cells * sizeof(uint8_t));
    std::memset(ctx->u_prev, 0, u_count * sizeof(float));
    std::memset(ctx->v_prev, 0, v_count * sizeof(float));
    std::memset(ctx->smoke_prev, 0, cells * sizeof(float));
    std::memset(ctx->cg_r, 0, cells * sizeof(float));
    std::memset(ctx->cg_d, 0, cells * sizeof(float));
    std::memset(ctx->cg_q, 0, cells * sizeof(float));
    std::memset(ctx->cg_z, 0, cells * sizeof(float));

    Scenario s = load_scenario(type, ctx, p);
    fluid_setup_physics(ctx, *p, solver, precond);
    s.init(ctx, *p);
    return s;
}

void DrawControlPanel(RuntimeControls& ctrl, FluidContext* ctx, ScenarioParams& params)
{
    ImGui::Begin("Controls");

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("%.1f FPS  (%.2f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::Text("Reynolds: %.2f", ctx->reynolds);
    ImGui::Text("OpenMP threads: %d", omp_get_max_threads());
    ImGui::Separator();

    ImGui::Text("Visualization");
    int mode = (int)ctrl.mode;
    ImGui::RadioButton("Smoke",           &mode, (int)vis_mode::smoke);
    ImGui::RadioButton("Pressure",        &mode, (int)vis_mode::pressure);
    ImGui::RadioButton("Velocity Mag",    &mode, (int)vis_mode::velocity_magnitude);
    ImGui::RadioButton("Vectors Only",    &mode, (int)vis_mode::velocity_vectors_only);
    ImGui::RadioButton("Field + Vectors", &mode, (int)vis_mode::field_plus_vectors);
    ctrl.mode = (vis_mode)mode;
    ImGui::Separator();

    static const char* scenario_items[] = { "Lid-Driven", "Karman Vortex", "Airfoil", "Urban City" };
    int sc = (int)ctrl.scenarioType;
    if (ImGui::Combo("Scenario", &sc, scenario_items, IM_ARRAYSIZE(scenario_items)))
    {
        ctrl.scenarioType           = (ScenarioType)sc;
        ctrl.requestRebuildScenario = true;
    }
    ImGui::Separator();

    ImGui::Text("Solver");
    static const char* solver_items[]  = { "PCG", "RBGS", "SOR" };
    static const char* precond_items[] = { "Identity", "Jacobi", "Multigrid" };
    if (ImGui::Combo("Pressure Solver", &ctrl.solverIdx, solver_items, IM_ARRAYSIZE(solver_items)))
        ctx->pressure_solver = pick_solver(ctrl.solverIdx);
    if (ImGui::Combo("Preconditioner", &ctrl.precondIdx, precond_items, IM_ARRAYSIZE(precond_items)))
    {
        // Setting the function pointer directly mirrors what fluid_setup_physics does internally.
        switch (pick_precond(ctrl.precondIdx))
        {
            case PRECOND_IDENTITY:  ctx->precondition = precondition_identity;  break;
            case PRECOND_JACOBI:    ctx->precondition = precondition_jacobi;    break;
            case PRECOND_MULTIGRID: ctx->precondition = precondition_multigrid; break;
        }
    }
    ImGui::Separator();

    ImGui::Text("Parameters");
    ImGui::SliderFloat("Inlet Velocity", &params.inlet_velocity, 0.0f, 5.0f);
    ImGui::SliderFloat("Viscosity", &ctx->visc, 0.0001f, 0.1f, "%.5f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("dt", &ctx->dt, 0.001f, 0.05f, "%.4f");
    ImGui::Checkbox("Auto Omega", &ctrl.autoOmega);
    if (ctrl.autoOmega)
        ImGui::Text("Omega: %.4f (auto)", ctx->omega);
    else
        ImGui::SliderFloat("Omega", &ctx->omega, 1.0f, 1.99f);
    ImGui::SliderInt("Poisson Iter", &ctx->poisson_iter, 1, 2000);
    ImGui::SliderFloat("Threshold", &ctx->threshold, 1e-7f, 1e-3f, "%.7f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderInt("Substeps/Frame", &ctrl.substepsPerFrame, 1, 50);
    ImGui::SliderInt("Arrow Stride", &ctrl.arrowStride, 2, 32);
    ImGui::SliderFloat("Arrow Scale", &ctrl.arrowScale, 0.001f, 0.2f, "%.4f", ImGuiSliderFlags_Logarithmic);
    ImGui::Separator();

    if (ctrl.scenarioType == KARMAN_VORTEX)
    {
        ImGui::Text("Karman Obstacle");
        ImGui::SliderFloat("Obstacle X", &params.obstacle_x, 1.0f, (float)ctx->x - 2.0f);
        ImGui::SliderFloat("Obstacle Y", &params.obstacle_y, 1.0f, (float)ctx->y - 2.0f);
        int radius = (int)params.obstacle_radius;
        if (ImGui::SliderInt("Obstacle Radius", &radius, 2, (int)(ctx->y / 4)))
            params.obstacle_radius = (size_t)radius;
        if (ImGui::Button("Rebuild Solids"))
            ctrl.requestRebuildScenario = true;
        ImGui::Separator();
    }

    if (ImGui::Button("Reset"))
        ctrl.requestReset = true;

    ImGui::End();
}

int main()
{
    graphics_engine engine(800, 800, "Fluid Simulation");

    FluidContext* ctx = fluid_create_context(256, 256, 0.016f, 0.1f, 1.0f, 0.001f, 100, 1e-4f);

    RuntimeControls ctrl;
    ScenarioParams  params;
    Scenario        scenario = reload_scenario(ctx, &params, ctrl.scenarioType,
                                               pick_solver(ctrl.solverIdx),
                                               pick_precond(ctrl.precondIdx));

    std::vector<float> velMag((size_t)ctx->num_cells, 0.0f);

    double lastTime   = glfwGetTime();
    double titleTimer = 0.0;

    while (!engine.should_close())
    {
        double now = glfwGetTime();
        double dt  = now - lastTime;
        lastTime   = now;

        // Auto-recompute omega each frame if enabled (the user may have changed grid via rebuild).
        if (ctrl.autoOmega)
            ctx->omega = 2.0f / (1.0f + std::sin(pi / (float)ctx->x));

        for (int s = 0; s < ctrl.substepsPerFrame; ++s)
            fluid_step(ctx, params, scenario);

        engine.begin_ui();
        DrawControlPanel(ctrl, ctx, params);

        if (ctrl.requestReset || ctrl.requestRebuildScenario)
        {
            scenario                    = reload_scenario(ctx, &params, ctrl.scenarioType,
                                                          pick_solver(ctrl.solverIdx),
                                                          pick_precond(ctrl.precondIdx));
            ctrl.requestReset           = false;
            ctrl.requestRebuildScenario = false;
        }

        const int w = (int)ctx->x;
        const int h = (int)ctx->y;

        switch (ctrl.mode)
        {
            case vis_mode::smoke:
                engine.update_field(ctx->smoke, w, h, 0.0f, 1.0f);
                break;
            case vis_mode::pressure:
            {
                float pmin, pmax;
                scan_min_max(ctx->p, ctx->num_cells, pmin, pmax);
                if (pmax - pmin < 1e-6f) { pmin = 0.0f; pmax = 1.0f; }
                engine.update_field(ctx->p, w, h, pmin, pmax);
                break;
            }
            case vis_mode::velocity_magnitude:
            case vis_mode::field_plus_vectors:
            {
                float vmin, vmax;
                compute_velocity_magnitude(ctx, velMag.data(), vmin, vmax);
                engine.update_field(velMag.data(), w, h, vmin, vmax);
                break;
            }
            case vis_mode::velocity_vectors_only:
                break;
        }

        if (ctrl.mode == vis_mode::velocity_vectors_only || ctrl.mode == vis_mode::field_plus_vectors)
            engine.update_arrows(ctx->u, ctx->v, w, h, ctrl.arrowStride, ctrl.arrowScale);

        engine.draw(ctrl.mode);

        titleTimer += dt;
        if (titleTimer > 0.5)
        {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "Fluid Simulation - %.1f FPS", 1.0 / (dt > 0.0 ? dt : 1.0));
            glfwSetWindowTitle(engine.window(), buf);
            titleTimer = 0.0;
        }
    }

    fluid_destroy_context(ctx);
    return 0;
}
