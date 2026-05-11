# GraphicsEngine — Codebase Reference

Read this to get oriented before working on the project. For coding-style rules, see [AGENTS.md](AGENTS.md). For low-level OpenGL/GLSL background, see [teaching_material.md](teaching_material.md).

## TL;DR — what this project actually is

Despite the name "GraphicsEngine" and the apparent split into "graphics + physics," this is **not** a general-purpose 3D engine and **not** a rigid-body physics engine. It is:

- A **2D incompressible-fluid (CFD) simulator** written in C — the Navier–Stokes equations on a staggered MAC grid, with three pressure solvers (PCG / RBGS / SOR) and pluggable preconditioners. Lives in `src/fluid_physics/`.
- A **minimal OpenGL 4.6 visualizer** written in C++ that shows the simulation as a colored scalar field (Turbo colormap) plus optional velocity arrows.
- An **ImGui control panel** for live parameter tweaking and scenario selection.
- Bundled with substantial **teaching material** — the project is educational/instructional (graphics + numerical-methods learning).

When you see "physics" in this project, it means **fluid dynamics** (Navier–Stokes), not collisions/rigid bodies.

## Ownership

The codebase is split between two developers:

- **`src/fluid_physics/` (C, the Navier–Stokes solver)** — owned by another developer. **Read-only from this side**: do not edit, refactor, or reformat any file in this folder. Read it for understanding only.
- **Everything else (C++ graphics, shaders, build, docs, root files)** — fully editable. [AGENTS.md](AGENTS.md) governs the style for new and modified code here.

## Tech stack

| Layer | Tech | Version |
|---|---|---|
| Language (renderer) | C++ | C++17 |
| Language (solver) | C | C11 |
| Build | CMake | 4.2+ |
| Windowing/input | GLFW | 3.4 |
| GL loader | GLAD | bundled in `include/` + `src/glad.c` |
| Graphics API | OpenGL Core | 4.6 |
| UI | Dear ImGui | v1.91.5 (GLFW + OpenGL3 backends) |
| Parallelism | OpenMP | optional via `find_package(OpenMP)` |
| Target | Single executable | `GraphicsEngine` |

All third-party dependencies are pulled via CMake `FetchContent` — no vendored libs. See [CMakeLists.txt](CMakeLists.txt).

## Project layout

```
C:\Projects\GraphicsEngine\
├── AGENTS.md                    coding-standards rules for this project
├── CMakeLists.txt               build config (FetchContent for GLFW/GLM/ImGui)
├── codebase_documentation.md    this file
├── imgui.ini                    ImGui window state (auto-generated)
├── teaching_material.md         ~2,800 lines of OpenGL/GLSL teaching content
├── teaching_material_rules.md   meta-guidelines for the teaching doc
├── include/
│   ├── glad/glad.h              OpenGL function loader header
│   └── KHR/khrplatform.h        Khronos platform defs
├── src/
│   ├── main.cpp                 entry point + main loop + ImGui panel (299 lines)
│   ├── graphics_engine.hpp      graphics_engine class interface (65 lines)
│   ├── graphics_engine.cpp      graphics_engine impl (~300 lines)
│   ├── glad.c                   GL loader impl (~1.1 MB generated)
│   └── fluid_physics/           THE "physics" — Navier-Stokes solver in C (READ-ONLY)
│       ├── types.h              FluidContext, ScenarioParams structs
│       ├── core.h / core.c      advect, diffuse, project, pressure solvers (~700 lines C)
│       ├── boundaries.h / .c    no-slip, slip, inlet, outlet BCs
│       ├── scenarios.h / .c     4 scenarios: lid-driven, Karman, airfoil, urban
│       ├── preconditioners.h / .c   Identity, Jacobi, Multigrid(stub) for PCG
│       ├── utilities.h          inline helpers: MAC indexing, Laplacians, macros
│       └── main.c               STANDALONE driver — NOT linked into final exe
└── assets/
    └── shaders/
        ├── field.vert / .frag   fullscreen-quad + Turbo colormap of R32F texture
        └── arrow.vert / .frag   white line segments for velocity vectors
```

## The renderer (C++) — `src/graphics_engine.{hpp,cpp}`

A small, self-contained `graphics_engine` class. It owns the GLFW window, the GL context, two shader programs, a fullscreen quad, a dynamic line-buffer for arrows, and the ImGui setup.

Key surface ([graphics_engine.hpp](src/graphics_engine.hpp)):

