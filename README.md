# GraphicsEngine

An interactive, real-time 2D incompressible fluid simulator with a live OpenGL viewer. The numerical core is C, the renderer and UI are C++17, and the hot loops are parallelized with OpenMP. Pressure projection, preconditioner, scenario, and physical parameters can all be changed at runtime from a Dear ImGui panel.

The physics engine lives under `src/fluid_physics/` as a Git submodule pointing at [atahancetindemir/fluid-graphics](https://github.com/atahancetindemir/fluid-graphics).

## Highlights

- 2D incompressible Navier–Stokes on a staggered MAC grid
- Four scenarios: Lid-Driven Cavity, Kármán Vortex Street, NACA 2412 Airfoil, Urban City
- Three pressure solvers (PCG, Red-Black Gauss-Seidel, SOR) swappable at runtime
- Three preconditioners (Identity, Jacobi, Multigrid placeholder) for PCG
- Five visualization modes: smoke, pressure, velocity magnitude, vectors, field + vectors
- OpenMP parallelism, Turbo colormap on an `R32F` texture

## How It Works

### Physics (C — `src/fluid_physics/`)

Each `fluid_step` performs the classical projection method on a MAC staggered grid:

1. Apply scenario sources (inlet velocity, body forces).
2. Diffuse velocity — explicit or implicit, chosen automatically from the diffusion number.
3. Advect velocity with semi-Lagrangian self-advection.
4. Compute divergence of the intermediate field.
5. Solve `∇²p = ρ/dt · ∇·u*` with the selected solver (PCG / RBGS / SOR).
6. Subtract `∇p` to project velocity onto the divergence-free subspace.
7. Apply boundary conditions (no-slip on the solid mask, inflow/outflow on domain edges).
8. Advect the smoke tracer.

Solvers and preconditioners are stored as function pointers on the `FluidContext`, so swapping them at runtime is a single assignment.

### Graphics (C++17 — `src/graphics_engine.{hpp,cpp}`)

- OpenGL 4.6 Core Profile with two shader programs.
- **Field shader**: full-screen quad reading an `R32F` texture, normalized against the current min/max and colormapped with a polynomial Turbo approximation.
- **Arrow shader**: line segments uploaded from a CPU-built vertex buffer (shaft + two head wings per arrow).
- Dear ImGui is layered on top via the GLFW + OpenGL3 backends and rebuilt each frame.

### Glue (`src/main.cpp`)

Per frame: recompute auto-omega if enabled → run N `fluid_step` substeps → draw the ImGui panel and honor any reset/rebuild requests → update the field texture and/or arrow buffer for the active mode → render and present.

## Tech Stack

| Component | Version / Notes |
|---|---|
| C / C++ | C11 / C++17 |
| CMake | 3.20+ |
| GLFW | 3.4 (via `FetchContent`) |
| Dear ImGui | v1.91.5 (via `FetchContent`) |
| GLAD | OpenGL 4.6 loader |
| OpenMP | linked when found |
| Fluid engine | [atahancetindemir/fluid-graphics](https://github.com/atahancetindemir/fluid-graphics) as a submodule |

## Project Layout

```
.
├── CMakeLists.txt
├── assets/shaders/        # field.vert/frag, arrow.vert/frag
├── include/               # glad, KHR headers
└── src/
    ├── main.cpp           # main loop + ImGui panel
    ├── graphics_engine.*  # OpenGL renderer
    ├── glad.c
    └── fluid_physics/     # submodule (C fluid engine)
```

## Getting Started

```bash
git clone --recursive https://github.com/<user>/GraphicsEngine.git
cmake -S . -B build -G Ninja
cmake --build build
./build/GraphicsEngine
```

If you cloned without `--recursive`, run `git submodule update --init src/fluid_physics` first. Launch from the repo root so `assets/shaders/` is reachable — the renderer loads shaders with relative paths. CLion opens the project out of the box.

## Updating the Fluid Engine

A submodule is pinned to a commit. To track the upstream `main`:

```bash
git submodule update --remote src/fluid_physics
git add src/fluid_physics
git commit -m "Update fluid_physics submodule"
```

This manual step is intentional — it prevents the engine from changing under you between builds.

## Credits

- Fluid engine: [atahancetindemir/fluid-graphics](https://github.com/atahancetindemir/fluid-graphics) by Atahan Çetindemir.
- [GLFW](https://www.glfw.org/), [Dear ImGui](https://github.com/ocornut/imgui), [GLAD](https://glad.dav1d.de/).
- [Turbo colormap](https://blog.research.google/2019/08/turbo-improved-rainbow-colormap-for.html) (Mikhailov, Google Research, 2019).