- `graphics_engine(int width, int height, const char* title)` — creates window, loads GL via GLAD, compiles shaders, builds quad + arrow VBO, inits ImGui.
- `update_field(const float* data, int width, int height, float range_min, float range_max)` — uploads a scalar field to a `GL_R32F` texture via `glTexSubImage2D`; resizes texture on dimension change; stashes min/max for colormap normalization.
- `update_arrows(const float* u, const float* v, int width, int height, int stride, float scale)` — samples the staggered velocity field every `stride` cells, averages u/v to cell centers, generates shaft + arrowhead line segments in NDC, uploads to a dynamic VBO.
- `begin_ui()` — `ImGui_ImplOpenGL3_NewFrame` + `ImGui_ImplGlfw_NewFrame` + `ImGui::NewFrame`.
- `draw(vis_mode mode)` — clears, optionally runs field pass, optionally runs arrow pass, renders ImGui draw data, swaps buffers, polls events.

`vis_mode` enum: `smoke`, `pressure`, `velocity_magnitude`, `velocity_vectors_only`, `field_plus_vectors`.

**Shaders** (all in [assets/shaders/](assets/shaders/)):
- `field.vert` / `field.frag` — quad passthrough; fragment samples a `GL_R32F` texture and applies the **Turbo colormap** (Google 2019 polynomial approximation), normalized to `[u_range_min, u_range_max]`. The fragment uses `texture(u_field, v_tex_coord.yx)` — the `.yx` swap accounts for the simulation's row-major `[i*y + j]` layout, putting screen-X along sim-`i` and screen-Y along sim-`j`.
- `arrow.vert` / `arrow.frag` — passthrough vertex, solid-white fragment. Drawn as `GL_LINES`. The arrow geometry is built CPU-side every frame in `graphics_engine::update_arrows`.

Shader naming uses the project's Hungarian convention: `a_*` for vertex attributes, `v_*` for varyings, `u_*` for uniforms, `frag_color` for the fragment output. See [AGENTS.md](AGENTS.md) → "Shaders (GLSL)".

Shader paths are hardcoded relative to CWD (`"assets/shaders/field.vert"`), so the working directory must be the project root at launch.

## The "physics engine" — `src/fluid_physics/` (C, read-only)

A grid-based, **incompressible**, **2D**, **Eulerian** Navier–Stokes solver on a **MAC (staggered)** grid. All state lives in a single `FluidContext` struct.

### Data model — [types.h](src/fluid_physics/types.h)

`FluidContext` owns:
- **Domain**: `x`, `y` (grid extents), `num_cells = x*y`, `dx` (cell size in meters).
- **Physics**: `dt`, `dens`, `visc`, `reynolds`, `omega` (SOR relaxation), `poisson_iter`, `diffuse_iter`, `threshold`.
- **Velocity (staggered)**: `u` sized `(x+1)*y`, `v` sized `x*(y+1)`.
- **Pressure / divergence (cell-centered)**: `p`, `div`.
- **Scalar transport**: `smoke`.
- **Geometry**: `solid` (uint8_t mask — 1 = solid, 0 = fluid).
- **Previous frame** (for semi-implicit schemes): `u_prev`, `v_prev`, `smoke_prev`.
- **Function pointers**: `pressure_solver`, `precondition` — swappable at runtime.
- **PCG workspace**: `cg_r` (residual), `cg_d` (direction), `cg_q` (Ap), `cg_z` (preconditioned residual).

`ScenarioParams` carries per-scenario knobs: inlet velocity, length scale, target omega, obstacle position/radius (for Karman), chord length and angle of attack (for airfoil).

### Simulation pipeline — `fluid_step(ctx, params, scenario)` in [core.c](src/fluid_physics/core.c)

Per substep:
1. **Sources** — `scenario.apply_sources(ctx, params)` (inject smoke / drive inlet).
2. **Boundaries** — `scenario.apply_boundaries(ctx, params)` (no-slip on solids, inlet/outlet on edges).
3. **Diffusion** — `diffuse_velocity()` and `diffuse_scalar()`. Auto-switches between explicit forward-Euler and implicit Gauss-Seidel based on the diffusion CFL number `a = dt*ν/dx²` (explicit when stable, implicit otherwise).
4. **Advection** — semi-Lagrangian backtrace with bilinear interpolation (`advect_velocity`, `advect_scalar`). Unconditionally stable.
5. **Divergence** — 5-point stencil over the staggered grid (`compute_divergence`).
6. **Pressure projection** — solve `∇²p = (ρ/dt)·∇·u` via the user-selected pressure solver, then `subtract_gradient()` corrects velocities so they are divergence-free.

### Pressure solvers (`ctx->pressure_solver` is a function pointer)

| Solver | Function | Notes |
|---|---|---|
| **PCG** (default) | `solve_pressure_pcg` | Preconditioned Conjugate Gradient using the `cg_*` workspace; preconditioner pluggable. |
| **RBGS** | `solve_pressure_rbgs` | Red-Black Gauss-Seidel; parallelization-friendly cell ordering. |
| **SOR** | `solve_pressure_sor` | Successive Over-Relaxation; uses `ctx->omega`. Optimal omega auto-computed each frame as `2 / (1 + sin(π/x))` when "Auto Omega" is on (see [main.cpp:236](src/main.cpp#L236)). |

### Preconditioners — [preconditioners.c](src/fluid_physics/preconditioners.c)

- `precondition_identity` — M = I (no preconditioning).
- `precondition_jacobi` — M⁻¹ = diag(A)⁻¹.
- `precondition_multigrid` — **stub / placeholder**, not real multigrid.

### Boundary conditions — [boundaries.c](src/fluid_physics/boundaries.c)

- `bound_apply_no_slip` — zero velocity at solid boundaries.
- `bound_apply_slip_horizontal` — zero normal component on horizontal walls.
- `bound_apply_inlet_left` — constant velocity + optional smoke injection at the left edge.
- `bound_apply_outlet_right` — zero-gradient (Neumann) outflow at the right edge.
- `bound_build_outer_walls` — flags the domain perimeter as solid.

### Scenarios — [scenarios.c](src/fluid_physics/scenarios.c)

Four selectable scenarios via the `Scenario` struct (`init`, `apply_sources`, `apply_boundaries` callbacks):

| Enum | Name | What it tests |
|---|---|---|
| `LID_DRIVEN` | Lid-driven cavity | Classic steady-state recirculation benchmark. |
| `KARMAN_VORTEX` (default) | Karman vortex street | Vortex shedding behind a circular cylinder; obstacle pose is editable live. |
| `AIRFOIL` | NACA 2412 airfoil | Aerodynamic flow with configurable angle of attack. |
| `URBAN_CITY` | Multi-obstacle | Flow through a grid of rectangular buildings. |

### Utilities — [utilities.h](src/fluid_physics/utilities.h)

Inline header providing the workhorses: staggered-grid indexing macros (`IX`, `IX_U`, `IX_V`), 5-point Laplacian stencils for cell-centered and u/v fields with Neumann boundary handling at solids, explicit-diffusion update helpers, `MAX`/`MIN`/`SWAP_PTR`/`BUILD_BLOCK`, plus `GET_TIME_SEC()` and the `INLINE` macro that resolves per-compiler.

### Not linked: `src/fluid_physics/main.c`

A standalone driver for headless validation (it has `#ifdef VALIDATE` / `#ifdef OUTPUT` paths). **It is not in the executable's source list in CMakeLists.txt** — only `core.c`, `boundaries.c`, `scenarios.c`, `preconditioners.c` are compiled into `GraphicsEngine`.

## Integration: how physics meets graphics

There is **no GameObject / Entity / Scene abstraction**. The simulation grid *is* the scene. The same `FluidContext*` pointer that `fluid_step()` mutates is the same pointer the renderer reads (`ctx->smoke`, `ctx->u`, `ctx->v`, `ctx->p`) — no copies, no sync layer, no IDs.

### Main loop — [main.cpp:228-294](src/main.cpp#L228)

```
while (!engine.should_close())
{
    delta_time = now - last_time;

    if (auto_omega) fluid_context->omega = 2 / (1 + sin(π / fluid_context->x));

    for (s = 0; s < substeps_per_frame; ++s)
        fluid_step(fluid_context, params, scenario);    // physics

    engine.begin_ui();
    draw_control_panel(controls, fluid_context, params);  // ImGui widgets

    if (request_reset || request_rebuild_scenario)
        scenario = reload_scenario(...);            // memset + re-init

    // upload field data to GPU based on visualization mode
    switch (controls.mode) {
        case smoke:                   engine.update_field(fluid_context->smoke, ...)
        case pressure:                scan_min_max(...); engine.update_field(fluid_context->p, ...)
        case velocity_magnitude:
        case field_plus_vectors:      compute_velocity_magnitude(...); engine.update_field(velocity_magnitudes, ...)
        case velocity_vectors_only:   (skip field)
    }
    if (mode == velocity_vectors_only || field_plus_vectors)
        engine.update_arrows(fluid_context->u, fluid_context->v, ..., arrow_stride, arrow_scale);

    engine.draw(controls.mode);                     // GL passes + ImGui render + swap
    // update window title with FPS every 0.5s
}
fluid_destroy_context(fluid_context);
```

Physics and graphics share **one timestep**: each rendered frame runs `substeps_per_frame` (default 5) physics steps with `fluid_context->dt` (default 0.016s). There is no fixed-timestep accumulator, no decoupled physics thread, no interpolation — when you raise substeps, the sim runs faster wall-clock; when you raise dt, the sim's *physical* time per step grows.

### Control panel — `draw_control_panel()` in [main.cpp](src/main.cpp)

Live-tunable knobs:
- Visualization mode (5 radio buttons).
- Scenario combo (rebuilds on change).
- Solver combo (PCG / RBGS / SOR) and preconditioner combo (Identity / Jacobi / Multigrid).
- Sliders: inlet velocity, viscosity (log), dt, omega (or auto), Poisson iterations (1–2000), threshold (log), substeps/frame (1–50), arrow stride, arrow scale (log).
- Karman-only: obstacle X, Y, radius + "Rebuild Solids" button.
- Reset button.
- Read-outs: FPS, Reynolds number, OpenMP thread count.

### Memory & ownership

- `FluidContext` is heap-allocated by `fluid_create_context()` (uses `calloc` for all buffers) and freed by `fluid_destroy_context()`.
- `graphics_engine` is a stack value in `main`, RAII-managed (destructor releases GL objects and shuts down ImGui).
- No smart pointers, no handle tables, no pools.

## Build & run

CMake-driven, single target:
```
cmake -S . -B build
cmake --build build
./build/GraphicsEngine     # CWD must be project root so "assets/shaders/..." resolves
```
Default window is 800×800 titled "Fluid Simulation"; default grid is 256×256, default scenario is Karman vortex.

## Recent commit history

```
03dbed4  Overhaul Update and Teaching Material
196f1c0  .hpp files                              -- introduced graphics_engine.hpp
1723097  Fluid Graphics Connected                -- wired the C solver into main.cpp
a322e11  Fluid Graphics Added                    -- added the C fluid module (1619 lines)
7ae8b99  init
```

The fluid module replaced an earlier `simulation_engine.c/.h` (888 lines, no longer in tree). The folder it lives in was originally `src/fluid_graphics/` and has since been renamed `src/fluid_physics/` to match its actual purpose.

## Things that are NOT here

- No rigid bodies, no collision detection (sphere/box/GJK/EPA), no constraint solver, no integrator selection (Euler/Verlet/RK4) in the rigid-body sense.
- No 3D anything — no meshes, no models, no `.obj`/`.fbx`/`.gltf`, no camera, no view/projection matrices, no lighting model (Phong/PBR), no shadows, no deferred/forward path.
- No texture files on disk; the only texture is created at runtime to receive scalar-field data.
- No scene graph, no entity system, no asset manager, no resource cache.
- No README, no CI config, no test suite.
- No third-party math library — all math is done with raw `float*` arrays and hand-rolled arithmetic.

## Educational material

- [teaching_material.md](teaching_material.md) (~230 KB, ~2,800 lines): a 13-part low-level graphics primer covering GPU/CPU, the graphics pipeline, shaders, GLSL, buffers, attributes, uniforms, textures, framebuffers. Targets someone with engine experience (e.g., Unity) who is new to raw OpenGL.
- [teaching_material_rules.md](teaching_material_rules.md): the meta-guidelines for *writing* the teaching material (structure, audience calibration, code-sample policy).

> Note: the teaching material was last updated to reflect the codebase before the recent rename and shader-naming changes. Some specific identifiers it shows (`aPos`, `u_RangeMin`, `v_TexCoord`, references to `src/fluid_graphics/`) no longer match the source. The conventions it teaches are still correct.

## Critical files cheat-sheet

| File | What it contains | When to look here |
|---|---|---|
| [src/main.cpp](src/main.cpp) | Entry point, main loop, ImGui panel, glue code | "How does a frame work?" / "How is the UI wired?" |
| [src/graphics_engine.hpp](src/graphics_engine.hpp) | `graphics_engine` public surface, `vis_mode` enum | "What can the renderer do?" |
| [src/graphics_engine.cpp](src/graphics_engine.cpp) | GL init, shader compile, field & arrow rendering, ImGui setup | "How is something drawn?" |
| [src/fluid_physics/types.h](src/fluid_physics/types.h) | `FluidContext`, `ScenarioParams`, function-pointer typedefs | "What does the sim state look like?" |
| [src/fluid_physics/core.c](src/fluid_physics/core.c) | `fluid_step`, advection, diffusion, projection, pressure solvers | "How is the sim actually computed?" |
| [src/fluid_physics/scenarios.c](src/fluid_physics/scenarios.c) | The four scenarios | "What initial / boundary conditions does X use?" |
| [src/fluid_physics/boundaries.c](src/fluid_physics/boundaries.c) | BC helpers | "How is no-slip / inlet / outlet enforced?" |
| [src/fluid_physics/utilities.h](src/fluid_physics/utilities.h) | Indexing macros, Laplacian stencils | "What is `IX_U(i,j)` doing?" |
| [CMakeLists.txt](CMakeLists.txt) | Dependencies, build targets | "What's the build?" |
| [assets/shaders/field.frag](assets/shaders/field.frag) | Turbo colormap | "Where does the color come from?" |
| [AGENTS.md](AGENTS.md) | Coding standards (C++ + GLSL) | "How should new code look?" |
