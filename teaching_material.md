# Graphics Engine — Teaching Material

A slow walkthrough of how shaders and the OpenGL pipeline actually work, building from the hardware up.

---

## Table of Contents

### Part 1: The hardware foundation
- 1.1 CPU and GPU — what each is for
- 1.2 GPU memory — where data lives on the card
- 1.3 The full graphics pipeline — every stage

### Part 2: Kinds of shaders
- 2.1 The five graphics-pipeline shader stages
- 2.2 Compute shaders — the separate world

### Part 3: How shaders go from text to running on the GPU
- 3.1 The shader source file
- 3.2 Who actually compiles it, when, and where
- 3.3 The compile → link → activate flow
- 3.4 Why this happens at runtime, not build time
- 3.5 Errors are silent unless you ask
- 3.6 What GLSL looks like — a quick tour

### Part 4: How you talk to OpenGL
- 4.1 What OpenGL actually is
- 4.2 OpenGL's own type names — why they exist
- 4.3 OpenGL is a state machine
- 4.4 What "bound" and "active" mean
- 4.5 The two phases: set state, then act

### Part 5: What we are actually drawing
- 5.1 The atomic unit of geometry: the vertex
- 5.2 Primitives: how vertices combine into shapes
- 5.3 Coordinate spaces: from your numbers to screen pixels
- 5.4 What "drawing" means in the OpenGL sense

### Part 6: Buffers — where the data lives
- 6.1 What a buffer is
- 6.2 Buffer kinds and what each is for
- 6.3 The create-bind-upload pattern
- 6.4 Usage hints
- 6.5 What kinds of data fit in buffers
- 6.6 Vertex Buffer Objects (VBOs)
- 6.7 Index Buffer Objects (IBOs) and why we use them
- 6.8 Why a quad is 4 vertices and 6 indices

### Part 7: Vertex attributes — feeding data into the vertex shader
- 7.1 What an attribute is
- 7.2 Attribute slots — how many you have, why they're numbered
- 7.3 `glVertexAttribPointer` — every parameter
- 7.4 Stride and offset
- 7.5 The normalize flag
- 7.6 Enabling attributes
- 7.7 Vertex Array Objects (VAOs) — the recipe

### Part 8: The vertex shader (in depth)
- 8.1 What a vertex shader does
- 8.2 Inputs from attributes
- 8.3 User-declared outputs
- 8.4 Built-in outputs (`gl_Position`, others)
- 8.5 Built-in inputs (`gl_VertexID`, others)
- 8.6 The `gl_` prefix and naming conventions

### Part 9: The middle of the pipeline (fixed function)
- 9.1 Primitive assembly
- 9.2 Clipping
- 9.3 Perspective divide and viewport transform
- 9.4 Rasterization
- 9.5 Interpolation — what the rasterizer does to your `out` variables

### Part 10: The fragment shader (in depth)
- 10.1 What a fragment shader does
- 10.2 Inputs (interpolated varyings)
- 10.3 Built-in inputs (`gl_FragCoord`, others)
- 10.4 Outputs
- 10.5 Built-in outputs (`gl_FragDepth`)
- 10.6 Multiple outputs

### Part 11: Per-fragment operations and the framebuffer
- 11.1 The framebuffer
- 11.2 Depth testing
- 11.3 Stencil testing
- 11.4 Blending
- 11.5 Multiple color attachments and deferred rendering

### Part 12: Uniforms
- 12.1 What a uniform is
- 12.2 Uniform locations
- 12.3 Setting uniforms from C++
- 12.4 When uniforms are read

### Part 13: Textures and samplers
- 13.1 What a texture is on the GPU
- 13.2 Texture units — what they are, how many
- 13.3 Binding a texture to a unit
- 13.4 Samplers and how they reference units
- 13.5 The `texture()` function
- 13.6 Filtering
- 13.7 Wrapping

### Part 14: GLSL — the language
- 14.1 Scalar and vector types
- 14.2 Vector construction
- 14.3 Swizzles
- 14.4 Built-in math functions

### Part 15: Practical
- 15.1 Walkthrough: setting up the quad and its texture
- 15.2 Walkthrough: the field shaders
- 15.3 Walkthrough: drawing one frame
- 15.4 Common pitfalls
- 15.5 Debugging shaders
- 15.6 How to modify safely

---

# Part 1: The hardware foundation

## 1.1 CPU and GPU — what each is for

Your computer has two main processors that matter for this conversation. They're physically separate chips with separate memory, and they're built for different jobs.

**The CPU** has a small number of cores — typically 8, 16, or 32. Each core is *complex and powerful*:

- It can execute different instructions than its neighbors at the same time.
- It has lots of caching and branch prediction.
- It's good at code that has lots of `if`s, loops, and unpredictable paths.

**The GPU** has thousands of cores — sometimes 5,000+ on a modern card. But each core is *simple*:

- It's designed to do the *same operation* as its neighbors, on *different data*.
- It has minimal branch prediction.
- It's built for code where you say "do this exact same calculation 1,000,000 times on 1,000,000 different inputs."

This is the fundamental tradeoff: the CPU is a few generalists, the GPU is a huge crowd of specialists.

A concrete example. Suppose you want to color every pixel of an 800×800 image — that's 640,000 pixels.

- The CPU would do this with a loop: pixel 0, pixel 1, pixel 2, ... pixel 639,999. One after another (or with a few cores splitting the work).
- The GPU does it in big batches in parallel: hundreds of pixels at once, all running the same coloring code on different pixel positions.

For this kind of "same operation on lots of independent data" workload, the GPU is dramatically faster than any CPU. For other workloads (a database server, a compiler), the CPU wins.

When we render a frame, we want to harness the GPU because we're doing the same coloring math on hundreds of thousands of pixels.

## 1.2 GPU memory — where data lives on the card

The GPU **does not share memory with the CPU**. It has its own RAM, called **VRAM** (Video RAM). On a desktop with a dedicated graphics card, VRAM is physically on that card. (Integrated GPUs share system RAM, but the OS still separates a region for graphics and you can mostly think of it as separate.)

This separation is important because: **the GPU can only directly use data that's in VRAM.** If your C++ code has a `float[]` array sitting in regular RAM, the GPU literally cannot see it. You have to copy that data into VRAM first.

Things that live in VRAM:

- **Vertex buffers** — the raw vertex data (positions, texture coordinates, etc.). What you fill with `glBufferData(GL_ARRAY_BUFFER, ...)`.
- **Index buffers** — the lists of which vertices form which triangles.
- **Textures** — 2D images.
- **Framebuffers** — images the GPU draws into. The default framebuffer is the screen; you can also create your own (covered in Part 11).
- **Compiled shader programs** — yes, the shader code itself ends up loaded into the GPU.
- **Uniform values** — small named constants you set per draw call.

(There are several *kinds* of buffers, each playing a specific role — full coverage in Part 6.)

When your C++ code calls `glBufferData(...)`, what really happens is "copy this data from system RAM into VRAM." That copy isn't free — it has to travel over a bus (called PCIe on most desktops) which is much slower than VRAM-internal access. So the convention is: **copy data once, draw with it many times.** That's why we set up the quad's vertices in the constructor and never touch them again.

The picture in your head should be:

```
[CPU + system RAM]  <---PCIe bus--->  [GPU + VRAM]
   your C++ code                       buffers
   pointers, structs                   textures
                                       shader programs
                                       framebuffers
```

Every OpenGL call you make is essentially you, the CPU side, sending a command across that bus to tell the GPU what to do. (We'll cover what *OpenGL* itself actually is in Part 4.)

## 1.3 The graphics pipeline — the map

When you tell the GPU to draw something, it runs through a series of stages. Two of those stages are programs you write (the **vertex shader** and the **fragment shader**). The rest are fixed hardware that runs automatically.

This is the simplified map you should hold in your head:

```
                     ┌────────────────────────────────────────┐
                     │ CPU                                    │
                     │   glDrawElements(...) issued           │
                     └─────────────────┬──────────────────────┘
                                       │ command sent to GPU
                                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│ GPU                                                                  │
│                                                                      │
│   1. Vertex fetch                                                    │
│      Reads vertex data from a buffer.                                │
│                                                                      │
│   2. VERTEX SHADER                          ← you write this         │
│      Runs once per vertex.                                           │
│                                                                      │
│   3. Optional shader stages                                          │
│      Tessellation, geometry. We skip these.                          │
│                                                                      │
│   4. Primitive assembly                                              │
│      Groups vertices into shapes — triangles, lines, or points.      │
│                                                                      │
│   5. Rasterization                                                   │
│      For each shape, finds which screen pixels it covers.            │
│      Produces one "fragment" (a pixel-sized piece) per covered pixel.│
│                                                                      │
│   6. FRAGMENT SHADER                        ← you write this         │
│      Runs once per fragment.                                         │
│                                                                      │
│   7. Framebuffer write                                               │
│      The fragment's color becomes a pixel on the screen.             │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

The takeaway:

- You only write **two stages** of the pipeline (vertex and fragment shaders). The rest run automatically.
- A draw call runs **the entire pipeline once** for the geometry you specified.
- **Compute shaders** are not in this pipeline at all — they run separately. Covered in Part 2.
- Between these numbered stages there are also small fixed-function steps (clipping, perspective divide, and viewport transform between the vertex shader and primitive assembly; depth test, stencil test, and blending after the fragment shader). They run automatically. The middle ones are covered in Part 9; the post-fragment ones are covered in Part 11.
- We'll come back to each stage in its own dedicated Part later. For now, just have this shape in your head.

---

# Part 2: Kinds of shaders

Earlier I implied "there are two kinds of shaders." That was a simplification. There are actually **five shader stages inside the graphics pipeline**, plus **compute shaders** as a sixth type that runs outside the graphics pipeline entirely.

You only have to write the ones you need. Most simple programs (including ours) write two of them. But the others exist and you should know what they do.

## 2.1 The five graphics-pipeline shader stages

Listed in the order they run:

```
Vertex shader                       (required)
   │
   ▼
Tessellation control shader         (optional)
   │
   ▼
[hardware tessellator]              (fixed-function, runs only if tessellation shaders are present)
   │
   ▼
Tessellation evaluation shader      (optional)
   │
   ▼
Geometry shader                     (optional)
   │
   ▼
[primitive assembly + rasterization]
   │
   ▼
Fragment shader                     (required for visible output)
```

The items in `[brackets]` are **fixed-function** steps — not shader stages. You don't program them; the GPU runs them automatically between the shader stages. They appear here so you see the complete pipeline, but each one is briefly:

- **Hardware tessellator** — sits between the tessellation control and evaluation shaders. Mass-creates new vertices according to the subdivision levels the control shader chose.
- **Primitive assembly** — groups vertices into shapes (triangles, lines, or points) based on what the draw call asked for.
- **Rasterization** — for each shape, figures out which screen pixels it covers. Generates one "fragment" (a pixel-sized piece) per covered pixel.

Full coverage of these fixed-function steps is in Part 9.

### Vertex shader (required)

- Runs once per **vertex** (one input data point — full coverage in Part 5).
- Job: take that vertex's input data and produce its position, plus any extra values to pass downstream.
- Used for: every drawing operation — there is no rendering without it.
- **We use this.** Ours just passes the position through.

### Tessellation control shader (optional)

- Runs once per **patch** — a small group of "control points" that defines a region to subdivide, like the corners of a curved surface.
- Job: decide how finely to subdivide that patch.
- Used for: smooth curves from few input points; level of detail (more triangles for close objects, fewer for far ones).
- **We don't use this.** Our quad is flat geometry that needs no subdivision.

### Tessellation evaluation shader (optional)

- Runs once per vertex generated by the hardware tessellator (a fixed-function step that runs between the two tessellation shaders and mass-creates the new vertices).
- Job: place each newly generated vertex in space.
- Used for: works alongside the tessellation control shader for the same applications.
- **We don't use this.**

### Geometry shader (optional)

- Runs once per **primitive** (a primitive is one shape unit being drawn — a triangle, line, or point — full coverage in Part 5).
- Job: take one primitive in, output zero or more primitives. Can multiply, delete, or replace primitives.
- Used for: generating quad sprites from points; particle systems; shadow volumes.
- **We don't use this.** Modern engines often skip it because compute shaders or "instanced rendering" can do similar things faster.

### Fragment shader (required for visible output)

- Runs once per fragment (pixel-sized piece of a primitive after rasterization).
- Job: compute the color of that pixel, plus optionally its depth.
- Used for: every drawing operation that puts pixels on the screen.
- **We use this.** Ours samples the field texture and applies the colormap.

### Why our project only has two

Because we draw a **flat 2D quad with a texture on it**. No subdivision needed (vertex shader gives final positions directly), no primitive generation needed (the four corners are exactly the geometry we want). Tessellation and geometry stages have nothing to do, so we skip them.

In modern engines, even complex 3D scenes often only use vertex + fragment. Tessellation and geometry shaders are powerful but specialized.

## 2.2 Compute shaders — the separate world

Compute shaders are different from everything above. They are **not part of the graphics pipeline**. They don't take vertices in or produce pixels out. They're for **general-purpose computation on the GPU**.

You launch a compute shader with a single C++ call:

```cpp
glDispatchCompute(...);
```

No draw call, no triangles, no rasterization. The GPU runs your compute shader many times in parallel, then stops.

### What compute shaders can do that the others can't

Inside a compute shader, you can:

- **Read from any buffer or texture** that you've made available.
- **Write to any buffer or texture.** This is the big difference from vertex/fragment shaders, which are very constrained about what they output.

So instead of "vertex shader produces `gl_Position`" or "fragment shader produces `FragColor`," a compute shader is "run this code in parallel and you choose what it reads and writes."

### When to use compute shaders

Anything that needs GPU parallelism but isn't "draw a triangle":

- **Image processing**: blur, sharpen, color correction.
- **Physics simulations**: cloth, particles, fluids.
- **Post-processing effects**: bloom, depth-of-field.
- **General number-crunching**: any code that does the same operation on lots of independent data.

### Are we using compute shaders?

**No.** Our fluid simulation runs on the **CPU** with OpenMP (multiple CPU cores cooperating). Each `fluid_step()` is a normal C function — diffusion, advection, pressure solve — running on CPU cores.

If we wanted to make the simulation much faster, we could rewrite each step as a compute shader, keep the velocity/pressure/smoke fields in GPU buffers, and run thousands of GPU cores on the math. That's a major project and we're not doing it. But it's why compute shaders matter in real engines: they let you keep your data on the GPU and process it there.

## Summary

The count of shader types in modern OpenGL:

- **Five graphics pipeline shader stages**: vertex, tessellation control, tessellation evaluation, geometry, fragment.
- **One separate type**: compute.
- **Required for drawing geometry**: vertex + fragment. Everything else is optional.

---

# Part 3: How shaders go from text to running on the GPU

## 3.1 The shader source file

A shader is a **text file** containing **GLSL** code. GLSL stands for *OpenGL Shading Language* — it's the language you write shaders in, similar to C in syntax but specialized for running on the GPU. Your C++ compiler does not understand GLSL. Your build system does not compile it. As far as your build is concerned, those `.vert` and `.frag` files are just text files sitting in your `assets/shaders/` folder.

When your program runs, your C++ code:

1. Opens the `.vert` and `.frag` files.
2. Reads their contents into a `std::string`.
3. Hands the string to OpenGL with the message "compile this for me."

That's what our renderer's shader-compile helper does. It reads the file into a string; the rest is OpenGL calls.

So the shader source travels as **text** from your disk to the function that compiles it. The C++ compiler never sees the GLSL.

## 3.2 Who actually compiles it, when, and where

The **graphics driver** is software that ships with your GPU. It's written by NVIDIA / AMD / Intel / etc. It runs on the CPU as part of your process when you make OpenGL calls. The driver is what implements `glCompileShader`, `glLinkProgram`, `glDrawElements`, and every other `gl*` function.

When you call `glCompileShader(handle)`:

1. The driver takes the GLSL source string you previously attached.
2. It parses, type-checks, and optimizes the GLSL.
3. It translates it into **GPU machine code** — instructions specific to your particular GPU model. NVIDIA's machine code is different from AMD's, which is different from Intel's.
4. It stores the compiled machine code in VRAM, ready for the GPU to execute later.

All of this happens on the **CPU**, inside the driver. The GPU is uninvolved in compilation. The GPU only enters the picture later, when you actually draw and it runs the compiled code.

So the precise statement is: **your C++ code triggers compilation by calling `glCompileShader`, which is implemented by the graphics driver, which runs on the CPU and produces GPU machine code.**

## 3.3 The compile → link → activate flow

A shader program isn't ready to use after one `glCompileShader` call. There's a multi-step ritual. Here's the full sequence:

```cpp
// 1. Create empty shader objects
unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);

// 2. Attach the source code (driver doesn't compile yet)
glShaderSource(vert, 1, &vSrc, nullptr);
glShaderSource(frag, 1, &fSrc, nullptr);

// 3. Compile each one (driver compiles to GPU machine code now)
glCompileShader(vert);
glCompileShader(frag);

// 4. Create a program object — this is what the GPU actually runs
unsigned int program = glCreateProgram();

// 5. Attach the compiled shaders to the program
glAttachShader(program, vert);
glAttachShader(program, frag);

// 6. Link them together
glLinkProgram(program);

// 7. The individual shader objects aren't needed anymore
glDeleteShader(vert);
glDeleteShader(frag);

// (later, when drawing:)
glUseProgram(program);  // 8. activate this program for subsequent draw calls
```

Two things are happening here that you might not expect:

### Why "compile" and "link" are separate steps

Compilation is per-shader. The driver compiles the vertex shader and the fragment shader **independently** — it doesn't yet know they'll be used together.

Linking is what connects them. The driver checks:

- Every `out` variable in the vertex shader has a matching `in` variable in the fragment shader (same name, same type).
- Uniforms with the same name in both shaders have consistent types.
- All required outputs are written.

If something doesn't match (typo in a varying name, mismatched types, etc.), linking fails — even though both shaders compiled fine on their own. The error appears at link time, not compile time.

After linking, the driver produces a final executable: a single piece of GPU machine code that runs the whole pipeline (vertex stage + fragment stage) as one thing.

### Why we delete the individual shaders after linking

Once `glLinkProgram` succeeds, the program object holds its own copy of the compiled code. The individual `vert` and `frag` shader objects are no longer needed — they were just stepping stones. `glDeleteShader` frees them.

The `program` is what survives and is what `glUseProgram` activates.

## 3.4 Why this happens at runtime, not build time

You might wonder: why not compile shaders ahead of time, like normal C++ code, and ship pre-compiled shaders with the program?

The reason is that **GPU machine code is GPU-specific**. NVIDIA, AMD, Intel, Apple, ARM Mali — they all have different instruction sets. A shader compiled for an NVIDIA RTX 4080 cannot run on an AMD card, and vice versa. There is no universal GPU instruction set the way `x86-64` is universal for desktop CPUs.

So the only way for one program to work across all GPUs is:
- Ship the shader **source code** (GLSL text).
- Let the user's driver compile it for their specific GPU at startup.

That's why the compilation has to happen at runtime, in the driver, on the user's machine.

(Side note: there are newer schemes that try to fix this. Vulkan ships shaders in **SPIR-V**, an intermediate representation that's already partially compiled — closer to the GPU but not specific to any one vendor. The driver still does the final translation to vendor machine code, but it's faster than parsing GLSL from scratch. OpenGL has a "binary shaders" feature too, but it's optional and rarely used.)

## 3.5 Errors are silent unless you ask

Important practical note: `glCompileShader` and `glLinkProgram` **don't crash or print** if they fail. Your draw call just becomes a no-op, the screen stays black, and you have no idea why.

To see the error, you have to ask:

```cpp
GLint ok;
glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
if (!ok) {
    char log[1024];
    glGetShaderInfoLog(vert, sizeof(log), nullptr, log);
    fprintf(stderr, "Vertex shader compile error:\n%s\n", log);
}

// And for the program after linking:
glGetProgramiv(program, GL_LINK_STATUS, &ok);
if (!ok) {
    char log[1024];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    fprintf(stderr, "Program link error:\n%s\n", log);
}
```

Our renderer doesn't do this error-checking currently — which is fine while the shaders are tiny and known to work, but the moment you start writing your own shaders this is the first thing to add.

## 3.6 What GLSL looks like — a quick tour

Before later Parts start showing shader code, here's just enough GLSL syntax to read it. Full coverage of the language is in Part 14; this section is a primer for the parts that aren't C-like.

### Mostly C-like

GLSL was designed to be easy to learn for C/C++ programmers. The following are all the same as in C:

- Function declarations: `vec3 myFunction(float x) { ... }`.
- Control flow: `if`, `else`, `for`, `while`, `return`, `break`, `continue`.
- Operators: `+ - * / % == != < <= > >= && || !`.
- Comments: `//` and `/* ... */`.
- Structs: `struct Material { vec3 color; float roughness; };`.

If you can read C, you can read most of any GLSL shader.

### Vector and matrix types are first-class

Where GLSL diverges from C is its built-in support for vectors and matrices:

- **`vec2`, `vec3`, `vec4`** — 2-, 3-, 4-component float vectors.
- **`mat2`, `mat3`, `mat4`** — 2×2, 3×3, 4×4 float matrices.
- Integer / boolean variants exist (`ivec3`, `bvec2`, etc.); used less often.

These types appear constantly in shader code — positions, colors, texture coordinates, transformation matrices.

### Vector construction

Vectors are built with constructor expressions:

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);    // explicit components
vec3 b = vec3(0.5);               // splat — (0.5, 0.5, 0.5)
vec4 c = vec4(a, 1.0);            // extend a vec3 with one float — (1, 2, 3, 1)
vec4 d = vec4(0.0, vec3(1.0));    // mix scalars and smaller vectors
```

The "extend" form is everywhere in shader code. A line like `gl_Position = vec4(aPos, 0.0, 1.0)` builds a 4-component clip-space position from a 2D position by appending `z = 0` and `w = 1`.

### Swizzles — picking and reordering components

A swizzle is shorthand for building a new vector from components of an existing one:

```glsl
vec4 v = vec4(1.0, 2.0, 3.0, 4.0);

vec2 a = v.xy;     // (1, 2)
vec2 b = v.yx;     // (2, 1) — swapped
vec3 c = v.zyx;    // (3, 2, 1) — reversed
vec3 d = v.rgb;    // first three (color naming, same as .xyz)
```

There are three interchangeable name sets: `.xyzw` (positional), `.rgba` (color), `.stpq` (texture coord). Pick whichever reads best for the role of the vector. We'll see swizzles used in shaders to swap, splat, or extract components.

### Operators on vectors are component-wise — except matrix × vector

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);
vec3 b = vec3(10.0, 20.0, 30.0);

vec3 sum  = a + b;       // (11, 22, 33)
vec3 prod = a * b;       // (10, 40, 90) — element-wise multiply, NOT a dot product
```

A `vec * vec` is component-wise — not a dot product. For dot product, use `dot(a, b)`.

A `mat4 * vec4` is real matrix multiplication; you'll see this in 3D transformation code: `gl_Position = projection * view * model * vec4(...)`.

### Built-in math functions

GLSL has a rich math library — `clamp`, `mix`, `smoothstep`, `length`, `normalize`, `dot`, `pow`, `sqrt`, `sin`, `cos`, and many more. Most apply component-wise to vectors. Full reference in Part 14.

### Naming conventions

By project convention (used in this material):

- `a_` or `a` prefix → an attribute (vertex shader input from a VBO).
- `v_` or `v` prefix → a varying (vertex `out` → fragment `in`).
- `u_` or `u` prefix → a uniform.
- `gl_` prefix → reserved built-in (e.g. `gl_Position`, `gl_FragCoord`).

Not enforced by the language; just a useful signal in source code.

### That's enough to read shader code

This much background lets you parse any shader source you'll see in later Parts. The deep coverage — the full math library, all type variants, edge cases — comes in Part 14.

## Summary

- A shader is a text file. The C++ compiler doesn't touch it.
- At runtime, your C++ code reads the text and hands it to the **graphics driver**, which compiles it on the CPU into GPU-specific machine code.
- The compiled code is then **linked** with other shaders into a **program**, which is what the GPU actually runs.
- This all happens at runtime because every GPU vendor has different machine code; there's no portable GPU binary.
- Compilation and linking errors are silent unless you query the status — always add error checking once you write your own shaders.

---

# Part 4: How you talk to OpenGL

So far we've talked about *what the GPU is* (Part 1), *what shaders do* (Part 2), and *how shaders go from text to running on the GPU* (Part 3). What we haven't covered is *how your C++ code actually drives all of this* — how you load buffers, set uniforms, trigger draws. The answer is: through OpenGL — and OpenGL has a particular style that's different from most libraries you've used. You need this style as a foundation before any concrete details (buffers, attributes, etc.) make sense.

## 4.1 What OpenGL actually is

OpenGL is **not a library you ship with your program**. It's not a `.dll` or `.so` file you download. OpenGL is a **specification** — a written document that defines a list of functions, what arguments they take, and what they're supposed to do.

The actual *implementation* of those functions is provided by your **graphics driver**, which is software that ships with your GPU (written by NVIDIA, AMD, Intel, Apple, and so on). When your C++ code calls `glDrawElements(...)`, you're calling a function that the driver implements.

So the layering is:

```
Your C++ code
     │ calls
     ▼
OpenGL function (e.g., glDrawElements)
     │ implemented by
     ▼
Graphics driver (CPU-side software, vendor-specific)
     │ commands
     ▼
GPU hardware
```

Two practical consequences of this design:

**One: function loading.** On Windows, the only OpenGL functions exposed by the system header are very old ones (OpenGL 1.1). Modern OpenGL functions must be looked up at runtime, by asking the driver "give me a function pointer for `glDrawElements`." This is what libraries like **GLAD** (which we use) do for you. They do this lookup once at program startup, then store the function pointers in global variables so your code can call them normally.

**Two: behavior depends on the driver.** Two computers with different GPUs run completely different OpenGL implementations. They should behave the same according to the spec, but in practice there are small bugs and differences between drivers.

## 4.2 OpenGL's own type names — why they exist

When you read OpenGL code, you'll see types like `GLuint`, `GLint`, `GLfloat`, `GLboolean`, `GLenum`, `GLsizei`. These are typedefs that OpenGL defines in its headers. Plain C++ types like `int` and `float` would seem to do the same job — so why bother?

Three reasons stacked together:

### Reason 1: OpenGL predates standard fixed-width types

OpenGL was designed in the early 1990s. C didn't have `int32_t` or `uint32_t` back then — those came in C99 (1999). Worse, on different platforms a plain `int` could be 16 or 32 bits, a `long` could be 32 or 64. OpenGL needs **exact** sizes for binary protocol stability and cross-machine consistency. So it defined its own typedefs with guaranteed widths:

- `GLint` is always 32 bits.
- `GLshort` is always 16 bits.
- `GLfloat` is always 32-bit IEEE float.

Regardless of compiler or platform, the widths are fixed by spec.

### Reason 2: cross-language portability

OpenGL is a C API but has bindings in many other languages — Rust, Python, Java, JavaScript, etc. Each has its own native type sizes. The OpenGL type names give every language binding a clear contract: "this parameter is exactly 32 bits unsigned, no matter what your language calls a similarly-sized type."

### Reason 3: self-documenting code

A function parameter typed `GLenum` clearly takes an OpenGL constant like `GL_TRIANGLES`. A parameter typed `unsigned int` could be anything. The named types signal intent at the call site.

### The common types

| OpenGL type | Underlying C type | Purpose |
|---|---|---|
| `GLint` | `int` (32-bit) | Signed 32-bit integer |
| `GLuint` | `unsigned int` (32-bit) | Unsigned 32-bit integer |
| `GLshort`, `GLushort` | `short`, `unsigned short` | 16-bit integer |
| `GLbyte`, `GLubyte` | `signed char`, `unsigned char` | 8-bit integer |
| `GLfloat` | `float` | 32-bit IEEE float |
| `GLdouble` | `double` | 64-bit IEEE float |
| `GLboolean` | `unsigned char` (**not** C++ `bool`) | 0 = `GL_FALSE`, 1 = `GL_TRUE` |
| `GLenum` | `unsigned int` (32-bit) | OpenGL constants like `GL_TRIANGLES`, `GL_FLOAT` |
| `GLbitfield` | `unsigned int` (32-bit) | Bit-flag combinations like `GL_COLOR_BUFFER_BIT` |
| `GLsizei` | `int` (32-bit) | Sizes and counts |

### The `GLboolean` gotcha

This one is worth singling out. **`GLboolean` is *not* a C++ `bool`.** It's typedef'd as `unsigned char` (1 byte), with values `GL_TRUE` (= 1) and `GL_FALSE` (= 0). C++'s `bool` is a separate type whose size is implementation-defined.

In practice you can usually pass `true` and `false` from C++ where `GLboolean` is expected, and the compiler will convert. But the OpenGL convention is to use `GL_TRUE` and `GL_FALSE`. That's why functions like `glVertexAttribPointer` (Part 7.3) take `GL_FALSE` rather than `false`.

### In practice

On most modern desktops:

- `GLuint` ≡ `unsigned int`
- `GLint` ≡ `int`
- `GLfloat` ≡ `float`
- `GLboolean` ≡ `unsigned char`

So plain C++ values usually work where OpenGL types are expected. Using the OpenGL type names in your own code is good practice anyway — your code documents itself, and stays correct on any platform.

## 4.3 OpenGL is a state machine

The most important concept about OpenGL: **OpenGL is a state machine.**

That means OpenGL keeps a large internal bag of settings — call it "current state" — that persists across function calls. Most OpenGL functions don't do real work directly. They just modify one slot in that state. The real work happens only when you call a draw function, which uses whatever state is currently active.

Picture the state as a control panel with many slots:

- One slot for "currently active shader program."
- One slot for "currently active VAO" (a recipe object — covered in Part 7).
- A slot for each kind of buffer (one for `GL_ARRAY_BUFFER`, one for `GL_ELEMENT_ARRAY_BUFFER`, etc.) saying which buffer is in that role right now.
- One slot for "currently active texture unit," and per unit a slot for the texture sitting on it.
- Slots for the viewport rectangle, the clear color, depth-test settings, blend settings, and many more.

You set these slots one at a time. They persist until you change them. Then you trigger a draw, and it uses whatever is currently in the slots.

Concretely, drawing one piece of geometry looks like this:

```cpp
glUseProgram(myProgram);            // set the active shader program slot
glBindVertexArray(myVAO);           // set the active VAO slot
glDrawElements(GL_TRIANGLES,        // trigger — uses both slots above
               6, GL_UNSIGNED_INT, nullptr);
```

The first two calls don't draw anything; they just change slots. Only `glDrawElements` actually triggers GPU work, and it consumes whatever the current slot values are. This pattern — set a few slots, then trigger — runs through every OpenGL program ever written. We'll cover the full version in 4.5.

Why this design? It's older than most modern APIs — when OpenGL was designed in the early 1990s, this was a common style. It also avoids passing many parameters to every draw call: instead of "draw this VAO with this program with these uniforms with this texture..." you set those things once, then just say "draw." If they don't change between draws, you don't reset them.

## 4.4 What "bound" and "active" mean

Throughout OpenGL documentation (and the rest of this material), you'll see phrases like "the currently bound VAO," "the active program," "the bound buffer." They all mean the same thing: **whichever object is in the matching state slot right now.**

The functions that set those slots usually have names starting with `glBind*`, `glActive*`, or `glUse*`:

- `glUseProgram(program)` — set the active shader program slot.
- `glBindVertexArray(vao)` — set the active VAO slot.
- `glBindBuffer(target, buffer)` — set the slot for the given buffer target.
- `glActiveTexture(GL_TEXTURE0)` — set which texture unit is currently active.
- `glBindTexture(target, tex)` — on the currently active unit, set the slot for the given target.

Calling any of these does not draw, does not compute, does not read pixels. It only changes a slot in the state machine.

A consequence: a buffer has no fixed "type" of its own. The same buffer becomes a vertex buffer when bound to `GL_ARRAY_BUFFER`, an index buffer when bound to `GL_ELEMENT_ARRAY_BUFFER`, a copy source when bound to `GL_COPY_READ_BUFFER`. The bytes never moved; only its role in the state changed. (Full coverage of buffer types is in Part 6.)

## 4.5 The two phases: set state, then act

Because of the state machine, every interaction with OpenGL splits into two phases:

**Phase 1 — set state.** Some number of `glBind*` / `glUse*` / `glUniform*` calls that prepare the slots.

**Phase 2 — issue a real action.** One `glDraw*` call (or `glClear`, or `glDispatchCompute` for compute shaders) that actually triggers the GPU to do work.

A typical "draw one object" sequence:

```cpp
// Phase 1 — set state
glUseProgram(myProgram);                    // active program       ← myProgram
glBindVertexArray(myVAO);                   // active VAO           ← myVAO
glActiveTexture(GL_TEXTURE0);                // active texture unit  ← 0
glBindTexture(GL_TEXTURE_2D, myTex);         // unit 0's 2D slot     ← myTex
glUniform1i(samplerLoc, 0);                  // tell shader sampler to use unit 0
glUniform1f(otherLoc, someValue);            // upload another uniform

// Phase 2 — act
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
```

The order of the Phase 1 calls doesn't matter — they're just slot assignments. What matters is that **every relevant slot is set correctly by the time you reach the draw call**.

State **persists**. If you don't change a slot, it keeps whatever you last put in it. So a second draw can skip whatever didn't change.

This is why most OpenGL programs have a "bind, bind, bind, draw, bind, bind, draw" rhythm. You're not redoing work every time — you're just updating the slots that need to change since the last draw.

## Summary

- **OpenGL is a specification.** Your graphics driver implements it; your code calls into the driver via OpenGL function pointers (loaded at startup by libraries like GLAD).
- **OpenGL is a state machine.** A big bag of slots that persist across calls.
- **"Bound" / "active"** means "in the matching state slot right now."
- Most OpenGL calls just change a slot. Real GPU work happens on `glDraw*`, `glClear`, and a few others.
- The pattern is always **set state, then act**. State persists; you only re-set what changed.

---

# Part 5: What we are actually drawing

Part 4 covered *how* you talk to OpenGL — the state-machine model and the bind-then-act pattern. Before we put real things into those state slots (buffers, attributes, programs), we need to be precise about what we are even feeding the GPU. So this Part is about the shapes themselves: what a vertex really is, how vertices form shapes, and what coordinates we use to describe where those shapes go.

## 5.1 The atomic unit of geometry: the vertex

A **vertex** is the smallest unit of geometry you send to the GPU. The GPU does not understand "draw a circle" or "draw a square." It only understands vertices and the rules for connecting them.

But here is the subtle part: **a vertex is not just a position.** A vertex is a *bundle of values associated with one point*. You decide what values are in that bundle.

A minimal vertex has just one value:

```
Vertex { position }
```

A more useful vertex might carry more:

```
Vertex { position, texture coordinate }
```

Or:

```
Vertex { position, color }
```

Or, for 3D lighting:

```
Vertex { position, normal vector, texture coordinate }
```

Each value attached to a vertex is called an **attribute**. When you set up a buffer of vertex data, you're filling it with these bundles. When the vertex shader runs, it reads one bundle and computes whatever it needs for that one point.

So a vertex is not "a 3D point." A vertex is "a point that carries some attributes you chose."

### What attributes can a vertex have?

Anything you want, as long as you tell the vertex shader to expect it. Common ones:

| Attribute | What it's used for |
|---|---|
| Position | Where the vertex is in space. **Almost always present.** |
| Texture coordinate | Where this vertex maps onto a texture image. |
| Color | A per-vertex color. |
| Normal | The "outward direction" at this vertex. Used for lighting. |
| Tangent / bitangent | Used for advanced lighting (normal mapping). |
| Bone weights, custom data | For animation, particle systems, anything else. |

For our project, our quad's vertices have **two** attributes: a 2D position and a 2D texture coordinate. That's it. We don't need normals, colors, or anything else, because we're just stretching a texture over a flat shape.

### One key idea: the vertex shader runs once per vertex

Whatever attributes you give a vertex, the vertex shader sees them all and runs *once* for that vertex. It can read them, do math on them, and produce an output. Then it moves on to the next vertex.

This is why the vertex shader is "per-vertex": it cannot see neighboring vertices, only its own attributes.

## 5.2 Primitives: how vertices combine into shapes

A vertex by itself is just a point in space. To make a **shape**, you tell the GPU "treat groups of these vertices as triangles" (or lines, or points).

The grouping rule is called the **primitive type**, and you choose it when you call the draw function:

```cpp
glDrawArrays(GL_TRIANGLES, ...);
glDrawElements(GL_TRIANGLES, ...);
```

That `GL_TRIANGLES` says "every 3 vertices form one triangle."

The main primitive types:

| Type | Meaning |
|---|---|
| `GL_POINTS` | Each vertex is its own pixel-sized dot. |
| `GL_LINES` | Each pair of vertices forms one line segment. |
| `GL_LINE_STRIP` | Vertices form a connected chain of line segments. |
| `GL_TRIANGLES` | Every 3 vertices form one triangle. |
| `GL_TRIANGLE_STRIP` | Each new vertex extends the previous triangle by reusing the last two. |
| `GL_TRIANGLE_FAN` | All triangles share the first vertex like slices of a pie. |

For our project we use:
- `GL_TRIANGLES` to draw the quad as two triangles.
- `GL_LINES` to draw arrow shafts and arrowheads.

### Why triangles?

Triangles are the universal building block of solid surfaces in 3D graphics, for three reasons:

1. **Three points always lie on a single flat plane.** This makes the math for rasterization (figuring out which pixels are inside) simple and unambiguous. A polygon with 4+ vertices could be non-flat, which is messy.
2. **Triangles can tile any 2D or 3D shape.** A quad is two triangles. A cube is twelve triangles. A complex character mesh is millions of triangles. There's no shape you can't approximate with triangles.
3. **GPU hardware is built around triangles.** Rasterizers, depth tests, and interpolators — they're all designed for triangles. Drawing triangles is the most efficient operation a GPU can do.

In modern OpenGL there is no "draw a quad" command. Quads don't exist. To draw a rectangular region, you draw two triangles that together cover it.

## 5.3 Coordinate spaces: from your numbers to screen pixels

When you write a vertex's position, you're writing numbers in *some* coordinate system. There are several coordinate systems involved between the numbers you write and the pixel that lights up. You need to know what they are and which one you're working in at each step.

### The full chain (for 3D rendering)

Real 3D engines transform vertex positions through several spaces:

```
Object space   →  World space  →  View space  →  Clip space
(local)            (placed)        (camera-       (after
                                    relative)      projection)

Then the GPU does, automatically:

Clip space  →  NDC  →  Window space
                            (pixels)
```

What each space means:

1. **Object space (also "model space" or "local space").** Where the mesh's vertices are defined, relative to the mesh's own origin. A teapot mesh has its vertices around (0, 0, 0).

2. **World space.** After multiplying by a *model matrix*, the mesh is placed somewhere in the larger world. The teapot is now sitting on a table at (5, 0, 3).

3. **View space (also "camera space" or "eye space").** After multiplying by a *view matrix*, everything is now relative to the camera. The teapot is in front of the camera.

4. **Clip space.** After multiplying by a *projection matrix*, the world has been distorted so that things in the camera's view fit into a standard cube the GPU can clip against. Clip space coordinates are 4D: `(x, y, z, w)`.

The vertex shader's job in 3D is to take a vertex in object space and output it in **clip space**. The output is written to a special built-in variable called `gl_Position` — that's the channel the GPU reads to learn where the vertex lands (full coverage in Part 8). For 3D, you compute `gl_Position` by multiplying through the chain:

```
gl_Position = projection * view * model * vec4(local_position, 1.0);
```

5. **NDC (Normalized Device Coordinates).** After the vertex shader outputs clip-space `(x, y, z, w)`, the GPU automatically divides `xyz` by `w`. This is called the **perspective divide**. The result is NDC, where:
   - `x` is in `[-1, +1]`, with `-1` at the left edge of the screen, `+1` at the right.
   - `y` is in `[-1, +1]`, with `-1` at the bottom, `+1` at the top.
   - `z` is in `[-1, +1]`, with `-1` near and `+1` far.

   **NDC is normalized.** Every pixel on the screen, no matter the resolution, is somewhere inside this `[-1, +1]³` cube.

6. **Window space (also "screen space").** Finally, the GPU applies the **viewport transform** (using the values you set with `glViewport(x, y, width, height)`) to convert NDC into actual pixel coordinates. NDC `(-1, -1)` becomes the bottom-left pixel; NDC `(+1, +1)` becomes the top-right.

   Window space is **not normalized** — its values are pixel coordinates like `(123.5, 456.5)`.

### The chain for our 2D project

Our project skips the early stages because we don't have 3D objects, cameras, or projection. We write our vertex positions directly in a system where `(-1, -1)` is the bottom-left of the window and `(+1, +1)` is the top-right. That's already the same range as NDC.

The vertex shader writes:

```glsl
gl_Position = vec4(aPos, 0.0, 1.0);
```

`aPos` is the 2D position we put in the buffer. We pad it with `z = 0` (any depth in `[-1, +1]` is fine for 2D) and `w = 1` (so the perspective divide does nothing). The result is clip space, but with `w = 1` it's effectively the same numbers as NDC.

Then the GPU automatically:
- Does the perspective divide (no-op since `w = 1`).
- Applies the viewport transform to convert to actual pixel coordinates.

So in our project we work in clip space directly, with the convention that our values are already in NDC range.

### Which spaces do you have to think about?

For our 2D project: **only clip space**, and only because we have to write `gl_Position`. Everything else is automatic.

For 3D projects: you write the model/view/projection matrices in C++, multiply them in the vertex shader, output clip space, and the GPU does the rest.

## 5.4 What "drawing" means in the OpenGL sense

Now that we know what vertices are, what primitives are, and how OpenGL's state machine works (Part 4), we can be precise about what happens during a draw call.

Before any draw, the two-phase pattern from Part 4 applies: first set state — bind a shader program, set uniforms, configure where vertex data lives — then issue the draw. The exact mechanics of "configure where vertex data lives" are covered in Part 6 (buffers) and Part 7 (vertex attributes). For now, assume that setup is done.

With state ready, when you call:

```cpp
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
```

The GPU takes the currently active state and runs through these steps:

1. **Read vertex data.** Using state set up in advance, the GPU reads vertex data and indices from buffers in VRAM.
2. **Run the vertex shader on each vertex.** The vertex shader is part of the currently bound shader program. Each invocation runs in parallel and produces `gl_Position` plus any user-declared `out` values.
3. **Group the vertices into triangles.** Vertices 0, 1, 2 form one triangle; 3, 4, 5 form the next (because we asked for `GL_TRIANGLES`).
4. **Clip each triangle** against the `[-1, +1]³` cube — anything outside the view is trimmed or discarded.
5. **Perspective divide.** Divide `xyz` by `w` for each surviving vertex.
6. **Viewport transform.** Convert NDC into actual pixel positions.
7. **Rasterize.** For each triangle, figure out which screen pixels it covers. Generate one fragment per pixel.
8. **Interpolate the vertex shader's `out` values** across the fragments inside each triangle.
9. **Run the fragment shader on each fragment.** The fragment shader is part of the currently bound program. Each invocation runs in parallel and produces a color.
10. **Write the colors to the framebuffer** (after optional per-fragment operations covered in Part 11).

That's "drawing." Every `glDraw*` call sets this pipeline in motion using the active state. Your two shaders (vertex and fragment) run during steps 2 and 9; everything else is fixed-function hardware running automatically.

The other draw function, `glDrawArrays(GL_TRIANGLES, first, count)`, skips the index step: instead of looking up indices, it just walks vertices `first` through `first + count - 1` in order. Use indices when triangles share vertices (almost always, for real meshes); use arrays when the vertices aren't shared.

## Summary

- A **vertex** is a bundle of attributes (position plus anything else you want) — not just a point. The vertex shader sees the bundle and runs once per vertex.
- A **primitive** is the rule for connecting vertices into shapes: triangles, lines, or points. Triangles are the universal solid surface building block.
- Your numbers go through several **coordinate spaces** between your buffer and the screen: object → world → view → clip → NDC → window. For 2D we skip everything before clip space and write coordinates directly in NDC range.
- "Drawing" means: fetch vertices, run vertex shader, assemble primitives, clip, divide, viewport-transform, rasterize, run fragment shader, write to the framebuffer. One draw call runs the entire pipeline.

---

# Part 6: Buffers — where the data lives

We now know what vertices and primitives are (Part 5) and how OpenGL's state machine works (Part 4). The next question: where does the GPU find vertex data when it runs a draw call? The answer is **buffers** — chunks of GPU memory you allocate and fill from your C++ code. This Part is about what they are, what kinds exist, and how you create them.

## 6.1 What a buffer is

A **buffer** is a chunk of GPU memory (VRAM) that you allocate and fill with data from your C++ code. The GPU reads from it (and sometimes writes to it) when running shaders or other operations.

The key thing to internalize: **a buffer is just bytes.** The GPU doesn't care what's inside. *You* decide what those bytes mean by how you set up the buffer and how you use it.

When you create a buffer, OpenGL gives you back a **handle** — an integer ID, like `7` or `42`. You don't get a CPU pointer to the bytes; the bytes live in VRAM where your C++ code can't reach directly. To work with the buffer (write to it, read from it, configure it), you pass its handle to OpenGL functions.

The same buffer can hold any kind of data — floats, integers, structs, anything you can serialize to bytes. The "type" of a buffer comes from how it's used, not from what's inside.

## 6.2 Buffer kinds and what each is for

OpenGL has a concept called **targets**. A target is a slot you bind a buffer to (using `glBindBuffer`), and the target determines what role that buffer plays for the duration of the binding. Different targets give buffers different roles.

Here are the common targets and what each one is for:

| Target | Common name | What it does |
|---|---|---|
| `GL_ARRAY_BUFFER` | **VBO** (Vertex Buffer Object) | Holds per-vertex data: positions, normals, texture coords, colors. The vertex shader reads from this. |
| `GL_ELEMENT_ARRAY_BUFFER` | **IBO** or **EBO** (Index/Element Buffer Object) | Holds integer indices saying "form a triangle from vertices 0, 1, 2; then another from 2, 3, 0." |
| `GL_UNIFORM_BUFFER` | **UBO** (Uniform Buffer Object) | Holds many uniform values in one block. Multiple shaders can share the same UBO. |
| `GL_SHADER_STORAGE_BUFFER` | **SSBO** (Shader Storage Buffer Object) | Like a UBO but much larger and **writable from shaders**. Compute shaders use these heavily. |
| `GL_PIXEL_UNPACK_BUFFER` / `GL_PIXEL_PACK_BUFFER` | **PBO** (Pixel Buffer Object) | For asynchronous texture uploads/downloads — lets you transfer image data without blocking. |
| `GL_TEXTURE_BUFFER` | **TBO** (Texture Buffer Object) | A buffer accessed as if it were a 1D texture in shaders. Used for very large datasets. |
| `GL_TRANSFORM_FEEDBACK_BUFFER` | Transform feedback buffer | Captures the vertex shader's output — the GPU writes geometry results back to a buffer instead of drawing. |
| `GL_ATOMIC_COUNTER_BUFFER` | Atomic counter buffer | Holds atomic integer counters that shaders can safely increment from many invocations at once. |
| `GL_DRAW_INDIRECT_BUFFER` | Draw indirect buffer | Holds parameters for `glDrawArraysIndirect` — lets the GPU itself decide how much to draw. |
| `GL_COPY_READ_BUFFER` / `GL_COPY_WRITE_BUFFER` | Copy buffers | Generic targets for copying bytes between buffers without going through the CPU. |

This list is intimidating but you don't need to know all of them to start. For most programs, you'll only ever touch **VBOs** and **IBOs**, which we cover in detail in 6.6 and 6.7.

Recall from Part 4 that a buffer has no fixed type of its own. The same buffer object becomes a VBO when bound to `GL_ARRAY_BUFFER`, becomes an IBO when bound to `GL_ELEMENT_ARRAY_BUFFER`, becomes a copy source when bound to `GL_COPY_READ_BUFFER`. The bytes in VRAM never move; only the role in the state machine changes.

For our project we use **VBOs** (for vertex data) and **IBOs** (for indices). Other libraries inside the program (like ImGui) also use VBOs and IBOs internally for their own geometry, but that's hidden from us.

## 6.3 The create-bind-upload pattern

Creating a buffer is always the same three-step pattern, regardless of which target you'll bind it to. To follow what each step actually changes, keep three separate things in mind:

- **The handle** — an integer ID, just a name OpenGL gives you to refer to a buffer.
- **The buffer object** — the data structure (and eventually the VRAM) the handle refers to.
- **The state-machine slot** — a named target (like `GL_ARRAY_BUFFER`) that says "this buffer is currently in this role."

Each step touches a different layer.

A note on terminology before we start: a target like `GL_ARRAY_BUFFER` does double duty. It's both the name of a slot in the state machine *and* a label for the role a buffer plays while occupying that slot. When OpenGL says "the buffer bound to `GL_ARRAY_BUFFER`," it's referring to whichever buffer is in that slot right now.

### Step 1: get a handle

```cpp
GLuint buf;
glGenBuffers(1, &buf);
```

`glGenBuffers(count, output_pointer)` reserves `count` new buffer names and writes them into the array you point at. After this call, `buf` holds an integer ID — but **no buffer object exists yet, and no VRAM has been allocated**. You only have a name OpenGL has reserved for you to use later.

### Step 2: bind the name to a target

```cpp
glBindBuffer(GL_ARRAY_BUFFER, buf);
```

`glBindBuffer(target, name)` does two things:

1. If a buffer object with this name doesn't yet exist, it creates one — but with **zero allocated VRAM**.
2. It puts that buffer object into the state-machine slot named by `target`.

So after this call: the `GL_ARRAY_BUFFER` slot is no longer empty — it points at your buffer object. The buffer object exists. But it has no memory behind it yet.

(Per Part 4: this step only changes a slot in the state machine. No allocation work yet.)

### Step 3: allocate VRAM for the buffer in the slot

```cpp
glBufferData(GL_ARRAY_BUFFER, size_in_bytes, data_pointer, GL_STATIC_DRAW);
```

This is the step that finally allocates memory. Notice the first argument: it's a **target**, not your `buf` handle. `glBufferData` says: "for whichever buffer is currently bound to this target, allocate `size` bytes of VRAM and (if `data` is not null) copy `size` bytes from CPU memory into that VRAM."

The slot doesn't change here — it's still pointing to the same buffer it pointed to after step 2. What changes is the buffer itself: it now has real VRAM behind it, and (if you provided `data`) that VRAM now contains your data.

If you pass `nullptr` for `data`, the buffer is allocated but uninitialized — useful when you'll fill it later with `glBufferSubData`. The fourth argument — the "usage hint" — controls where the driver puts the buffer in VRAM and is covered in 6.4.

After `glBufferData` returns, your data is in VRAM and ready for the GPU to use.

### What changes after each step

|                  | After Step 1   | After Step 2          | After Step 3                    |
|------------------|----------------|-----------------------|---------------------------------|
| Handle           | exists         | exists                | exists                          |
| Buffer object    | does not exist | exists, no VRAM       | exists, has VRAM                |
| Slot for target  | unchanged      | points at your buffer | points at your buffer (unchanged) |
| Your data        | in CPU RAM     | in CPU RAM            | also copied into VRAM           |

### Modifying without re-allocating

To overwrite part of an existing buffer without re-allocating (the buffer's size doesn't change), use `glBufferSubData`:

```cpp
glBufferSubData(GL_ARRAY_BUFFER, offset, size, new_data);
```

This copies `size` bytes from `new_data` into the buffer that's currently bound to `GL_ARRAY_BUFFER`, starting at byte `offset`. Faster than `glBufferData` because there's no allocation. Our renderer uses this when geometry changes between frames but the existing allocation is large enough.

If the new data is *bigger* than the existing allocation, you can't use `glBufferSubData` — you need `glBufferData` again, which re-allocates.

## 6.4 Usage hints

The fourth argument of `glBufferData` is the **usage hint**:

```cpp
glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
//                                        ^^^^^^^^^^^^^^^
//                                        usage hint
```

It tells the driver how you intend to use this buffer — how often you'll change it, who reads it, who writes it. The driver uses the hint to decide *where* in VRAM to put the buffer. Some VRAM regions are faster for the GPU to read; others are easier for the CPU to write into. The hint helps the driver make a good choice.

**This is a hint, not a rule.** The buffer works the same regardless of which hint you pass. If you pick the "wrong" hint, you might lose some performance, but your code still runs correctly.

The hint name has the format `GL_<frequency>_<access>`. The frequency tells the driver how often the data changes:

| Frequency | Meaning |
|---|---|
| `STATIC` | You'll set the data once and rarely (or never) change it. |
| `DYNAMIC` | You'll change the data occasionally — say, every few frames. |
| `STREAM` | You'll change the data every frame, or close to it. |

The access tells the driver who's writing and who's reading:

| Access | Meaning |
|---|---|
| `DRAW` | CPU writes the data; GPU reads it during draw calls. (The common case.) |
| `READ` | GPU writes the data; CPU reads it back. |
| `COPY` | GPU writes the data; GPU reads it for another draw. |

You combine one frequency and one access. The most common combinations:

- **`GL_STATIC_DRAW`** — for data you upload once and reuse many times. Used for the quad's VBO.
- **`GL_DYNAMIC_DRAW`** — for data that changes occasionally. Used for the arrow VBO when its size grows.
- **`GL_STREAM_DRAW`** — for data that changes every frame.

For most projects you'll only ever use the `_DRAW` variants. The `_READ` and `_COPY` variants exist for advanced cases (reading GPU output back to CPU, copying between GPU buffers).

## 6.5 What kinds of data fit in buffers

Anything you can serialize into a flat byte sequence fits in a buffer. There are three common patterns for arranging vertex data, each with different tradeoffs.

### A flat array of one type

The simplest case: one attribute per vertex, packed back-to-back.

```cpp
float positions[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f,
};
glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
```

8 floats = 32 bytes total. Each pair of floats is one vertex's 2D position.

### Interleaved struct data

The most common pattern for vertex data: pack everything for one vertex together, then move on to the next vertex.

```cpp
struct Vertex {
    float x, y;       // position
    float u, v;       // texture coordinate
};

Vertex verts[4] = {
    { -1.0f, -1.0f, 0.0f, 0.0f },
    {  1.0f, -1.0f, 1.0f, 0.0f },
    {  1.0f,  1.0f, 1.0f, 1.0f },
    { -1.0f,  1.0f, 0.0f, 1.0f },
};
glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
```

In memory, this looks like:

```
bytes:  [ x  y  u  v ][ x  y  u  v ][ x  y  u  v ][ x  y  u  v ]
        \____________/\____________/\____________/\____________/
          vertex 0      vertex 1      vertex 2      vertex 3
        each vertex = 16 bytes (4 floats)
```

The C++ struct's memory layout becomes the buffer's layout. Later (in Part 7), we'll tell the vertex shader "the position attribute is at byte offset 0 within each vertex, the texture coord is at offset 8, and the total stride between vertices is 16 bytes."

This is what our quad's VBO looks like — interleaved position + texcoord.

### Separate arrays per attribute

You can also keep different attributes in different buffers entirely:

```cpp
float positions[] = { /* 8 floats */ };
float texcoords[] = { /* 8 floats */ };

GLuint posBuf, uvBuf;
glGenBuffers(1, &posBuf);
glGenBuffers(1, &uvBuf);

glBindBuffer(GL_ARRAY_BUFFER, posBuf);
glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

glBindBuffer(GL_ARRAY_BUFFER, uvBuf);
glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
```

After this sequence the `GL_ARRAY_BUFFER` slot points at `uvBuf` — the last one bound. `posBuf` still exists in VRAM with its data intact; only the slot's pointer changed. Either buffer can be re-bound later when needed.

Each buffer holds one attribute. You then point the vertex shader's two attributes at two different buffers (covered in Part 7).

### Which to use?

- **Interleaved** is usually a tiny bit faster because GPU memory reads pull a small chunk (a "cache line") at once — when one vertex's position is fetched, its texcoord is already in the same chunk and comes along for free. Spread the same data across separate buffers and the GPU has to do two unrelated fetches per vertex.
- **Separate arrays** are easier when one attribute changes often and the others are static. You can re-upload only the changed one.
- **Flat single-type** is for when there's only one attribute, or when you're storing indices (covered in 6.7).

For our project we use interleaved for the quad VBO and a flat single-type buffer for indices.

## 6.6 Vertex Buffer Objects (VBOs)

A **Vertex Buffer Object** (VBO) is just a name for a buffer bound to `GL_ARRAY_BUFFER`. Functionally, it's the buffer the vertex shader reads its inputs from.

A VBO doesn't, by itself, say *what* its bytes mean. The same 32 bytes could be 8 floats arranged as four 2D positions, or 4 floats arranged as one 4D vector, or 32 bytes of integer indices. The bytes are just bytes; the **interpretation** of those bytes comes from a separate object called a VAO (covered in Part 7).

So a VBO in isolation is a flat byte container. A VBO + a VAO describing how to read it is a usable source of vertex data.

### Multiple VBOs

You can have many VBOs in one program. They're independent — different sizes, different contents, different lifetimes. Which VBO feeds which input of the vertex shader is decided by configuration covered in Part 7.

In our project we have two VBOs:

- A **static** VBO holding the four corners of the fullscreen quad. We fill it once at startup and never change it.
- A **dynamic** VBO holding line endpoints for the velocity-vector arrows. We refill it every frame the user has arrows enabled, because the velocity field is different every frame.

Each gets a usage hint that matches its lifetime: `GL_STATIC_DRAW` for the quad, `GL_DYNAMIC_DRAW` for the arrows.

## 6.7 Index Buffer Objects (IBOs) and why we use them

An **Index Buffer Object** (IBO, also called Element Buffer Object or EBO) is a buffer bound to `GL_ELEMENT_ARRAY_BUFFER`. It holds **integer indices** that point into a VBO.

The reason indices exist is straightforward: triangles share vertices.

### The setup

Without indices, every triangle needs to fully list its three vertices in the VBO. If two triangles share an edge, the shared vertices appear twice — once for each triangle that uses them.

With indices, you store **each unique vertex once** in the VBO, and the IBO holds integer positions that say "this triangle uses vertices 0, 1, 2; the next uses 2, 3, 0." The GPU walks the IBO, fetches the matching vertex from the VBO for each index, and runs the vertex shader on it.

### Why this is actually a saving

The trick is that **a vertex is much bigger than an index**. A vertex with position + texcoord is 16 bytes. A vertex with position + normal + texcoord (typical for 3D) is 32 bytes. An index is 4 bytes (a 32-bit integer), or even 2 bytes if you have fewer than 65,536 vertices.

So the comparison isn't "6 vertices vs 4 vertices + 6 indices" — it's about bytes.

**Quad example** (4 unique corners, 2 triangles, vertex = 16 bytes, index = 4 bytes):

| | Without indices | With indices |
|---|---|---|
| Vertex storage | 6 × 16 = 96 bytes | 4 × 16 = 64 bytes |
| Index storage | — | 6 × 4 = 24 bytes |
| **Total** | **96 bytes** | **88 bytes** |

For a quad the savings are tiny — 8 bytes. The gain looks underwhelming.

**Real mesh example** (1,000 triangles, ~500 unique vertices, vertex = 32 bytes, index = 4 bytes):

| | Without indices | With indices |
|---|---|---|
| Vertex storage | 3,000 × 32 = 96,000 bytes | 500 × 32 = 16,000 bytes |
| Index storage | — | 3,000 × 4 = 12,000 bytes |
| **Total** | **96 KB** | **28 KB** |

Now the savings are real — about 70%. Indices are tiny compared to full vertex structs, and complex meshes share each vertex among 5–6 triangles. The bigger and more vertex-rich your geometry is, the more indexing pays off.

### The other benefit: vertex shader caching

There's a second, smaller win. When the same index appears more than once in a draw call (because the same vertex is reused across triangles), the GPU can cache the vertex shader's output for that index and reuse it instead of re-running the shader. For complex meshes this saves real shader work. For our 4-vertex quad it barely matters.

### Why we use IBOs in our project

Even though the savings on a 4-vertex quad are negligible, we still use an IBO because:

- It's the standard way to draw two triangles forming a rectangle.
- Setup-wise it's the same effort.
- It makes the geometry description cleaner: 4 unique corners, 6 indices that say how to triangulate them.

## 6.8 Why a quad is 4 vertices and 6 indices

This question deserves its own section because it's a common stumbling point.

A "quad" in graphics terminology means a rectangle. But the GPU only knows three primitive shapes: triangles, lines, and points. There is no native "draw a rectangle" primitive in modern OpenGL.

So a quad is drawn as **two triangles** that together cover the rectangle.

Two triangles need **six vertex uses** (three per triangle). But the four corners of a rectangle, when split along a diagonal, share two corners:

```
   3 ───── 2
   │     ╱ │
   │   ╱   │
   │ ╱     │
   0 ───── 1

Triangle 1: vertices 0, 1, 2  (bottom-left → bottom-right → top-right)
Triangle 2: vertices 2, 3, 0  (top-right   → top-left     → bottom-left)
```

The diagonal goes from corner 0 (bottom-left) to corner 2 (top-right). Both triangles use those two corners — the other two corners (1 and 3) belong to one triangle each.

So:

- **4 unique vertices** in the VBO — one for each corner.
- **6 indices** in the IBO — `0, 1, 2, 2, 3, 0`. Corners 0 and 2 appear twice because they sit on the shared diagonal; corners 1 and 3 appear once.

Together, the GPU draws two triangles whose union is the full rectangle, and we used the minimum amount of vertex data to describe it.

That's why our quad's VBO has 4 vertex entries and the IBO has 6 index entries.

---

## Part 6 summary

- A **buffer** is a chunk of VRAM with a handle — just bytes the GPU can read. The "type" of a buffer is determined by the **target** you bind it to.
- The pattern for every buffer is **create handle → bind to target → allocate VRAM**, with the slot, the buffer object, and your data each living at a different layer.
- **Usage hints** (`GL_STATIC_DRAW`, etc.) tell the driver how often you'll change a buffer; they affect performance, not correctness.
- **VBOs** hold per-vertex data the vertex shader reads. They're flat bytes; the interpretation (what byte range is which attribute) is configured separately in Part 7.
- **IBOs** hold integer indices into a VBO. Indices are tiny, vertices are big — the savings grow with mesh complexity.
- **A quad needs 4 vertices and 6 indices** because it's two triangles sharing two corners on the diagonal.

---

# Part 7: Vertex attributes — feeding data into the vertex shader

We have buffers full of bytes (Part 6) and we know what a vertex is (Part 5). What's still missing is the link between the two: how does the GPU know which bytes in a VBO correspond to which input of the vertex shader? That link is what an **attribute** is on the OpenGL side, and configuring attributes is what this Part is about.

## 7.1 What an attribute is

In Part 5 we defined a vertex as a bundle of values, and called each value an **attribute** (position, texture coordinate, color, etc.). That covered the conceptual side: an attribute is a per-vertex data field.

On the OpenGL side, an attribute is more specific: it's a **numbered input channel** that the vertex shader reads. Every draw call, the GPU walks through your vertex data and feeds one set of attributes per vertex into the shader.

The vertex shader declares which channels it reads, by number:

```glsl
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
```

This vertex shader declares two attribute channels: position at slot 0, texture coordinate at slot 1. The shader knows what to read; OpenGL has to be told **which bytes of which VBO** feed each slot. That configuration is what Part 7 is about.

So an attribute, in OpenGL terms, is the link between **a column of bytes in a VBO** and **a numbered input of the vertex shader**.

### Attributes are vertex-shader-specific

The word "attribute" in OpenGL is reserved for **vertex shader inputs**. Other shader stages — fragment, geometry, tessellation — also have inputs, but they're not called attributes.

The fragment shader's inputs are called **varyings**. They come from the previous shader stage's `out` variables, interpolated across the triangle by the rasterizer. The fragment shader does not read from VBOs directly; it reads from whatever the vertex shader produced.

The data flow looks like:

```
VBOs ──→ [vertex shader inputs = ATTRIBUTES] ──→ [vertex shader outputs]
                                                    │  rasterizer interpolates
                                                    ▼
                                                  [fragment shader inputs = VARYINGS]
```

This is why this Part is titled "Vertex attributes" specifically. Varyings — how vertex shader outputs reach the fragment shader — are covered in Part 10. For Part 7, when you read "attribute," it always means a vertex shader input.

### Attributes are per-vertex

Attribute values change per vertex. The vertex shader runs once per vertex, and each invocation gets a fresh set: vertex 0's position and texcoord, then vertex 1's position and texcoord, then vertex 2's, and so on.

That's the contract: per-vertex data feeds per-vertex shader invocations.

(Other inputs to shaders that don't vary per vertex — uniforms, textures — are covered in Parts 12 and 13. Don't confuse them with attributes.)

### What kinds of values can be attributes?

An attribute is one to four numeric components per vertex. So it can be:

- A scalar (1 component): `float`, `int`.
- A vec2 (2 components): typically a 2D position or texture coordinate.
- A vec3 (3 components): typically a 3D position or normal.
- A vec4 (4 components): typically a color (RGBA) or 4D position.

The components can be of various numeric types — `float`, `int`, `unsigned byte`, etc. (full list in 7.3).

For our quad: two attributes, both `vec2 float` (a 2D position and a 2D texture coordinate). For our arrows: one attribute, `vec2 float` (just position).

## 7.2 Attribute slots — how many you have, why they're numbered

Every vertex shader has a fixed number of **attribute slots** available. Each slot is identified by a non-negative integer: 0, 1, 2, and so on.

The OpenGL spec guarantees at least **16 slots** per vertex shader. Most modern GPUs offer more — typically 16 to 32. You can query the actual limit on your machine:

```cpp
GLint maxAttribs;
glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
// maxAttribs is typically 16 or 32
```

In practice you'll never run out unless you're doing something exotic. Most shaders use 2 to 5 attributes.

### Why slots are numbered

The vertex shader source and the C++ side of your program are compiled separately. The shader is compiled at runtime by the driver (Part 3); the C++ side is compiled by your C++ compiler. They never see each other's source.

So when the vertex shader writes:

```glsl
layout (location = 0) in vec2 aPos;
```

…and the C++ side writes:

```cpp
glVertexAttribPointer(0, 2, GL_FLOAT, ...);
```

…the only thing connecting these two sides is **the slot number `0`**. The variable name `aPos` exists only inside the shader; the C++ side doesn't know it. The two sides agree on slot numbers, not names.

This is why attributes are numbered: it's a contract that survives the compilation gap between shader source and C++ source.

### Two ways to assign slots to attribute names

Modern style (what we use): put `layout (location = N) in ...` in the shader. The slot number is fixed at the point of declaration.

Older style: omit `layout (location = ...)` and use `glBindAttribLocation(program, N, "aPos")` from C++ before linking the program. The slot number is set by C++ instead.

Both styles work, but the modern one is preferred — it keeps the slot number visible in the shader source, where it's used. We use the modern style throughout.

## 7.3 `glVertexAttribPointer` — every parameter

This is the function that configures one attribute slot. Most of Part 7 boils down to understanding what each of its parameters does.

```cpp
glVertexAttribPointer(
    GLuint       index,
    GLint        size,
    GLenum       type,
    GLboolean    normalized,
    GLsizei      stride,
    const void*  pointer
);
```

What each parameter does:

**`index`** — the attribute slot number to configure. Same number as `layout (location = N)` in the vertex shader.

**`size`** — number of components per vertex for this attribute. 1 for a scalar, 2 for a `vec2`, 3 for a `vec3`, 4 for a `vec4`. Awkwardly named "size" but it's really "component count."

**`type`** — the data type of each component. Common values:

| `type` value | Bytes per component | Used for |
|---|---|---|
| `GL_FLOAT` | 4 | most attributes |
| `GL_HALF_FLOAT` | 2 | compact floats (memory savings) |
| `GL_INT` | 4 | signed integers |
| `GL_UNSIGNED_INT` | 4 | unsigned integers |
| `GL_BYTE` / `GL_UNSIGNED_BYTE` | 1 | packed colors, small integer data |
| `GL_SHORT` / `GL_UNSIGNED_SHORT` | 2 | medium-precision integer data |

`size × bytes-per-component` gives the total bytes for one attribute on one vertex. For example, a `vec2` of `GL_FLOAT` is 2 × 4 = 8 bytes.

**`normalized`** — only relevant when `type` is an integer type. If `GL_TRUE`, the integer value is normalized into a floating range before reaching the shader: unsigned ints become `[0, 1]`, signed ints become `[-1, +1]`. If `GL_FALSE`, the integer reaches the shader as a raw integer. Ignored entirely when `type` is a float type. Why this is `GLboolean` (not C++ `bool`) is covered in 7.5.

**`stride`** — number of bytes between the start of one vertex's data and the start of the next. Covered in detail in 7.4.

**`pointer`** — despite the name, this is **not a pointer**. It's a **byte offset** within the bound VBO, cast to `(void*)` for historical reasons. Covered in detail in 7.4.

### A real example for our quad

Our quad's VBO has 4 vertices, each storing a position (`vec2`) followed by a texture coordinate (`vec2`):

```
bytes:  [ x  y  u  v ][ x  y  u  v ][ x  y  u  v ][ x  y  u  v ]
        \____________/\____________/\____________/\____________/
            16 bytes      16 bytes      16 bytes      16 bytes
```

Configuring the two attributes from C++ looks like:

```cpp
// position attribute — slot 0, vec2 of floats
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);

// texcoord attribute — slot 1, vec2 of floats
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
```

For both calls: `size=2` (two floats), `type=GL_FLOAT`, `normalized=GL_FALSE` (it's a float, the flag is ignored anyway), `stride=16` (16 bytes per vertex). The only difference is **`index`** (which slot) and **`pointer`** (where in each 16-byte chunk this attribute starts).

The next two sections zoom in on `stride` and `pointer` (7.4) and on `normalized` / `GLboolean` (7.5).

## 7.4 Stride and offset

These are the two parameters of `glVertexAttribPointer` that describe **where to find the bytes of this attribute inside the VBO**. They're related but answer different questions:

- **`stride`** — *how far apart* consecutive vertices are in the buffer.
- **`pointer`** (the offset) — *where* this specific attribute starts within each vertex chunk.

Together they let OpenGL walk the buffer correctly: for vertex N, read this attribute starting at byte `(offset + N × stride)`.

### The interleaved case (what our quad uses)

Our quad VBO has 4 vertices laid out as `position (vec2 float) + texcoord (vec2 float)`:

```
        vertex 0          vertex 1          vertex 2          vertex 3
       ┌───────────────┬───────────────┬───────────────┬───────────────┐
bytes  │ x  y  u  v   │ x  y  u  v   │ x  y  u  v   │ x  y  u  v   │
       │ 0  4  8  12  │16 20 24 28   │32 36 40 44   │48 52 56 60   │
       └───────────────┴───────────────┴───────────────┴───────────────┘
         ↑     ↑         ↑     ↑         ↑     ↑         ↑     ↑
         pos   tex       pos   tex       pos   tex       pos   tex
```

Each vertex chunk is 16 bytes (4 floats × 4 bytes each). So `stride = 16` for both attributes — the distance between one vertex's data and the next is always 16 bytes, regardless of which attribute you're reading.

What changes is the offset within each chunk:

- Position starts at byte 0 of its chunk → `offset = 0`.
- Texcoord starts at byte 8 of its chunk (after 2 floats of position) → `offset = 8`.

So for vertex N:

- Position is at byte `0 + N × 16` (i.e., 0, 16, 32, 48).
- Texcoord is at byte `8 + N × 16` (i.e., 8, 24, 40, 56).

Both attributes share the same stride because they live in the same buffer, side by side. They differ in offset because they start at different points within each vertex chunk.

### The separate-buffer case

If position and texcoord lived in **different** buffers (one buffer per attribute):

```
posBuf:  [ x y ][ x y ][ x y ][ x y ]   — each entry 8 bytes, packed
uvBuf:   [ u v ][ u v ][ u v ][ u v ]   — each entry 8 bytes, packed
```

Then each attribute has:

- `stride = 8` (8 bytes between consecutive position values, since they're packed).
- `offset = 0` (each attribute starts at byte 0 of its own buffer).

Because the buffer holds only this attribute, there's nothing else interleaved — the stride is just the size of one entry.

### `stride = 0` means "tightly packed"

When you pass `stride = 0`, OpenGL doesn't take the value literally. It interprets `0` as a special signal: "I have no interleaving — the values for this attribute are stored back-to-back, with no gap between them. Compute the stride for me."

The stride OpenGL substitutes is `size × bytes-per-component`:

- `vec2` of `GL_FLOAT` → stride becomes `2 × 4 = 8` bytes.
- `vec3` of `GL_FLOAT` → stride becomes `3 × 4 = 12` bytes.
- `vec4` of `GL_UNSIGNED_BYTE` → stride becomes `4 × 1 = 4` bytes.

When this is correct: the buffer holds **only this one attribute**, packed tightly with no padding (the separate-buffer case described above).

When this is wrong: the buffer is **interleaved** with other attributes. Then the actual stride is the size of the whole vertex chunk, not the size of one attribute. Passing `stride = 0` here would cause OpenGL to read every vertex from the wrong position, producing scrambled output.

Why we don't use this shortcut in our project: explicit numbers reveal the layout to anyone reading the code. A future reader (or future you) glancing at `glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, ...)` immediately sees that vertex chunks are 16 bytes apart. With `stride = 0`, the layout is implicit and easier to misuse.

### Why the offset parameter is called `pointer` and typed as `const void*`

The sixth parameter of `glVertexAttribPointer` is awkwardly typed:

```cpp
const void*  pointer
```

Despite the name and type, **this is not a pointer in modern OpenGL**. It's a byte offset, cast to `(void*)` because the function signature requires it.

The reason is historical. In early OpenGL — before VBOs existed (added in OpenGL 1.5, 2003) — vertex data lived in **regular CPU memory**. Your code would call `glVertexAttribPointer` with an actual pointer to a `float` array on the CPU, and OpenGL would copy the data over to the GPU on every draw call:

```cpp
float positions[100] = { /* ... */ };
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);  // a real pointer
```

The function name and `const void*` type made sense in that world.

When VBOs were introduced, OpenGL **reused the same function** to avoid breaking existing code. The signature stayed the same, but the *meaning* of the last parameter changed depending on what's bound to `GL_ARRAY_BUFFER`:

- **A VBO is bound:** the value is interpreted as a **byte offset** within the VBO. The pointer type is just a container for the integer.
- **No VBO is bound:** the value is still interpreted as a CPU pointer (legacy behavior).

In modern Core profile, you always have a VBO bound, so the parameter is always a byte offset. But to pass an integer through the `const void*` type, you have to cast it:

```cpp
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (const void*)0);  // offset 0 bytes
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (const void*)8);  // offset 8 bytes
```

The cast is purely syntactic noise — the runtime treats the value as an integer offset. It's just a relic of the old API that survives for backward compatibility. Newer OpenGL functions (`glVertexAttribFormat` and friends) take a clean `GLuint relativeoffset` parameter without the awkward cast, but `glVertexAttribPointer` is what you'll meet most often, so it's worth knowing why it looks the way it does.

### The takeaway

- **Stride** = "advance this many bytes to get to the next vertex."
- **Offset** (the `pointer` parameter) = "skip this many bytes from the start of each vertex chunk to find this attribute."

For interleaved data (one buffer, multiple attributes per vertex): same stride for all attributes, different offsets. For separate buffers (one buffer per attribute): each attribute has its own buffer, stride equals the size of that attribute, offset is 0.

## 7.5 The normalize flag

The fourth parameter of `glVertexAttribPointer`:

```cpp
glVertexAttribPointer(index, size, type, normalized, stride, pointer);
//                                       ^^^^^^^^^^
```

It's a `GLboolean` — `GL_TRUE` or `GL_FALSE` — and it controls how integer-typed attribute components reach the shader.

### What it does

If `type` is a **floating-point type** (`GL_FLOAT`, `GL_HALF_FLOAT`, `GL_DOUBLE`), this flag is **ignored**. Floats reach the shader as floats; there's nothing to normalize.

If `type` is an **integer type** (`GL_BYTE`, `GL_UNSIGNED_BYTE`, `GL_SHORT`, etc.), the flag matters:

- `GL_TRUE` — the integer value is **scaled into a normalized floating range** before reaching the shader.
  - Unsigned types map to `[0, 1]` (e.g., `0..255` → `0.0..1.0`).
  - Signed types map to `[-1, +1]` (e.g., `-128..127` → `-1.0..1.0`).
  - The shader sees a float.
- `GL_FALSE` — the integer reaches the shader as a raw integer (or is cast to float without scaling, depending on the shader's input type).

### Why this matters

It lets you store data **compactly** in the buffer but use it as a float in the shader.

The classic example is color. A color with 4 channels stored as `GL_FLOAT` takes 4 × 4 = 16 bytes per vertex. Stored as `GL_UNSIGNED_BYTE` with `normalized = GL_TRUE` it takes only 4 bytes — a 75% memory saving — and the shader still sees float colors in `[0, 1]`. The conversion happens automatically in dedicated hardware — you don't write any conversion code in the shader.

For our project we use `GL_FLOAT` for everything, so `normalized` is `GL_FALSE` (and ignored) on every call. The flag matters more in projects that pack vertex data tightly.

The parameter type is `GLboolean` rather than C++ `bool` for the reasons covered in 4.2 (OpenGL's own type names) — that's why you write `GL_FALSE` here rather than `false`.

## 7.6 Enabling attributes

After configuring an attribute slot with `glVertexAttribPointer`, there's one more step: you have to **enable** it.

```cpp
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
glEnableVertexAttribArray(0);   // turn slot 0 on
```

Without the enable call, the slot stays disabled and OpenGL won't read from your buffer for that slot, even though you configured it.

### Why enable is a separate step

By default, **every attribute slot is disabled**. The convention is that you opt-in: configure the slots you actually need, then enable them. Slots you didn't enable provide a constant default value to the shader (set with `glVertexAttrib4f` and friends, but in practice almost no one uses this).

This separation also lets you turn slots on or off without re-configuring them, which can be useful for switching between geometry that uses different attribute sets.

### Disabling

The counterpart:

```cpp
glDisableVertexAttribArray(0);
```

This turns slot 0 off without erasing its configuration. Re-enabling later picks up the same configuration.

### Where the enable state lives

Both `glEnableVertexAttribArray` and `glDisableVertexAttribArray` modify state that's recorded **inside the currently bound VAO**. Each VAO remembers its own set of enabled/disabled slots — so different VAOs can have different attributes turned on, even if the slot numbers overlap. (The VAO — short for Vertex Array Object — is the configuration container we'll cover in 7.7.)

## 7.7 Vertex Array Objects (VAOs) — the recipe

Now the payoff. We have three things from this Part:

- attribute configuration (`glVertexAttribPointer` — what bytes feed each slot)
- enable state (`glEnableVertexAttribArray` — which slots are on)
- the VBO that supplies each attribute (captured from `GL_ARRAY_BUFFER` when you call `glVertexAttribPointer`)

A **Vertex Array Object** (VAO) is the object that holds all of that — a complete recording of "everything OpenGL needs to know to feed the vertex shader."

When a VAO is bound (`glBindVertexArray(vao)`), the following calls **record into it** instead of just changing global state:

- `glVertexAttribPointer(i, ...)` — records "for slot `i`: this layout, sourced from whichever VBO is currently bound to `GL_ARRAY_BUFFER`."
- `glEnableVertexAttribArray(i)` / `glDisableVertexAttribArray(i)` — records the slot's on/off state.
- `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo)` — records the IBO. (This one is special: the IBO binding goes *into* the VAO, not into a global slot.)

Once you've recorded all this once, drawing later is just "bind the VAO, issue the draw" — the VAO replays the configuration automatically.

### The full setup pattern

```cpp
GLuint vao;
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);                          // start recording into this VAO

  // Bind the VBO. The VAO does not capture this binding directly —
  // it captures it on the next glVertexAttribPointer call.
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  // Bind the IBO. This one IS captured directly into the VAO.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

  // Configure attribute 0. Captures the currently bound VBO + this layout.
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
  glEnableVertexAttribArray(0);

  // Configure attribute 1. Captures the same VBO again + this offset.
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
  glEnableVertexAttribArray(1);

glBindVertexArray(0);                            // stop recording (optional)
```

After this, the VAO `vao` knows:

- "Slot 0 reads positions from `vbo`, with this layout, and is enabled."
- "Slot 1 reads texcoords from `vbo`, with this layout, and is enabled."
- "Indices come from `ibo`."

### Drawing with a VAO

```cpp
glBindVertexArray(vao);                          // bring the recipe live
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
```

That's it. The draw call uses everything the VAO has remembered. You don't re-bind buffers, don't re-configure attributes.

### When does the `GL_ARRAY_BUFFER` binding matter?

A natural question, especially after Part 4. The precise answer:

**The `GL_ARRAY_BUFFER` binding only matters at the moment you call `glVertexAttribPointer`.** That call snapshots the currently bound VBO and stores it inside the VAO for that attribute slot. After the snapshot, changing the `GL_ARRAY_BUFFER` binding doesn't affect attributes that have already been configured.

To make this concrete:

```cpp
glBindVertexArray(vao);

glBindBuffer(GL_ARRAY_BUFFER, vbo_A);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
glEnableVertexAttribArray(0);
//   ↑ At this moment, the VAO records "slot 0 reads from vbo_A".

glBindBuffer(GL_ARRAY_BUFFER, vbo_B);
//   ↑ This changes the global GL_ARRAY_BUFFER slot, but does NOT change
//     what the VAO remembers. Slot 0 still reads from vbo_A.

glDrawArrays(GL_TRIANGLES, 0, 3);
//   ↑ Reads vertex data from vbo_A, not vbo_B.
```

The IBO is different. Binding to `GL_ELEMENT_ARRAY_BUFFER` while a VAO is bound updates the VAO directly. There's no "snapshot moment" — the IBO binding *is* a property of the VAO.

### A required object in modern OpenGL

In OpenGL Core profile (which we use), you **must** have a non-zero VAO bound to issue a draw call. It's not optional. Even if your program only ever uses one VAO, that VAO must exist.

Older OpenGL (the Compatibility profile) had a default global VAO that the system created for you, so you could draw without explicitly creating one. Core profile removed that — every program has to be explicit about its VAOs.

### Multiple VAOs

A program with multiple kinds of geometry has multiple VAOs — one per layout. Switching geometry types is then just `glBindVertexArray(otherVao)` to swap recipes; you don't reconfigure anything.

In our project we have two VAOs:

- One for the quad — its recipe says "slot 0 = position, slot 1 = texcoord, both from the quad VBO with stride 16, offsets 0 and 8; IBO holds the 6 indices."
- One for the arrows — its recipe says "slot 0 = position from the arrow VBO, no IBO."

Each VAO matches the layout its corresponding shader program expects.

---

## Part 7 summary

- An **attribute** is a vertex shader input — specifically a *vertex* shader input. Other shader stages have inputs but they go by other names (varyings).
- Attributes are identified by **numbered slots** because shader source and C++ source are compiled separately and only share numbers, not names.
- **`glVertexAttribPointer`** configures one slot: which slot, how many components, what type, optional normalize for integer types, stride between vertices, offset within each vertex.
- **Stride** is "how far apart consecutive vertices are." **Offset** is "where this attribute starts within each vertex chunk." Both are in bytes.
- Attribute slots are **disabled by default**. After configuring, call `glEnableVertexAttribArray(slot)` to turn it on.
- A **VAO** records all the attribute configuration plus the IBO binding. The VBO bound to `GL_ARRAY_BUFFER` at the moment of `glVertexAttribPointer` is captured into the VAO; the current binding at draw time is ignored.
- In Core profile, every draw call needs a bound VAO.

---

# Part 8: The vertex shader (in depth)

We have buffers (Part 6), attributes (Part 7), and the API model around them (Part 4). Now we open up the vertex shader itself: what runs inside it, what comes in, what goes out, and what built-in variables the language provides.

## 8.1 What a vertex shader does

A vertex shader is a small program that runs **once per vertex** in a draw call. Its job is to take that vertex's input attributes and produce, at minimum, the vertex's clip-space position. It can also produce extra values to be passed downstream.

### The contract

For every vertex the GPU wants to draw, the vertex shader receives:

- A set of attribute values configured via Part 7 (position, texcoord, color, whatever you set up).
- Access to uniforms (Part 12) and textures (Part 13), if you read from them.

And must produce:

- A clip-space position written into a special variable called `gl_Position` (covered in 8.4).
- Optionally, any user-declared `out` values intended for the next pipeline stage (covered in 8.3).

That's the entire interface. One vertex in, one transformed vertex out.

### Each invocation is isolated

The vertex shader runs many invocations in parallel. They cannot communicate — there is no way for "the vertex shader for vertex 5" to look at what "the vertex shader for vertex 4" produced. Each invocation sees only its own attribute values and the shared uniforms/textures.

This isolation is what makes vertex shading so parallel: the GPU can run hundreds of vertex shader invocations concurrently because none of them depend on each other.

### Two common jobs

Most vertex shaders do one of two things:

**3D transformation.** Read a position in some local coordinate system (object space) and multiply it by transformation matrices to get a clip-space position. Roughly:

```glsl
gl_Position = projection * view * model * vec4(aPos, 1.0);
```

**2D / fullscreen pass-through.** Read a position that's already in clip-space coordinates and write it through with no math:

```glsl
gl_Position = vec4(aPos, 0.0, 1.0);
```

Our project uses the second form because the quad we draw is already specified in clip-space coordinates (the corners are at `(±1, ±1)`).

### What runs the shader

When you call a draw function, the GPU looks up the bound shader program (set by `glUseProgram`), takes its compiled vertex-shader machine code, and runs it once per vertex. The output of every invocation feeds into the next pipeline stage (primitive assembly + rasterization, covered in Part 9).

## 8.2 Inputs from attributes

A vertex shader reads its per-vertex inputs by declaring them with the `in` storage qualifier and a `layout (location = N)` to pin the slot number.

```glsl
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
```

Three things to specify in each declaration:

- **The slot number** (`location = N`) — must match the slot number you configured from C++ via `glVertexAttribPointer(N, ...)`. The slot is the only thing connecting the two sides (Part 7.2).
- **The storage qualifier** (`in`) — declares this as an input from the previous pipeline stage. For a vertex shader, "previous stage" means the vertex fetch step that pulls bytes from the VBO.
- **The type** — `vec2`, `vec3`, `vec4`, `float`, `int`, etc. Must be compatible with what the C++ side configured.

### Type matching

The C++ configuration and the GLSL type don't have to be exactly identical, but they have to be compatible. A few common combinations:

| C++ side (`glVertexAttribPointer`) | GLSL `in` type | Notes |
|---|---|---|
| `size=2, type=GL_FLOAT, normalized=GL_FALSE` | `vec2` | Direct match. |
| `size=3, type=GL_FLOAT, normalized=GL_FALSE` | `vec3` | Direct match. |
| `size=4, type=GL_UNSIGNED_BYTE, normalized=GL_TRUE` | `vec4` | Bytes scaled to `[0, 1]` floats. Common for color. |
| `size=1, type=GL_INT, normalized=GL_FALSE` | `int` | Integer pass-through. |

If the configuration and the shader type disagree (e.g., C++ says `size=3` but the shader declares `vec2`), some components are dropped or padded — usually not what you want. Make them line up.

### Naming convention

By convention many projects (including ours) prefix vertex attributes with `a` (or `a_`) to make their role obvious in the shader source: `aPos`, `aTexCoord`, `aColor`. The convention is not enforced by the language — you can name them anything.

### One invocation, one set

Every invocation of the vertex shader sees its **own** values for these inputs. The GPU walks vertex by vertex and feeds the right slice of the buffer into each invocation. The shader doesn't request data; it just reads its declared `in` variables.

## 8.3 User-declared outputs

A vertex shader's job doesn't stop at writing `gl_Position`. It can also produce **extra values that get passed to the next pipeline stage** — typically to the fragment shader, after rasterization interpolates them across the triangle.

These extra values are declared with the `out` storage qualifier:

```glsl
out vec2 vTexCoord;
out vec3 vColor;
```

For each invocation, the vertex shader writes a value to each declared `out` variable. Those values then flow to the next stage.

### The matching declaration in the next stage

For the fragment shader to receive these values, it must declare matching inputs. The simplest pairing is **by name**:

```glsl
// vertex shader
out vec2 vTexCoord;

// fragment shader
in  vec2 vTexCoord;
```

Same name, same type, same storage direction (`out` on the producer, `in` on the consumer). The link is automatic.

You can also pair by location, similar to how vertex attributes use slot numbers:

```glsl
// vertex shader
layout (location = 0) out vec3 vNormal;

// fragment shader
layout (location = 0) in  vec3 vNormal;
```

Pairing by location is useful when you want the names to differ between stages, or when you want to reorder them. We use the simpler name-based pairing.

### What happens between stages

The values you write to `out` variables are not handed directly to the fragment shader. They're handed to **rasterization**, which interpolates them across the surface of the triangle, generating one interpolated value per fragment (per pixel). The fragment shader then sees those interpolated values through its matching `in` declarations.

The interpolation is automatic — it's done by dedicated hardware, with no code from you. Full coverage is in Part 9.

### Naming convention

By convention many projects prefix these `out` values with `v` (or `v_`) to mark them as "varyings" — a leftover term from older GLSL where these values were declared with a `varying` keyword. That keyword is gone, but the prefix lingers as a useful signal.

### Don't forget to write them

If you declare an `out` variable but never assign it a value before `main()` returns, downstream stages see undefined data. The GLSL compiler doesn't enforce that you write to every declared `out`, so this is a source of subtle bugs. As a habit: every `out` you declare should be assigned somewhere in your `main()`.

## 8.4 Built-in outputs

Beyond user-declared `out` variables, the vertex shader has a small set of **built-in outputs** — predefined variables with reserved names that feed into the GPU's fixed-function stages. They're always available; you never declare them; you can't redefine them.

The two you'll meet are `gl_Position` and `gl_PointSize`.

### `gl_Position` — required

```glsl
vec4 gl_Position;   // implicit; you don't declare it, just write to it
```

This is the variable that holds the vertex's **clip-space position**. The fixed-function stages after the vertex shader (clipping, perspective divide, viewport transform — all covered in Part 9) read `gl_Position` as their input.

Every vertex shader **must** write a value to `gl_Position` before `main()` returns. If you don't, the geometry won't render correctly (or at all).

The type is `vec4` because clip space is 4D `(x, y, z, w)`. After the vertex shader, the GPU divides `xyz` by `w` to get NDC (the perspective divide). For 2D rendering with no perspective effect, you write `w = 1.0` so the divide is a no-op:

```glsl
gl_Position = vec4(aPos, 0.0, 1.0);
```

For 3D rendering with perspective, you compute clip space by multiplying through transformation matrices:

```glsl
gl_Position = projection * view * model * vec4(aPos, 1.0);
```

In both cases the variable name is the same: `gl_Position`. The GPU knows what to do with it.

### `gl_PointSize` — used only when drawing points

```glsl
float gl_PointSize;   // implicit
```

This sets the size of a rendered point, in pixels. It's only meaningful when the draw call uses `GL_POINTS` as its primitive type. For triangles or lines, the value is ignored.

If you want big rendered points without scaling them yourself, write to `gl_PointSize`:

```glsl
gl_PointSize = 10.0;   // 10×10 pixel point
```

We don't use this — our project draws triangles and lines.

### `gl_ClipDistance[]` — advanced, mentioned for completeness

An array of floats used for user-defined clipping planes. Lets you reject fragments based on custom geometric criteria. Specialized feature; we don't use it. Mentioned here only so you know the name if you encounter it elsewhere.

## 8.5 Built-in inputs

The vertex shader also has built-in **inputs** — predefined variables it can read without declaring. They give the shader information about its own position in the draw call.

### `gl_VertexID` — which vertex am I?

```glsl
int gl_VertexID;   // implicit
```

The index of the current vertex within the draw call. Specifically:

- For `glDrawArrays(mode, first, count)`: `gl_VertexID` ranges from `first` to `first + count - 1`.
- For `glDrawElements(...)`: `gl_VertexID` is the value pulled from the IBO for this vertex.

The clever use case is **procedural vertex generation**: drawing geometry without binding any VBO. You issue a draw call with no vertex data, and the vertex shader uses `gl_VertexID` to compute positions on the fly. A common trick is "draw 3 vertices with no VBO" to render a fullscreen triangle:

```glsl
const vec2 corners[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main() {
    gl_Position = vec4(corners[gl_VertexID], 0.0, 1.0);
}
```

We don't use this trick — our quad has a real VBO — but it's a useful pattern to know.

### `gl_InstanceID` — which instance am I?

```glsl
int gl_InstanceID;   // implicit
```

Used for **instanced drawing** — a way to draw the same geometry many times in one draw call, with each copy ("instance") customized via uniforms or per-instance attributes. Each invocation sees an `gl_InstanceID` from `0` to `instanceCount - 1`.

We don't use instanced drawing. Mentioned for completeness.

### `gl_DrawID` — which draw call am I?

```glsl
int gl_DrawID;   // implicit
```

For multi-draw functions like `glMultiDrawElements`, this tells the shader which sub-draw it's part of. Niche feature; we don't use it.

## 8.6 The `gl_` prefix and naming conventions

Variables prefixed with `gl_` are **reserved by the GLSL language** for built-in use. You cannot declare your own variables with that prefix:

```glsl
out vec3 gl_MyValue;   // ERROR — gl_ prefix is reserved
```

The compiler will reject it. The reservation exists to keep the boundary between language built-ins and user variables clean: when you see `gl_Position`, you know it's a fixed-meaning variable defined by the spec. When you see `aPos` or `vTexCoord`, you know it's something the project author declared.

### Project-side conventions

Beyond the reserved `gl_` prefix, projects use their own conventions to signal a variable's role. Common ones in our project (and in many GLSL codebases):

| Prefix | Role | Example |
|---|---|---|
| `a` or `a_` | Attribute (vertex shader input from a VBO) | `aPos`, `aTexCoord` |
| `v` or `v_` | Varying (vertex `out` → fragment `in`) | `vTexCoord`, `vColor` |
| `u` or `u_` | Uniform (covered in Part 12) | `u_Field`, `u_Time` |
| `gl_` | Reserved built-in (don't define your own) | `gl_Position`, `gl_VertexID` |

These are not enforced by the language. The GLSL compiler doesn't care if you name your attribute `aPos` or `bananaSplit` or `x123`. The convention is a signal to the reader of the source code.

If you read GLSL from different projects, you'll see slight variants — some use snake_case (`a_pos`) instead of camelCase (`aPos`), some omit prefixes entirely. Pick a style and use it consistently within a project.

---

## Part 8 summary

- A vertex shader runs **once per vertex** and produces, at minimum, a clip-space position via `gl_Position`. Each invocation runs in isolation.
- It reads its per-vertex inputs through `in` declarations with matching slot numbers (`layout (location = N)`), connected to the C++ side via Part 7's configuration.
- It can produce **user-declared outputs** (`out` variables) that flow to the next pipeline stage. Pairing with the fragment shader is by name (or by location).
- It has **built-in outputs** with reserved `gl_` names: `gl_Position` (always required), plus `gl_PointSize`, `gl_ClipDistance[]` (situational).
- It has **built-in inputs** like `gl_VertexID` and `gl_InstanceID` that tell each invocation about its identity in the draw.
- The `gl_` prefix is reserved by the language. Project-side conventions like `a`/`v`/`u` prefixes aren't enforced but are useful signals to the reader.

---

# Part 9: The middle of the pipeline (fixed function)

Between the vertex shader (Part 8) and the fragment shader (Part 10), several fixed-function stages run automatically. You don't write code for them, but they do real work, and the way they transform your data shapes what the fragment shader receives. This Part covers each of those stages in order.

A reminder of the order from the pipeline map (Part 1.3):

```
Vertex shader  →  [primitive assembly, clipping, perspective divide, viewport transform, rasterization]  →  Fragment shader
```

Each bracketed stage gets its own section here.

## 9.1 Primitive assembly

The vertex shader produced one transformed vertex per input vertex — a stream of vertices, each with a `gl_Position` and any user-declared `out` values. Primitive assembly takes that stream and **groups it into primitives** (triangles, lines, or points) according to the draw mode you specified.

The grouping rules:

| Draw mode | Grouping rule | Number of primitives from N vertices |
|---|---|---|
| `GL_POINTS` | Each vertex is one point | N |
| `GL_LINES` | Every 2 consecutive vertices form one line | N / 2 |
| `GL_LINE_STRIP` | Each vertex extends a chain | N − 1 |
| `GL_TRIANGLES` | Every 3 consecutive vertices form one triangle | N / 3 |
| `GL_TRIANGLE_STRIP` | Each new vertex extends the previous triangle by reusing the last two | N − 2 |
| `GL_TRIANGLE_FAN` | All triangles share the first vertex | N − 2 |

The divisions are integer division; you're expected to pass a vertex count compatible with the primitive type (a multiple of 3 for `GL_TRIANGLES`, a multiple of 2 for `GL_LINES`, etc.). Submitting a count that doesn't divide cleanly will simply drop the trailing leftover vertices.

For our project: `GL_TRIANGLES` for the quad (6 indexed vertices → 2 triangles) and `GL_LINES` for the arrow segments (every pair of vertices is one segment).

### Why this is its own stage

The vertex shader processes one vertex at a time and has no concept of triangles or lines. It just outputs transformed positions. Primitive assembly is where the per-draw-call grouping rule (`GL_TRIANGLES` etc.) finally takes effect — vertices that came out of the shader independently get tied together into the shape units the rest of the pipeline operates on.

This stage is fixed-function: there's no code you write to control it. The only knobs you have are the draw mode you pass to `glDraw*` and (if you have one) a geometry shader, which can replace this default behavior.

## 9.2 Clipping

After primitives are assembled, clipping checks each one against the **clip-space cube** — the region where `−w ≤ x ≤ w`, `−w ≤ y ≤ w`, `−w ≤ z ≤ w` for each vertex's clip-space position `(x, y, z, w)`. Anything outside this region is invisible from the camera's view, so the GPU doesn't bother sending it to rasterization.

For each primitive, three things can happen:

1. **Fully inside.** All vertices satisfy the clip cube test. The primitive passes through unchanged.
2. **Fully outside.** All vertices are outside the cube on the same side. The primitive is **discarded** — it won't render at all.
3. **Partially inside.** Some vertices are inside and some are outside. The GPU **clips** the primitive, generating new vertices on the boundary where the primitive crosses out, and forwards only the inside portion.

For the partial case, the new boundary vertices are generated by the GPU. Their attribute values (the user-declared `out` values from the vertex shader) are interpolated linearly between the original vertices.

### Why clipping happens here, before perspective divide

Clipping has to happen *before* the perspective divide (next section). After the divide, all surviving primitives end up inside `[−1, +1]`, so there'd be nothing to clip. The clip cube test in pre-divide clip space (`−w ≤ x ≤ w`) is mathematically equivalent to checking `−1 ≤ x/w ≤ 1` in NDC, but it's done before the divide so that primitives can be cut cleanly.

### User-defined clipping

In addition to the implicit clip cube, the vertex shader's optional `gl_ClipDistance[]` output (mentioned in 8.4) lets you add custom clipping planes. A negative value clips that vertex out. We don't use this feature, but the clipping stage is where it would take effect.

## 9.3 Perspective divide and viewport transform

After clipping, two final coordinate transforms run before rasterization. They convert clip space into actual pixel positions in the framebuffer.

### Perspective divide

For each vertex's `gl_Position = (x, y, z, w)`, the GPU divides:

```
NDC.xyz = (x / w, y / w, z / w)
```

This is the **perspective divide**. The result is **Normalized Device Coordinates** (NDC), where each axis is in `[−1, +1]`.

For 2D rendering with `w = 1` (our case — our project's vertex shader writes `gl_Position = vec4(aPos, 0.0, 1.0)`, see Part 15.1), the divide is a no-op: `x / 1 = x`. The NDC values are the same numbers as the clip-space values, just with `w` dropped.

For 3D rendering with perspective projection, `w` varies with depth — vertices farther from the camera have larger `w`. The divide compresses those vertices toward the screen center, which is what produces the "things farther away look smaller" effect of perspective.

### Viewport transform

NDC is `[−1, +1]` on every axis, regardless of the actual window size. The viewport transform converts NDC into **pixel coordinates** using whatever rectangle was set with `glViewport(x, y, width, height)`:

- NDC `(−1, −1)` → window pixel `(x, y)` (the bottom-left of the viewport rectangle).
- NDC `(+1, +1)` → window pixel `(x + width, y + height)` (the top-right).
- NDC `(0, 0)` → the center of the viewport rectangle.

For a default `glViewport(0, 0, windowWidth, windowHeight)`, NDC maps onto the whole window.

The depth (`z`) component is also transformed into a range typically `[0, 1]`, used by depth testing later in the pipeline (covered in Part 11).

After the viewport transform, every vertex of every surviving primitive has a position in **window space** — actual pixel coordinates. This is the input rasterization works on.

## 9.4 Rasterization

Rasterization takes a primitive in window space and figures out which screen pixels it covers. For each covered pixel, it generates one **fragment**.

A fragment is a pixel-sized candidate output. Each fragment carries:

- The window-space coordinates of its pixel.
- A depth value (used by depth testing — Part 11).
- Interpolated values for every `out` variable the vertex shader declared (covered in 9.5).

The fragment is *not* yet a final pixel — it's a candidate. Later stages (depth test, stencil test, blending — all covered in Part 11) decide whether the fragment actually becomes a pixel in the framebuffer.

### How a triangle gets rasterized

For a triangle with three window-space vertex positions, the GPU:

1. Determines the triangle's bounding rectangle in pixel space.
2. For each pixel in that rectangle, tests whether the pixel center is inside the triangle (using the three edge equations).
3. If it's inside, generates a fragment for that pixel.

A small visualization. Imagine a triangle with corners at pixel positions `A`, `B`, `C`:

```
.  .  .  .  .  .  .  .  .  .
.  .  .  C  .  .  .  .  .  .
.  .  ╱ ░  ╲  .  .  .  .  .
.  ╱  ░  ░  ╲  .  .  .  .  .
A ░  ░  ░  ░  B  .  .  .  .
.  ╲  ░  ░  ╱  .  .  .  .  .
.  .  ╲ ░ ╱  .  .  .  .  .
.  .  .  .  .  .  .  .  .  .
```

The pixels marked `░` get fragments generated for them; pixels marked `.` don't. Each `░` becomes one fragment shader invocation later.

This is hardware-accelerated and very fast — modern GPUs rasterize hundreds of millions of pixels per second.

### Lines and points

Rasterization works similarly for other primitive types:

- **Lines** are rasterized along their length, generating fragments for each pixel the line crosses.
- **Points** are rasterized as a small filled region (typically a single pixel, or larger if `gl_PointSize` was set).

### What rasterization does not decide

Rasterization figures out *which pixels are covered*. It doesn't decide their color. Color is the fragment shader's job (Part 10).

## 9.5 Interpolation — what the rasterizer does to your `out` variables

When rasterization generates a fragment, the fragment doesn't get the value of any specific vertex. Instead, the rasterizer **interpolates** every `out` variable from the vertex shader, using the fragment's position relative to the primitive's vertices.

### How interpolation works

For a triangle with three vertices, each having a value (say, a `vec3` color):

```
Vertex A:  vColor = (1, 0, 0)   // red
Vertex B:  vColor = (0, 1, 0)   // green
Vertex C:  vColor = (0, 0, 1)   // blue
```

For a fragment somewhere inside the triangle, the rasterizer computes three weights — one per vertex — based on how close the fragment is to each vertex. The weights sum to `1.0`. (These are called **barycentric coordinates**.) The fragment's interpolated color is the weighted sum:

```
fragmentColor = wA × (1, 0, 0)  +  wB × (0, 1, 0)  +  wC × (0, 0, 1)
```

A fragment at vertex `A` would have weights `(1, 0, 0)` and see pure red. A fragment at the triangle's centroid would have weights `(⅓, ⅓, ⅓)` and see a brownish gray. Smooth gradients across the triangle come from this weighting.

This applies to **every** `out` variable, not just colors — texture coordinates, normals, custom values, all of them. Each fragment gets its own interpolated set.

### Interpolation modifiers

GLSL gives you three optional qualifiers on `in`/`out` declarations to control how interpolation behaves:

- **`smooth`** (the default) — perspective-correct linear interpolation. Standard for most data.
- **`noperspective`** — linear interpolation in window space, ignoring perspective correction. Cheaper, but textures and similar values can warp incorrectly under perspective.
- **`flat`** — no interpolation. The fragment receives the value from the **provoking vertex** (by default the last vertex of the primitive). Useful when you specifically want a single value across the whole primitive.

```glsl
flat   in int   vMaterialID;
smooth in vec2  vTexCoord;       // smooth is the default; this is the same as omitting it
```

For our project, all our varyings use the default smooth interpolation.

### What "perspective-correct" means

A naive linear interpolation in screen space gives wrong results for textures viewed at an angle — the texture appears warped. Perspective-correct interpolation compensates by interpolating using `1/w` factors from the original vertices. The result is that textures look right even on triangles at sharp angles to the camera. This is hardware-accelerated — the `smooth` qualifier (or the default) just turns it on; no extra code from you.

### A consequence of interpolation

The fragment shader **sees a single value per `in` variable per invocation** — not the three vertex values. The interpolation produces one number per pixel. From the shader's point of view, it just reads `vTexCoord` (or whatever) and gets a value; it doesn't know the value came from interpolating between three vertices. The smoothness of textures, lighting, and other effects is entirely a result of this interpolation step.

---

## Part 9 summary

- Between the vertex shader and the fragment shader, several **fixed-function stages** run automatically: primitive assembly, clipping, perspective divide, viewport transform, rasterization (with interpolation).
- **Primitive assembly** groups the stream of transformed vertices into triangles, lines, or points based on the draw mode.
- **Clipping** discards or trims primitives outside the clip-space cube; new vertices may be generated on the boundary.
- **Perspective divide** converts clip space `(x, y, z, w)` into NDC by dividing by `w`.
- **Viewport transform** converts NDC into actual pixel coordinates using the rectangle set by `glViewport`.
- **Rasterization** generates one fragment per covered pixel of each primitive.
- **Interpolation** computes a value per fragment for every `out` variable, using barycentric weights across the primitive's vertices. `smooth` (default), `noperspective`, and `flat` qualifiers control how.

---

# Part 10: The fragment shader (in depth)

The fragment shader is the second required shader stage. We've referenced it many times — Parts 1, 2, 5, 8, 9 — usually in the form "runs once per pixel, produces a color." Now we open it up.

## 10.1 What a fragment shader does

A fragment shader is a small program that runs **once per fragment** generated by rasterization (Part 9). Its job is to compute the color (and optionally the depth) of that fragment.

### What a "fragment" actually is

A **fragment** is neither a pixel nor a primitive. It's a pixel-sized **candidate output** produced by rasterization from a primitive.

The distinction matters:

- A **primitive** is a triangle, line, or point — the geometric input to rasterization.
- A **fragment** is a per-pixel candidate generated from a primitive — it carries a pixel position, a depth, and the interpolated values from the vertex shader.
- A **pixel** is a single addressable slot in the framebuffer — what eventually gets shown on screen.

One primitive becomes many fragments — one per pixel it covers. Multiple overlapping primitives can produce multiple fragments for the same pixel. Only one (or none, if discarded or rejected by depth testing) becomes the final pixel.

So the flow is:

```
primitive  ──rasterization──►  fragments  ──fragment shader──►  candidate colors
                                                                        │
                                                                  per-fragment operations
                                                                  (depth test, blending — Part 11)
                                                                        ▼
                                                                 final pixel value
```

A fragment is what the fragment shader works on. A pixel is the result that ends up in the framebuffer after all the per-fragment operations.

For our project (one fullscreen quad, no depth test, no blending), each fragment becomes exactly one pixel with no contention. The distinction matters more in complex scenes with overlapping geometry.

### The contract

For every fragment the rasterizer produced, the fragment shader receives:

- A set of interpolated values from the vertex shader's `out` declarations (covered in 10.2).
- A set of built-in inputs like `gl_FragCoord` (covered in 10.3).
- Access to uniforms (Part 12) and textures (Part 13), if it reads from them.

And produces:

- A color, written to a user-declared `out` variable (covered in 10.4).
- Optionally, an overridden depth value via `gl_FragDepth` (covered in 10.5).

That's the entire interface. One fragment in, one color out.

### Each invocation is isolated

Like the vertex shader, the fragment shader runs many invocations in parallel — typically *hundreds of thousands* per draw call (every covered pixel becomes one). Invocations cannot communicate. Each one sees only its own interpolated inputs and the shared uniforms/textures.

This isolation is what allows fragment shading to be parallel at scale. For an 800 × 800 fullscreen quad, 640,000 fragment shader invocations may run on the GPU concurrently in batches.

### Common jobs

Most fragment shaders do one or more of:

- **Sample a texture** at the interpolated texture coordinate, output the color.
- **Apply lighting** — compute brightness from interpolated normals and uniform light positions.
- **Apply post-processing** — read a previous render's color and transform it (blur, color correction).
- **Mix multiple inputs** — combine several interpolated values into a final color.

Our project's fragment shader is in the first category: it samples a texture and applies a colormap.

### Throwing fragments away

A fragment shader can call `discard;` to throw away the current fragment — no pixel gets written for it:

```glsl
if (alpha < 0.01) {
    discard;
}
```

This is useful for **cutout transparency** (sharp transparent areas, like leaves on a tree texture). When discarded, the fragment is just gone — none of the later stages (depth test, blending, framebuffer write — Part 11) happen for it.

We don't use `discard` in our project, but it's worth knowing the keyword exists.

## 10.2 Inputs (interpolated varyings)

A fragment shader reads its inputs through `in` declarations:

```glsl
in vec2 vTexCoord;
in vec3 vNormal;
```

These match the **vertex shader's `out` declarations** of the same name and type (covered in 8.3). Between the two stages, rasterization interpolates the values across each primitive (covered in 9.5), so each fragment receives its own interpolated value.

### Pairing with the vertex shader

The simplest pairing is **by name**:

```glsl
// vertex shader
out vec2 vTexCoord;

// fragment shader
in  vec2 vTexCoord;
```

Same name, same type, same direction (`out` on the producer, `in` on the consumer). The link is automatic when the program is linked.

Or by location, like vertex attributes:

```glsl
// vertex shader
layout (location = 0) out vec3 vNormal;

// fragment shader
layout (location = 0) in  vec3 vNormal;
```

We use name-based pairing in our project.

### What the fragment shader actually sees

Crucially: the fragment shader does **not** see the original three vertex values that produced this fragment. It sees **one value per `in` declaration** — the interpolated result for this fragment's specific pixel position. From the shader's point of view, you just read `vTexCoord` and you get a `vec2`. The smooth gradient across a textured surface is entirely a result of rasterization's interpolation; the shader's read is a single value per invocation.

### Interpolation qualifiers

The qualifiers from 9.5 (`smooth`, `flat`, `noperspective`) apply to the fragment shader's `in` declarations the same way they apply to the vertex shader's `out`. The two sides must agree:

```glsl
// vertex shader
flat out int vMaterialID;

// fragment shader
flat in  int vMaterialID;       // matching qualifier
```

If the qualifiers don't match, linking fails. The default is `smooth` on both sides; you only declare a qualifier when you want different behavior.

### `in` here is different from `in` in the vertex shader

The keyword `in` is the same, but the data source is completely different:

- In a vertex shader, `in` reads a per-vertex attribute from a VBO (Part 7's mechanism).
- In a fragment shader, `in` reads an interpolated value from rasterization (Part 9's mechanism).

You can't bind a VBO directly to a fragment shader's input. The path is always: VBO → vertex shader → vertex shader `out` → rasterizer → fragment shader `in`.

## 10.3 Built-in inputs

Beyond user-declared `in` variables, the fragment shader has several **built-in inputs** with reserved `gl_` names. They're always available without declaration. The most useful is `gl_FragCoord`.

### `gl_FragCoord` — the pixel position of this fragment

```glsl
vec4 gl_FragCoord;   // implicit
```

The window-space position of this fragment. Its components:

- **`gl_FragCoord.x` and `gl_FragCoord.y`** — the pixel coordinates, with sub-pixel precision. The center of pixel `(123, 456)` is `(123.5, 456.5)`. The origin (by default) is the bottom-left of the viewport.
- **`gl_FragCoord.z`** — the depth value of this fragment, in `[0, 1]` after the viewport transform. Used by depth testing if enabled (Part 11).
- **`gl_FragCoord.w`** — actually `1/w` from the original clip-space position. Used in advanced perspective math; rarely needed directly.

Why this is so useful: it tells the fragment shader where it is on screen. Examples:

- A fullscreen pass that wants screen-relative effects (vignettes, post-processing) reads `gl_FragCoord.xy` and applies math based on the pixel position.
- A "draw a grid" shader can decide whether the current fragment is on a grid line by checking if `gl_FragCoord.x` or `gl_FragCoord.y` is near a multiple of some spacing.

We don't use `gl_FragCoord` in our project — we have texture coordinates from the vertex shader, which are more convenient for our needs — but it's the most commonly used fragment shader built-in across all of OpenGL programming.

### `gl_FrontFacing` — front or back face?

```glsl
bool gl_FrontFacing;   // implicit
```

`true` if this fragment came from the front face of a triangle, `false` if it came from the back face.

#### What "front face" means: vertex winding order

OpenGL decides whether a triangle is front-facing by looking at the **order in which its three vertices appear in the draw call**, called the **winding order**.

Take a triangle with three vertices labeled A, B, C laid out like this on screen:

```
       C
       │
       │
   A───B
```

If the draw call lists the vertices in the order **A → B → C**, the winding goes counter-clockwise (when viewed from the camera). By default, OpenGL considers this the **front face**.

If the draw call lists them as **A → C → B**, the winding goes clockwise. That's the **back face**.

The same triangle in space has both a "front" and a "back" — which side OpenGL labels as which depends entirely on the order you submit the vertices.

For 3D objects like cubes or characters, modelers wind the vertices so that **outside-facing triangles are counter-clockwise**. Inside-facing triangles (the ones on the back of a wall, the inside of a closed object) end up clockwise. With back-face culling enabled (`glEnable(GL_CULL_FACE)` — not on by default, and we don't use it), the GPU automatically skips back-facing triangles. That saves rasterization and shader work for triangles you'd never see anyway.

#### When `gl_FrontFacing` is useful

The classic case is rendering a flat object that should look correct from both sides — a leaf, a sheet of paper, a billboard. The triangle has a "front" and a "back" but you want to see something on both sides without duplicating the geometry. The fragment shader checks which side it's looking at:

```glsl
vec3 normal = vNormal;
if (!gl_FrontFacing) {
    normal = -normal;   // flip the normal for back-face fragments
}
// ... use `normal` for lighting
```

This makes lighting work correctly from either side using the same triangle.

#### Defaults you can change

- `glFrontFace(GL_CCW)` — counter-clockwise is front. **Default.**
- `glFrontFace(GL_CW)` — clockwise is front.
- `glEnable(GL_CULL_FACE)` plus `glCullFace(GL_BACK)` — skip back-facing triangles entirely (the most common culling setup).

We don't change these. Our quad's vertices are listed in counter-clockwise order, so its triangles are front-facing, and we don't enable culling.

### `gl_PointCoord` — texture coordinate within a point

```glsl
vec2 gl_PointCoord;   // implicit
```

Only meaningful when drawing `GL_POINTS`. Gives the position of the fragment within the point's rendered area, ranging from `(0, 0)` at one corner to `(1, 1)` at the other. Useful for textured points (sprites). We don't use this — our points-equivalent geometry is the arrow line segments.

### `gl_PrimitiveID` — which primitive this fragment came from

```glsl
int gl_PrimitiveID;   // implicit
```

The index of the primitive (triangle, line, or point) that produced this fragment, counting from 0 within the current draw call. Useful for picking (figuring out which object the user clicked on) and for debug visualization. Niche; we don't use it.

## 10.4 Outputs

The fragment shader's main output — and almost always its only one — is a color, declared with the `out` storage qualifier:

```glsl
out vec4 FragColor;
```

This is a `vec4` representing **RGBA** color. The components are red, green, blue, and alpha (opacity), each in the range `[0, 1]` for normal display. A value of `vec4(1.0, 0.0, 0.0, 1.0)` is opaque red; `vec4(0.0, 0.0, 0.0, 1.0)` is opaque black; `vec4(1.0, 1.0, 1.0, 0.5)` is half-transparent white.

You assign to this variable inside `main()` to set the fragment's color:

```glsl
void main() {
    FragColor = vec4(1.0, 0.5, 0.2, 1.0);   // opaque orange
}
```

### Where the value goes

When there is exactly one user-declared `out`, OpenGL automatically writes it to **color attachment 0** of the currently bound framebuffer. By default, color attachment 0 is the screen, so the value lands as a pixel in the rendered image.

Before reaching the framebuffer, the fragment goes through optional **per-fragment operations** (covered in Part 11):

1. **Depth test** (if enabled) — compare the fragment's depth to what's already at this pixel; reject if it's behind.
2. **Stencil test** (if enabled) — used for masking effects.
3. **Blending** (if enabled) — combine the fragment color with the existing pixel color (e.g., for transparency).
4. **Framebuffer write** — the final value lands in the framebuffer's color attachment.

If none of these features are enabled (our project's case), the fragment's color goes straight to the framebuffer.

### Naming

The name `FragColor` is a convention, not a language requirement. You could call your output `myColor` or `PixelOut` and it would still work. `FragColor` is widely used because it's recognized by anyone reading shader code.

### Failing to write

Just like the vertex shader's user-declared `out` variables, the fragment shader's output must be **assigned somewhere in `main()`** — otherwise its value is undefined. A common bug is to write to `FragColor` only inside an `if` branch and leave the `else` path silent; the output then has undefined values along that path. Always make sure every code path through `main()` ends up assigning to the output.

## 10.5 Built-in outputs

The fragment shader has one main built-in output worth knowing.

### `gl_FragDepth` — override the depth value

```glsl
float gl_FragDepth;   // implicit
```

By default, the depth value used for depth testing (Part 11) is `gl_FragCoord.z` — the depth that rasterization computed for this fragment from the original primitive's geometry. You don't have to do anything; the GPU uses that automatically.

If you want to **override** the depth, you write a value to `gl_FragDepth` in the shader:

```glsl
void main() {
    FragColor   = vec4(1.0);
    gl_FragDepth = 0.5;   // force this fragment to depth 0.5
}
```

#### When you'd want to override depth

A few cases:

- **Imposters / billboards.** A flat textured quad pretending to be a 3D object. You write a depth value that fakes the curved surface so depth tests behave correctly.
- **Custom shadow techniques.** Some shadow-mapping methods need a depth that differs from the geometric one.
- **Soft particles.** Particles that fade as they approach solid geometry rely on depth tricks.

#### The cost

Writing to `gl_FragDepth` disables an important optimization called **early depth testing**. (Depth testing itself — what it is and how it works — is covered in Part 11.2; the short version: the GPU compares each fragment's depth against the depth already at that pixel, and rejects fragments that are behind.) Normally the GPU runs that test *before* the fragment shader, which lets it skip the shader entirely for fragments that will lose the test. If the shader can change the depth, that early-out is impossible — the GPU has to run the shader first and only then test. This is slower per fragment.

So as a rule: only write to `gl_FragDepth` when you actually need to change the depth. For everything else, leave it alone. Depth testing itself is covered in Part 11.

We don't use `gl_FragDepth`.

### Other built-in outputs

For completeness: `gl_SampleMask[]` exists for advanced multisampling (anti-aliasing) work. It's specialized; we don't use it.

## 10.6 Multiple outputs

Most fragment shaders write to a single `out` variable, which goes to a single color attachment in the framebuffer. But the language allows multiple outputs at once:

```glsl
layout (location = 0) out vec4 FragAlbedo;     // → color attachment 0
layout (location = 1) out vec4 FragNormal;     // → color attachment 1
layout (location = 2) out vec4 FragPosition;   // → color attachment 2
```

Each `out` is assigned to a specific **color attachment** of the bound framebuffer via the `layout (location = N)` qualifier. The framebuffer must have multiple color attachments configured for this to work — a default screen-only framebuffer has only attachment 0. Multi-attachment framebuffers are covered in Part 11.

### Why you'd use this — deferred rendering

The main reason fragment shaders need multiple outputs is a technique called **deferred rendering**. In a single draw pass, the fragment shader writes:

- The surface color into attachment 0.
- The surface normal into attachment 1.
- The world-space position into attachment 2.

Then a *second* pass reads from those attachments and computes lighting. This decouples geometry-pass cost from lighting-pass cost, which is a big win in scenes with many lights. Brief coverage of deferred rendering is in Part 11.5.

### Single-output is normal

For our project, single-output rendering is what we use. Multiple outputs are a specialized feature for advanced rendering techniques, and you only set them up when you specifically need them.

---

## Part 10 summary

- A fragment shader runs **once per fragment** generated by rasterization. A fragment is a pixel-sized candidate output, distinct from both a primitive (its source) and a pixel (the eventual result in the framebuffer).
- It reads **interpolated varyings** through `in` declarations that pair with the vertex shader's `out` declarations by name (or by location).
- It has **built-in inputs** with reserved `gl_` names: `gl_FragCoord` (window-space pixel position) is the most useful; `gl_FrontFacing`, `gl_PointCoord`, `gl_PrimitiveID` are situational.
- Its **output** is an RGBA color via a user-declared `out vec4`. With one output, OpenGL writes to color attachment 0 automatically; per-fragment operations and the framebuffer are covered in Part 11.
- It has one main **built-in output**, `gl_FragDepth`, which overrides the geometric depth — useful for imposter rendering and similar tricks but disables the early-depth-test optimization.
- Multiple outputs are possible (assigned to different color attachments via `layout (location = N) out`), used in techniques like deferred rendering. Single-output is normal for direct rendering.

---

# Part 11: Per-fragment operations and the framebuffer

After the fragment shader produces a candidate color for a fragment, the value isn't immediately a pixel on screen. It first goes through a chain of optional **per-fragment operations** — depth testing, stencil testing, blending — and finally lands in the **framebuffer**, the actual pixel storage on the GPU. This Part covers each of those concepts in order.

You've seen the names mentioned several times in earlier sections (1.3, 5.4, 9.4, 10.1, 10.4, 10.5, 10.6) with forward references here. This is where they're properly explained.

## 11.1 The framebuffer

A **framebuffer** is a collection of images the GPU draws into. It's not a single image — it's a *bundle*, with separate slots for different kinds of data:

- **Color attachments** — one or more images holding the rendered colors. Most programs use one (the screen image).
- **Depth attachment** — an image holding a depth value per pixel, used by depth testing (11.2).
- **Stencil attachment** — an image holding an integer per pixel, used by stencil testing (11.3).

A "draw call" writes its output into the **currently bound framebuffer**'s attachments.

### The default framebuffer

When you create a window with GLFW (or any other windowing system), OpenGL automatically creates a **default framebuffer** for you. Its color attachment is the screen, and it usually comes with a depth attachment too (whether you use it or not).

You don't have to set up the default framebuffer — it's just there. By default, every draw call targets it. That's why "the framebuffer" and "the screen" are interchangeable in simple programs.

### Custom framebuffers (FBOs)

You can also create your own framebuffer — called a **Framebuffer Object** (FBO) — that doesn't render to the screen. Instead, its attachments are images you allocated yourself. The pattern is:

```cpp
GLuint fbo;
glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);

// attach a texture as color attachment 0
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, myTexture, 0);
// attach a renderbuffer for depth
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, myDepthRB);
```

Now any draws made while this FBO is bound write into your custom attachments instead of the screen. Use cases include **render-to-texture** (drawing a scene into a texture so it can be sampled in a later pass), **post-processing** (rendering the world to an FBO, then drawing a fullscreen quad that reads it as input), and the techniques in 11.5.

To switch back to the default framebuffer:

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

A bind value of `0` means "the default framebuffer."

### What we use

Our project uses the default framebuffer only. We never create an FBO. Every draw goes straight to the screen. That's the simplest setup and is enough for most direct-rendering programs.

## 11.2 Depth testing

**Depth testing** is the per-fragment operation that decides whether a fragment is in front of, or behind, what's already drawn at the same pixel. If it's behind, the fragment is thrown away.

This solves the **occlusion problem** in 3D: when two objects overlap on screen, the closer one should hide the farther one. Without depth testing, drawing order would determine visibility — you'd have to sort all your geometry back-to-front before every frame, which is slow and brittle. With depth testing, geometry can be drawn in any order; the GPU figures out the visibility per pixel.

### How it works

Each pixel in the depth attachment holds a depth value where **0 means near** (closest to the camera) and **1 means far** (furthest), typically stored in the range `[0, 1]`. When a fragment arrives with its `gl_FragCoord.z`:

1. The GPU compares the fragment's depth to the value already stored at that pixel.
2. If the comparison passes, the fragment proceeds (and the stored depth is updated to the fragment's depth).
3. If it fails, the fragment is **discarded** — no color write, no further per-fragment operations.

The default comparison is `GL_LESS` (keep the fragment if its depth is *less* than what's stored, meaning it's closer to the camera).

### Enabling and configuring

Depth testing is **off by default**. Enable it with:

```cpp
glEnable(GL_DEPTH_TEST);
```

Change the comparison if needed:

```cpp
glDepthFunc(GL_LESS);          // default — keep if closer
glDepthFunc(GL_LEQUAL);        // keep if closer or equal (useful for some effects)
glDepthFunc(GL_GREATER);       // keep if farther (rare, but valid)
glDepthFunc(GL_ALWAYS);        // always pass (essentially disables the test)
```

Toggle depth writes (whether passing fragments update the stored depth):

```cpp
glDepthMask(GL_FALSE);   // fragments still test against depth, but don't update it
glDepthMask(GL_TRUE);    // default — passing fragments update the stored depth
```

`glDepthMask(GL_FALSE)` is useful when drawing transparent objects: you want them to respect what's behind them but not occlude things drawn after.

### Clearing the depth attachment

At the start of each frame, you typically reset the depth values to "far" (`1.0`):

```cpp
glClearDepth(1.0);                       // configure clear value (default is 1.0)
glClear(GL_DEPTH_BUFFER_BIT);            // clear the depth attachment
```

You usually clear color and depth together:

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

### What we use

Our project doesn't enable depth testing. We have a single fullscreen quad — there's nothing to occlude anything else. With one piece of geometry, depth testing would just add overhead with no visible effect.

## 11.3 Stencil testing

**Stencil testing** is a more flexible per-fragment test against an integer value stored in the **stencil attachment**. Unlike depth (which is a single comparison against a depth value), stencil lets you read, compare, and update an integer per pixel using configurable rules.

It's useful for:

- **Masking** — restrict drawing to a specific region of the screen by marking it in the stencil first.
- **Outlines** — draw an object, then draw a slightly larger version where the stencil says "outside the original." Produces clean outline borders.
- **Portals and mirrors** — render the inside of a portal only where the portal shape is on screen.
- **Shadow volumes** — a classical real-time shadow technique built on stencil math.

### How it works

The stencil attachment holds an 8-bit integer per pixel. When a fragment arrives:

1. The GPU reads the current stencil value at that pixel.
2. It compares it against a reference value using a function you configure.
3. If the test fails, the fragment is discarded.
4. If it passes (or fails — there are separate cases), the GPU may *update* the stored stencil value according to a rule you configure.

So unlike depth, stencil has both a **test** and an **update** that can be configured separately, with three possible outcomes (test fails, test passes but depth fails, both pass).

### The two key functions

```cpp
glEnable(GL_STENCIL_TEST);

glStencilFunc(GL_EQUAL, 1, 0xFF);        // pass if (stored & mask) == (ref & mask)
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);  // (stencil-fail, depth-fail, both-pass) actions
```

`glStencilFunc(func, ref, mask)` controls the test. Common comparison functions: `GL_EQUAL`, `GL_NOTEQUAL`, `GL_LESS`, `GL_ALWAYS`.

`glStencilOp(sfail, dpfail, dppass)` controls what happens to the stored value in each outcome. Common operations: `GL_KEEP` (don't change), `GL_REPLACE` (set to ref), `GL_INCR`, `GL_DECR`, `GL_ZERO`.

Stencil testing is genuinely powerful but has a steep learning curve. Most simple programs never enable it. If you later run into a need for masking, drawing outlines, rendering portals or mirrors, or implementing classic shadow-volume shadows, this section is the place to come back to and study in depth.

### What we use

Our project doesn't enable stencil testing. Single fullscreen quad rendering doesn't need masking.

## 11.4 Blending

**Blending** combines a fragment's color with the existing color already in the framebuffer at that pixel, instead of overwriting it. This is how transparency, glow, and many other effects are implemented.

### How it works

When a fragment arrives at a pixel that already has a color (from previous draws this frame):

```
final_color  =  src_factor × source_color  ⊕  dst_factor × destination_color
```

- **Source color** — the fragment's color (what the fragment shader output).
- **Destination color** — what's already in the framebuffer at this pixel.
- **Source factor / destination factor** — per-channel multipliers you configure.
- **`⊕`** — usually addition, but can be other operations (subtraction, min, max).

The default operation is addition. You configure the factors via `glBlendFunc`.

### Enabling and configuring

Blending is **off by default**. Enable it with:

```cpp
glEnable(GL_BLEND);
```

Set the factors:

```cpp
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

This is the most common configuration — **standard alpha blending**, suitable for transparency:

```
final  =  src.alpha × src_color  +  (1 − src.alpha) × dst_color
```

A fully opaque fragment (`alpha = 1.0`) overwrites the destination. A fully transparent fragment (`alpha = 0.0`) leaves the destination unchanged. Anything in between mixes them.

### Other common blend modes

| Configuration | Effect |
|---|---|
| `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` | Standard alpha blending (transparency). |
| `glBlendFunc(GL_ONE, GL_ONE)` | Additive blending — adds source on top of destination. Used for glows, fire, particles. |
| `glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)` | Premultiplied-alpha blending — assumes the source color was already pre-multiplied by its alpha. More numerically robust for complex compositing. |
| `glBlendFunc(GL_DST_COLOR, GL_ZERO)` | Multiplicative blending — modulates destination by source. |

The factor names like `GL_SRC_ALPHA` mean "use the source color's alpha channel as the factor." Other useful factor names: `GL_ONE` (factor 1), `GL_ZERO` (factor 0), `GL_DST_ALPHA`, `GL_SRC_COLOR`, `GL_DST_COLOR`, and their `_ONE_MINUS_*` complements.

### Order matters with blending

Unlike depth testing (which sorts itself out), **blended objects need to be drawn in correct back-to-front order** to look right. The destination color used by blending is whatever is *currently* in the framebuffer, so an object drawn first contributes its color first; a transparent object drawn over it sees that color as its destination. If you draw two transparent objects in the wrong order, the result is visibly wrong.

This is why most engines split rendering into "opaque pass" (depth-test enabled, any order) and "transparent pass" (depth-test enabled but depth-write disabled, sorted back-to-front).

### What we use

Our project doesn't explicitly enable blending. However, **ImGui internally enables it** while drawing the control panel, because ImGui widgets use transparency for shadows, hover effects, etc. ImGui sets blending up before its draw calls and tears it down afterward, so we don't have to think about it.

## 11.5 Multiple color attachments and deferred rendering

The framebuffer can have **more than one color attachment** — typically up to 8 simultaneous color attachments, depending on the GPU. Each is a separate image; the fragment shader can write to all of them in one draw call using the multi-output declarations from 10.6.

```glsl
layout (location = 0) out vec4 FragAlbedo;     // → color attachment 0
layout (location = 1) out vec4 FragNormal;     // → color attachment 1
layout (location = 2) out vec4 FragPosition;   // → color attachment 2
```

Setting up an FBO with multiple color attachments looks like:

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedoTex,   0);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex,   0);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, positionTex, 0);

// tell OpenGL which attachments the shader is allowed to write to
GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
glDrawBuffers(3, bufs);
```

After this, every draw made with this FBO bound writes its three outputs into the three textures.

### Deferred rendering — the canonical use case

The reason multiple attachments matter is a rendering technique called **deferred rendering**. It's the standard way modern games handle scenes with many lights.

**Forward rendering** (the simple alternative) computes lighting inside the geometry pass: for each fragment, it samples surface properties and runs the lighting formula in one shader. The cost scales with `objects × lights` — every fragment of every object has to consider every light.

**Deferred rendering** splits the work into two passes:

1. **Geometry pass.** Render the scene with a fragment shader that writes per-fragment surface data — color, normal, position, material id — into multiple color attachments of an FBO. This collection of attachments is called the "G-buffer."
2. **Lighting pass.** Switch to the default framebuffer. Render a fullscreen quad. Its fragment shader reads the G-buffer attachments as input textures and computes the final lighting per pixel. The cost scales with `pixels × lights`, not `objects × lights`.

For scenes with many lights, deferred rendering is dramatically faster than forward rendering. The tradeoffs: more memory bandwidth (multiple G-buffer textures per frame), trickier transparent-object handling (transparent objects don't fit in a G-buffer well), and more setup complexity.

### What we use

Our project uses single-output rendering with the default framebuffer's single color attachment. No FBOs, no G-buffer, no deferred path. Multiple-attachment rendering is a feature you set up only when you need it.

---

## Part 11 summary

- A **framebuffer** is a bundle of images the GPU draws into: color attachments, plus optional depth and stencil attachments. The default framebuffer is the screen; custom framebuffers (FBOs) let you render to your own images.
- **Depth testing** rejects fragments that are behind what's already drawn at the same pixel. Solves occlusion in 3D. Off by default; enable with `glEnable(GL_DEPTH_TEST)`. Configurable via `glDepthFunc`.
- **Stencil testing** is a configurable per-fragment test against integer values in the stencil attachment. Used for masking, outlines, portals, shadow volumes. Powerful but advanced; off by default.
- **Blending** combines a new fragment's color with the existing pixel color using configurable factors. Used for transparency, additive effects, etc. Off by default; enable with `glEnable(GL_BLEND)` and configure with `glBlendFunc`.
- A framebuffer can hold **multiple color attachments**. Each fragment shader output can target a different attachment. This is the foundation of **deferred rendering**, where a geometry pass writes surface data into a G-buffer and a separate lighting pass reads it.
- Our project uses the default framebuffer with one color attachment, no depth test, no stencil test, no explicit blending (ImGui enables blending internally for its UI).

---

# Part 12: Uniforms

We've seen two ways to feed values into a shader: **vertex attributes** (Part 7) for per-vertex data and **interpolated varyings** (Part 10.2) for per-fragment data. The third channel is **uniforms** — values that stay the same across every invocation in a draw call, set from the C++ side once per draw (or less often).

## 12.1 What a uniform is

A **uniform** is a value declared in a shader and set from your C++ code. The same value is visible to every invocation of that draw call — every vertex shader run, every fragment shader run, every other shader stage that's active.

The name "uniform" describes the key property: the value is *uniform* (the same) across the whole draw, regardless of which vertex or fragment is being processed.

### Where it sits relative to attributes and varyings

| Channel | Varies | Set from |
|---|---|---|
| **Attribute** (Part 7) | per vertex | a VBO, configured by `glVertexAttribPointer` |
| **Varying** (Part 10.2) | per fragment | the previous shader stage's `out` (interpolated) |
| **Uniform** | doesn't vary | C++ code, by location, via `glUniform*` |

So uniforms are how the CPU side passes values to the shader that aren't tied to specific vertices or fragments — settings that apply to the whole draw.

### Declaring a uniform

In GLSL, you declare a uniform with the `uniform` keyword:

```glsl
uniform float u_Time;
uniform vec3  u_LightColor;
uniform mat4  u_ViewProjection;
uniform sampler2D u_Field;
```

The type can be any GLSL type: scalars (`float`, `int`, `bool`, `uint`), vectors (`vec2`/`vec3`/`vec4` and their integer/boolean variants), matrices (`mat2`, `mat3`, `mat4`), and sampler types (texture handles, covered in Part 13). Arrays and structs of uniforms are also legal.

### Naming convention

By convention, many projects (including ours) prefix uniforms with `u` or `u_` to mark them at a glance: `u_Time`, `u_RangeMin`, `u_LightPos`. Like the `a` and `v` prefixes from Parts 7 and 10, this is a project convention, not a language requirement.

### Uniforms span the whole program

A program (the linked combination of vertex + fragment shader, from Part 3) is the unit that owns uniform values. **A uniform declared in the vertex shader and a uniform declared in the fragment shader, with the same name and type, refer to the same value.** Set it once from C++; both stages see it.

```glsl
// vertex shader of program P
uniform mat4 u_Model;
void main() {
    gl_Position = u_Model * vec4(aPos, 1.0);
}

// fragment shader of the same program P
uniform mat4 u_Model;   // same uniform — same value as in the vertex shader
```

Don't declare two unrelated uniforms with the same name in the same program — they'd collide.

### Common uses

- **Transformation matrices** — `u_Model`, `u_View`, `u_Projection`. Set once before drawing each object.
- **Time-based values** — `u_Time` for animation, `u_DeltaTime` for physics.
- **Tunable parameters** — colors, scales, thresholds. In our project, `u_RangeMin` and `u_RangeMax` for the colormap.
- **Light positions and colors** in 3D scenes.
- **Texture handles** via sampler uniforms (Part 13).

### What uniforms cost

Setting a uniform is cheap — a few bytes sent to the driver. There's no per-vertex or per-fragment overhead beyond what the shader's read-from-uniform costs. You can change uniforms freely between draw calls without much performance worry. Expensive operations in OpenGL are program switches, VAO switches, and draw-call overhead — uniforms are noise compared to those.

## 12.2 Uniform locations

To set a uniform from C++, you need the **location** of that uniform in the program. Just like vertex attributes (Part 7.2), uniforms are identified by a numeric location — not by name at the API level. Names exist in the shader source for human readability; the C++ side uses numbers.

### Getting a location

After you've linked a program (Part 3), ask OpenGL where a uniform lives:

```cpp
GLint loc = glGetUniformLocation(myProgram, "u_Time");
```

`glGetUniformLocation(program, name)` looks up the uniform by name and returns its integer location, or **`-1`** if no such uniform exists.

Important: **a return value of `-1` does not raise an error**. Setting a uniform at location `-1` is silently a no-op. This is a common source of "I set the uniform but it has no effect" bugs. Always check on first use:

```cpp
GLint loc = glGetUniformLocation(myProgram, "u_Time");
if (loc == -1) {
    fprintf(stderr, "Uniform u_Time not found in program\n");
}
```

### Why a uniform might be missing

There are two reasons the lookup can fail:

1. **You typo'd the name.** Common; easy to fix.
2. **The compiler optimized the uniform away.** If a uniform is declared but never used inside `main()` (or any function `main` calls), the GLSL compiler may remove it from the linked program. The shader source still declares the uniform, but the linked program has no location for it — so the lookup returns `-1`. Reference the uniform somewhere in `main()` and it reappears.

### Locations are per-program

A location is a property of one program. Two programs that both declare `uniform float u_Time` may end up with different locations. So you need to look up the location per program — don't reuse a location across programs.

Locations are **stable** for the lifetime of a program. You can call `glGetUniformLocation` once after linking and cache the result; it won't change unless you re-link the program.

If you do re-link a program (e.g. you recompiled a shader), the locations may change — re-query every cached location after a relink.

### Naming arrays and struct members

For array uniforms, you address each element by index in the lookup:

```glsl
uniform vec3 u_Lights[4];
```

```cpp
GLint locLight0 = glGetUniformLocation(prog, "u_Lights[0]");
GLint locLight1 = glGetUniformLocation(prog, "u_Lights[1]");
```

For struct uniforms, dot notation works:

```glsl
struct Material { vec3 color; float shininess; };
uniform Material u_Material;
```

```cpp
GLint locColor     = glGetUniformLocation(prog, "u_Material.color");
GLint locShininess = glGetUniformLocation(prog, "u_Material.shininess");
```

Each member of a struct has its own location.

### Optional: explicit locations in the shader

Modern GLSL also lets you pin a uniform's location explicitly:

```glsl
layout (location = 0) uniform float u_Time;
```

With this, the location is whatever you wrote (`0` here) — no need to call `glGetUniformLocation`. You'd just use `0` directly from C++.

This is a GLSL 4.30+ feature and somewhat less commonly used than for attributes. Most projects, including ours, look up locations with `glGetUniformLocation` instead.

## 12.3 Setting uniforms from C++

Once you have the location, you set the value with the **`glUniform*` family** of functions. There are many variants because OpenGL needs different signatures for different types and component counts. The naming pattern is:

```
glUniform{count}{type}[v]
```

- **`{count}`** — the number of components per uniform: `1`, `2`, `3`, or `4`.
- **`{type}`** — the data type: `f` (float), `i` (int), `ui` (unsigned int).
- **`v`** (optional suffix) — takes a *pointer* to data plus an array count, instead of separate component arguments.

### Common cases

| GLSL declaration | C++ call |
|---|---|
| `uniform float u_Scale;` | `glUniform1f(loc, 2.5f);` |
| `uniform int   u_Count;` | `glUniform1i(loc, 7);` |
| `uniform vec2  u_Range;` | `glUniform2f(loc, 0.0f, 1.0f);` *or* `glUniform2fv(loc, 1, valuesPtr);` |
| `uniform vec3  u_LightColor;` | `glUniform3f(loc, 1.0f, 0.9f, 0.7f);` |
| `uniform vec4  u_Color;` | `glUniform4f(loc, r, g, b, a);` |
| `uniform mat4  u_View;` | `glUniformMatrix4fv(loc, 1, GL_FALSE, matrixPtr);` |

### The pointer (`v`) variants

For arrays, or when you already have your data laid out in memory, the `*v` variants are cleaner:

```cpp
float lights[3 * 4] = { /* 4 vec3 lights, packed */ };
glUniform3fv(loc, 4, lights);   // 4 vec3 values
```

The second argument (`4`) is the **count** — how many uniforms of the declared size you're setting. For a single uniform you pass `1`.

### Matrices

Matrix uniforms use a slightly different family:

```cpp
glUniformMatrix4fv(loc, count, transpose, ptr);
```

The `transpose` parameter says whether to transpose the matrix on the way to the GPU; usually `GL_FALSE` because most matrix libraries (GLM, Eigen, custom math) already store matrices in column-major order, which matches GLSL's convention.

There are similar functions for `mat2` (`glUniformMatrix2fv`) and `mat3` (`glUniformMatrix3fv`), plus non-square matrices like `mat3x4` (`glUniformMatrix3x4fv`).

### Samplers

A `sampler2D` uniform (texture handle — covered in Part 13) is set with `glUniform1i`, passing the **texture unit number** as an integer:

```cpp
glUniform1i(loc, 0);   // sampler reads from texture unit 0
```

This looks odd at first — you're passing an int into a sampler — but it's how OpenGL connects samplers to bound textures. Full coverage in Part 13.

### The program must be bound

**The `glUniform*` family operates on the currently active program** — the one most recently set with `glUseProgram`. The location you pass is interpreted in that program's context.

```cpp
glUseProgram(myProgram);                    // make myProgram active
GLint loc = glGetUniformLocation(myProgram, "u_Time");
glUniform1f(loc, 12.5f);                    // sets u_Time on myProgram
```

If you forget the `glUseProgram` and a different program is bound, you'd update *that* program's uniform — or get nothing useful if no program is bound.

A newer alternative — Direct State Access — lets you set uniforms without binding the program:

```cpp
glProgramUniform1f(myProgram, loc, 12.5f);  // takes program as first argument
```

Either family works. The older `glUniform*` is more common in tutorials and existing code.

## 12.4 When uniforms are read

A uniform is read from the program's stored state **at the moment a draw call runs**. The shader's `uniform float u_Time;` line resolves to "look up the current value of `u_Time` in the program's uniform storage."

This timing has three practical consequences.

### Set any time before the draw — and the value persists

You don't have to set the uniform immediately before the draw. Once set, the value stays in the program's state until you change it or relink the program. So a typical pattern looks like:

```cpp
glUseProgram(myProgram);

glUniform1f(timeLoc,  currentTime);   // values that don't change per object
glUniform3f(lightLoc, 1.0f, 0.9f, 0.7f);

for (Object& obj : objects) {
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, obj.modelMatrix);   // per-object value
    glBindVertexArray(obj.vao);
    glDrawElements(GL_TRIANGLES, obj.indexCount, GL_UNSIGNED_INT, nullptr);
}
```

`u_Time` and `u_LightColor` are set once before the loop. `u_Model` is updated per object. Each draw inside the loop uses the current values of all three.

### Each program owns its own uniform state

Switching programs (`glUseProgram(otherProgram)`) does not carry uniform values across. Each program has its own uniform storage. If both programs declare `uniform float u_Time` and you want them in sync, you must set the uniform on each program separately — or use a **Uniform Buffer Object** (UBO, mentioned in Part 6.2), which lets multiple programs share a uniform block.

### Setting takes effect immediately

There's no "commit" step. The next draw with that program sees the new value. There's no batching, no flushing — `glUniform*` updates the state, and that's it.

### Performance note

Setting a uniform is cheap, but not free. Some engines cache the last-set value per uniform per program to skip redundant `glUniform*` calls when the value hasn't changed. That's a micro-optimization, useful in tight inner loops but rarely a critical bottleneck. The expensive things in OpenGL are program switches, VAO switches, and the per-draw-call overhead — uniforms are minor compared to any of those.

---

## Part 12 summary

- A **uniform** is a value declared in a shader and set from C++; the same value is visible to every invocation in a draw call. Distinct from attributes (per-vertex) and varyings (per-fragment).
- Any GLSL type works: scalars, vectors, matrices, samplers (texture handles — Part 13). Conventional `u_` prefix.
- A **program** owns its uniform values. The same uniform name in both stages of a program refers to the same value. Different programs have separate uniform state.
- The C++ side identifies a uniform by **location**, obtained via `glGetUniformLocation(program, name)`. A return of `-1` means the uniform isn't there or was optimized away — setting at `-1` is a silent no-op.
- Set with the **`glUniform*` family**: pattern `glUniform{count}{type}[v]`. Matrices use `glUniformMatrix{N}fv`. Samplers use `glUniform1i` with a texture unit number. The program must be bound (or use the `glProgramUniform*` variants that take a program handle).
- Uniforms are read **when a draw call runs**. Values persist in the program's state until changed or until the program is relinked. Set once, draw many — only update what changes per draw.

---

# Part 13: Textures and samplers

A **texture** is the GPU's specialized data structure for "data laid out in 2D (or 3D) that you read by coordinate." Most commonly, it's the image data sampled by a fragment shader to color a surface. But textures can hold any kind of structured numeric data — heightmaps, lookup tables, simulation fields, etc. Our project's field-display path is exactly that: the simulation writes a 2D float array into a texture, and the fragment shader samples it.

This Part covers what textures are on the GPU, how the shader connects to them through samplers, and how to control sampling behavior (filtering, wrapping).

## 13.1 What a texture is on the GPU

A **texture** is a chunk of GPU memory (VRAM, like buffers from Part 6) that holds a structured array of values — typically a 2D grid for image-like data, but other layouts exist. What makes textures distinct from buffers is that they're **specialized for sampling**: hardware support for reading them by floating-point coordinate, with filtering and wrapping built in.

### Texels, not pixels

Each element of a texture is called a **texel**. A texture isn't on screen, so calling its elements "pixels" would be misleading — pixels are the addressable slots in the framebuffer (Part 11.1). Texels are the addressable values inside a texture; they get *sampled* by shaders to compute pixel colors.

A 256×256 RGBA8 texture has 256 × 256 = 65,536 texels, each four bytes (one byte per channel).

### The internal format

When you create a texture, you specify an **internal format** that defines what each texel holds and how it's stored on the GPU. Common formats:

| Internal format | What each texel holds | Use case |
|---|---|---|
| `GL_R8` | one 8-bit unsigned int (`[0, 255]`) | grayscale image, mask |
| `GL_RGB8`, `GL_RGBA8` | three / four 8-bit channels | typical color images |
| `GL_R32F` | one 32-bit float | scalar field, heightmap. **What our project uses** |
| `GL_RGB32F`, `GL_RGBA32F` | three / four 32-bit floats | high-precision images, HDR data |
| `GL_R16I` | one 16-bit signed int | integer data |
| `GL_DEPTH_COMPONENT24` | 24-bit depth value | depth textures (e.g. shadow mapping — render the scene from a light's view into a depth texture, then sample it later to test which surfaces are in shadow) |

The internal format is fixed at allocation. You decide what kind of data the texture is intended to hold; the shader's sampler reads values consistent with that format.

### Texture types (targets)

OpenGL supports several texture *targets* — different layouts:

| Target | Layout |
|---|---|
| `GL_TEXTURE_1D` | 1D array of texels |
| `GL_TEXTURE_2D` | 2D grid of texels (most common) |
| `GL_TEXTURE_3D` | 3D volume of texels |
| `GL_TEXTURE_CUBE_MAP` | 6 square 2D faces forming a cube — for skyboxes, environment mapping |
| `GL_TEXTURE_2D_ARRAY` | a stack of 2D textures, indexed as a third coordinate |

Like buffers (Part 6.2), the target you bind a texture to determines what role it plays. We use `GL_TEXTURE_2D` for the simulation field.

### Contrast with buffers

A buffer (Part 6) and a texture both live in VRAM, but they're accessed very differently:

| Aspect | Buffer | Texture |
|---|---|---|
| Layout | flat sequence of bytes | 2D / 3D grid of texels |
| Access | by byte offset | by floating-point coordinate (`[0, 1]` per axis, typically) |
| Filtering | none — exact reads | bilinear / trilinear in hardware (Part 13.6) |
| Wrapping | none | configurable wrap modes (Part 13.7) |

If you want indexed-by-integer reads of unstructured data, a buffer fits. If you want spatial reads of grid-laid-out data — and you want filtering for free — a texture fits.

### Creating and uploading

Setup looks similar to buffers:

```cpp
GLuint tex;
glGenTextures(1, &tex);
glBindTexture(GL_TEXTURE_2D, tex);

// allocate storage of the chosen internal format
glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, dataPtr);
```

`glTexImage2D` allocates space for one **mip level** (level 0 = base, full resolution) and optionally uploads data. The `GL_R32F` is the internal format; `GL_RED, GL_FLOAT` describes the *source* data layout (what's at `dataPtr`). The two formats can differ — OpenGL converts during upload.

To later **update** a region without re-allocating:

```cpp
glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_FLOAT, dataPtr);
```

Our project uploads the whole field every frame via `glTexSubImage2D`, since the simulation values change continuously.

## 13.2 Texture units — what they are, how many

### First: what "sampling" means

Before we can talk about texture units, we need to nail down what a shader actually does with a texture. The verb is **sample**.

When a shader samples a texture, it asks: *"what value is at this coordinate?"* The shader provides a texture coordinate — a `vec2` for a 2D texture, like `(0.5, 0.7)` — and gets back a value read from the texture at that position. In GLSL it looks like this:

```glsl
vec4 value = texture(someTexture, vec2(0.5, 0.7));
```

We'll cover the `texture()` function fully in 13.5. For now, the picture is:

```
shader: "What value is at coordinate (0.5, 0.7) of this texture?"
GPU:    [reads the texture at that coordinate, possibly with filtering]  →  vec4 value
```

That act of reading is **sampling**. Every time a shader uses a texture, it's via one or more sampling calls.

### Why a shader might sample more than one texture

A shader often needs to read from multiple textures in a single draw call:

- A textured object might combine a base color image with a separate detail-noise image — two textures in one draw.
- A post-processing pass might sample the rendered frame as one texture and a lookup table as another — two textures.
- A complex 3D shader might pull from four or five — color, surface bumps, shininess, environment, etc.

So OpenGL has to answer: **how does a shader say which texture it's sampling, when there are several available?**

### Texture units — numbered slots between texture and shader

OpenGL's answer is **texture units**. A texture unit is a numbered slot — `unit 0`, `unit 1`, `unit 2`, and so on — where a texture lives for the duration of a draw call.

The flow:

1. The C++ side **binds a texture into a unit** before drawing (covered in 13.3).
2. The shader has a **sampler uniform** that holds the unit number it should read from (covered in 13.4).
3. When the shader calls `texture(samplerUniform, uv)`, it samples whatever texture is bound to the unit that sampler points at.

A diagram of this two-side connection:

```
   Shader-side declarations:               OpenGL state on C++ side:

   uniform sampler2D u_Foo;  ──reads from──► Texture unit 0 ──holds──► some texture (bound by C++)
   uniform sampler2D u_Bar;  ──reads from──► Texture unit 1 ──holds──► some texture (bound by C++)
   uniform sampler2D u_Baz;  ──reads from──► Texture unit 2 ──holds──► some texture (bound by C++)
```

Each sampler in the shader points at one unit. Each unit can hold one texture (per target). The shader doesn't see the texture handle; it sees the unit number, dereferences it, and gets the texture that's currently bound there.

### Why the indirection

You might ask: why insert "units" between shader and texture at all? Why not let the shader refer to a texture directly?

The reason is the same as for vertex attributes (Part 7.2). The shader source and the C++ source are compiled separately. They never see each other's variables; the only thing they can share is **numbers**. Texture units provide that numeric anchor: the shader says "I read from unit 2," and the C++ side independently arranges for unit 2 to hold whatever texture it wants. The shader doesn't have to know which specific texture; the C++ side doesn't have to know the shader's variable names. They meet in the middle, at unit 2.

### How many units do you have

The OpenGL spec guarantees at least these counts:

| Constant | Minimum value | What it counts |
|---|---|---|
| `GL_MAX_TEXTURE_IMAGE_UNITS` | 16 | Units accessible from a fragment shader |
| `GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS` | 16 | Units accessible from a vertex shader |
| `GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS` | 32 | Total units across all shader stages |

Modern GPUs typically allow many more — 32, 192, or higher, depending on hardware. You can query the actual numbers at runtime:

```cpp
GLint maxFragUnits;
glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragUnits);
// often 32 or higher
```

In practice you'll almost never run out. Even a complex 3D shader rarely uses more than around 8 textures at once.

### Unit numbers are not the same as texture handles

Texture unit `0` is just a slot. You can put any 2D texture in it, swap it for another, or leave it empty. The handle returned by `glGenTextures` (an integer like `1`, `2`, `3`...) is unrelated to the unit number:

- A **handle** identifies a particular texture object — allocated once when you create the texture, kept until you delete it.
- A **unit number** identifies a slot in the GPU's binding state — always there, can hold any texture (or none).

So saying "the texture handle is 7" and "the texture is bound to unit 0" are different things. The handle says *which texture*; the unit says *where* it's currently visible to shaders.

### Multiple targets per unit

A single unit actually has separate sub-slots for different texture targets — one for `GL_TEXTURE_2D`, one for `GL_TEXTURE_3D`, one for `GL_TEXTURE_CUBE_MAP`, and so on. So unit 0 can have *both* a 2D texture and a 3D texture bound at once, in different sub-slots.

Which one a shader sees depends on the sampler type: a `sampler2D` samples the 2D sub-slot of its unit; a `sampler3D` samples the 3D sub-slot. In practice you almost always use just one target per unit, so this is a corner you can usually ignore — but it's why `glBindTexture` takes a target parameter (covered in 13.3).

### Our project

Our project uses one texture (the simulation field) on one unit (unit 0). The shader has one `sampler2D` uniform pointing at that unit. Most simple programs look like this. Multi-texture setups become important for 3D rendering and post-processing effects.

## 13.3 Binding a texture to a unit

Binding a texture to a unit is a **two-step state-machine operation**:

1. **Select the active unit** — `glActiveTexture(GL_TEXTURE0 + N)`. This sets the global "currently active texture unit" slot.
2. **Bind a texture to a target on that unit** — `glBindTexture(target, textureHandle)`. This sets the currently active unit's slot for that target.

```cpp
glActiveTexture(GL_TEXTURE0);              // make unit 0 active
glBindTexture(GL_TEXTURE_2D, myTexture);   // unit 0's GL_TEXTURE_2D slot ← myTexture
```

After these two calls, unit 0 has `myTexture` bound on its 2D target.

### `GL_TEXTURE0`, `GL_TEXTURE1`, ...

These are enum constants defined so that `GL_TEXTURE0 + N == GL_TEXTURE_N`. So you can write:

```cpp
glActiveTexture(GL_TEXTURE3);              // explicit form
glActiveTexture(GL_TEXTURE0 + 3);          // equivalent
```

Both pick unit 3.

### Binding multiple textures

For a draw that uses several textures, you bind each one to its own unit:

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, albedoTex);

glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, normalTex);

glActiveTexture(GL_TEXTURE2);
glBindTexture(GL_TEXTURE_2D, roughnessTex);
```

After this, units 0, 1, 2 each have a different texture bound. The shader's three sampler uniforms (covered in 13.4) point at these units by number.

### State-machine view

Recall from Part 4: most OpenGL state lives in slots. The texture-binding state has a layered structure:

```
Global state:
    "currently active texture unit" ────► one of GL_TEXTURE0, GL_TEXTURE1, ...

Per unit (one set of slots per unit):
    GL_TEXTURE_2D    slot ────► some 2D texture handle (or 0)
    GL_TEXTURE_3D    slot ────► some 3D texture handle (or 0)
    GL_TEXTURE_CUBE  slot ────► some cubemap handle (or 0)
    ...
```

`glActiveTexture(...)` changes the global "active unit" pointer. `glBindTexture(target, handle)` changes the slot at `(active unit, target)`.

This is why both calls are needed — `glActiveTexture` selects which unit's slots you're going to modify, and `glBindTexture` does the modification.

### What we do

Our project binds its single field texture to unit 0:

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, fieldTex);
```

After this and the corresponding shader-side configuration (covered in 13.4), the fragment shader can sample the field.

## 13.4 Samplers and how they reference units

A **sampler** is a GLSL type that represents a texture the shader can read from. The key thing to know up front: **a sampler uniform does not hold a texture handle — it holds a texture unit number.** Everything else in this section is detail around that idea.

You declare a sampler as a uniform:

```glsl
uniform sampler2D u_Field;
uniform sampler3D u_Volume;
uniform samplerCube u_Skybox;
```

The sampler type determines what kind of texture target it can read from:

| GLSL type | Reads from |
|---|---|
| `sampler1D` | a `GL_TEXTURE_1D` |
| `sampler2D` | a `GL_TEXTURE_2D` |
| `sampler3D` | a `GL_TEXTURE_3D` |
| `samplerCube` | a `GL_TEXTURE_CUBE_MAP` |
| `sampler2DArray` | a `GL_TEXTURE_2D_ARRAY` |
| `isampler2D` / `usampler2D` | integer-format 2D textures (`GL_R16I`, `GL_R8UI`, etc.) |

The sampler type must match the bound texture's target and basic format type (float / int / unsigned int). A mismatch is undefined behavior.

### What the sampler actually holds

Here's the surprising part: **a sampler uniform doesn't hold a texture handle. It holds a texture unit number.**

You set a sampler uniform with `glUniform1i`, passing the unit number as an integer:

```cpp
glUniform1i(glGetUniformLocation(prog, "u_Field"), 0);   // u_Field reads from unit 0
```

The `0` here means "the texture unit numbered 0" — i.e., whatever texture is bound to unit 0 at draw time. The sampler doesn't care which texture is there; it just dereferences the unit.

This is why setting up texture access is a two-side operation. The C++ side must do both:

1. **Bind a texture to a unit** (`glActiveTexture` + `glBindTexture`, covered in 13.3).
2. **Tell the sampler which unit to read from** (`glUniform1i` with the unit number).

Then in the shader, every call to `texture(u_Field, ...)` follows the unit pointer to whatever's bound there.

### The two sides side by side

To see the full connection in one place:

```glsl
// shader side
uniform sampler2D u_Field;

void main() {
    vec4 v = texture(u_Field, uv);
    // ...
}
```

```cpp
// C++ side, before drawing
glActiveTexture(GL_TEXTURE0);                                  // 1. activate unit 0
glBindTexture(GL_TEXTURE_2D, fieldTex);                        // 2. bind texture to it
glUniform1i(glGetUniformLocation(prog, "u_Field"), 0);         // 3. point sampler at unit 0
```

Three calls on the C++ side hook the shader's `u_Field` to the actual texture data. None of the three knows about the others by name — they meet at the unit number `0`.

### Default sampler value

If you never call `glUniform1i` on a sampler, its default value is `0` — meaning it reads from unit 0. So if you only have one texture and you bound it to unit 0, you can technically skip the `glUniform1i` call. But it's good practice to set it explicitly so the connection is visible in the code.

### Our project

Our project has one texture (the simulation field) and one sampler:

```glsl
uniform sampler2D u_Field;
```

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, fieldTex);
glUniform1i(glGetUniformLocation(prog, "u_Field"), 0);
```

`u_Field` ↔ unit 0 ↔ `fieldTex`. One straight chain.

## 13.5 The `texture()` function

To read a texel from a texture, the shader calls the built-in `texture()` function:

```glsl
vec4 value = texture(u_Field, vec2(0.5, 0.5));
```

Two arguments:

- A sampler uniform (which unit to read from).
- A **texture coordinate** (a `vec2` for 2D textures, `vec3` for 3D or cubemaps).

The return is always a `vec4`, regardless of the texture's internal format. For single-channel textures like `R32F`, only the `.r` component is meaningful — the rest are filled with `(0, 0, 1)` by default.

```glsl
float scalar = texture(u_Field, uv).r;   // read R32F texture
vec4  color  = texture(u_Albedo, uv);    // read RGBA8 texture
```

### Texture coordinates

Texture coordinates are typically in the range `[0, 1]` on each axis, regardless of the texture's pixel resolution:

- `(0, 0)` is one corner of the texture (conventionally bottom-left).
- `(1, 1)` is the opposite corner (top-right).
- `(0.5, 0.5)` is the center.

This normalization means the same shader works for any resolution. A `64×64` and a `4096×4096` texture both read identically — `vec2(0.5, 0.5)` returns the center texel of either.

### What happens between texels

Texture coordinates are continuous floats, but texels are discrete. When `(0.3147, 0.829)` doesn't land exactly on a texel center, the GPU has to decide what to return — pick the nearest texel, or blend the surrounding ones? That decision is controlled by **filtering**, the rule for resolving between-texel coordinates (covered in 13.6).

### What happens outside `[0, 1]`

When a coordinate is, say, `(1.5, -0.3)`, the GPU has to decide what to return for a coordinate outside the texture's natural range — wrap around, clamp, return a border color? That decision is controlled by **wrapping**, the rule for handling out-of-range coordinates (covered in 13.7).

### Other sampling functions

Beyond `texture()`, GLSL offers more specialized variants:

| Function | What it does |
|---|---|
| `texelFetch(sampler, ivec2(x,y), lod)` | Read one texel by **integer** pixel coordinate, no filtering, no wrapping. |
| `textureLod(sampler, uv, lod)` | Sample at an explicit mipmap level (covered in 13.6). |
| `textureGrad(sampler, uv, dPdx, dPdy)` | Sample with explicit derivatives (controls anisotropic sampling). |
| `textureOffset(sampler, uv, offset)` | Sample with a constant integer offset added to the coordinate. |

Most shaders only need `texture()`. We use only `texture()`.

## 13.6 Filtering

When a texture coordinate doesn't land exactly on a texel center, **filtering** controls what value the sampler returns. Two filter settings, configured per texture:

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

- **`GL_TEXTURE_MIN_FILTER`** — used when the texture appears *smaller* than its native size on screen (one screen pixel covers many texels). Called "minification."
- **`GL_TEXTURE_MAG_FILTER`** — used when the texture appears *larger* than its native size on screen (one texel covers many screen pixels). Called "magnification."

The two settings are independent. You can use one filter for downsampling and a different one for upsampling.

### The two basic filter modes

| Mode | What it does at a coordinate that lands between texels |
|---|---|
| `GL_NEAREST` | Returns the value of the closest single texel — pick one neighbor, ignore the others. Result is blocky / pixelated. Good for pixel art and discrete-value data. |
| `GL_LINEAR` | Returns a weighted blend of the 4 nearest texels (in 2D), weighted by distance to the requested coordinate. **Bilinear interpolation.** Smooth result. Good for natural images and continuous data. |

Both are hardware-accelerated. `GL_LINEAR` is essentially free on modern GPUs.

### A worked example

Take a `4×4` texture with this layout:

```
   ┌────┬────┬────┬────┐
   │ 0  │ 1  │ 0  │ 1  │
   ├────┼────┼────┼────┤
   │ 1  │ 0  │ 1  │ 0  │
   ├────┼────┼────┼────┤
   │ 0  │ 1  │ 0  │ 1  │
   ├────┼────┼────┼────┤
   │ 1  │ 0  │ 1  │ 0  │
   └────┴────┴────┴────┘
```

Suppose the shader samples this texture at coordinate `(0.5, 0.5)` — the dead center. Suppose further that the texture is being displayed *larger* than its native 4×4 size on screen — one texel covers many screen pixels. That makes this a magnification case, so the **`GL_TEXTURE_MAG_FILTER`** setting is the one in effect for this sample.

- With `GL_TEXTURE_MAG_FILTER = GL_NEAREST`: the GPU picks the texel whose center is closest to the requested coordinate. With 4×4 texels and `(0.5, 0.5)`, the four central texels are equidistant; the GPU picks one (implementation-defined). You'd get either `0` or `1`.
- With `GL_TEXTURE_MAG_FILTER = GL_LINEAR`: the GPU blends the four central texels, each with weight 0.25. You'd get `0.5`.

The blending is what gives gradient-smooth output across the texture, even when texel values are discrete.

The same math applies to `GL_TEXTURE_MIN_FILTER` when the texture is *smaller* than native size on screen — `GL_NEAREST` picks one texel; `GL_LINEAR` blends the four nearest. Whichever filter setting is active for the current display scale (MIN or MAG) is what's consulted.

### Mipmapping (briefly)

For the minification case (texture displayed smaller than native), simple `GL_LINEAR` filtering — also called **bilinear filtering**, because it blends 4 texels in a 2D pattern (linear in each axis) — can produce aliasing when one screen pixel covers many texels. The 4-texel blend can't represent the average of dozens or hundreds of texels accurately. The standard solution is **mipmaps**: pre-computed downsampled copies of the texture at progressively smaller resolutions. The GPU picks the appropriate level based on how shrunk the texture appears, then samples that level.

Generating mipmaps after upload:

```cpp
glGenerateMipmap(GL_TEXTURE_2D);
```

Mipmap-aware filter modes:

- `GL_NEAREST_MIPMAP_NEAREST` — pick one mip level, nearest filter within it.
- `GL_LINEAR_MIPMAP_NEAREST` — pick one mip level, linear filter within it.
- `GL_LINEAR_MIPMAP_LINEAR` — blend two mip levels, linear filter in each. Called **trilinear** filtering.

These can only be used for `GL_TEXTURE_MIN_FILTER`. The MAG filter is always one of the two basic modes (mipmaps don't help when upsampling).

### Our project

Our field texture sets `GL_LINEAR` for **both** `GL_TEXTURE_MIN_FILTER` and `GL_TEXTURE_MAG_FILTER`, and uses no mipmaps. The texture is a 2D array of float values, displayed at a higher resolution than its native size, so the magnification setting is the one usually consulted in practice — but we set both to `GL_LINEAR` so the texture would look right under either scale. We want smooth interpolation between texels so the field looks continuous, not blocky.

## 13.7 Wrapping

When a texture coordinate falls **outside** the standard `[0, 1]` range, **wrapping** controls what value the sampler returns. Settings per axis (S = horizontal, T = vertical, R = third axis for 3D textures):

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

The two axes can have different modes; you'd typically use the same on both unless you have a reason not to.

### The four common wrap modes

| Mode | What it does at coordinate `1.5` (an example out-of-range value) |
|---|---|
| `GL_REPEAT` | Wraps to `0.5`. The texture tiles infinitely in both directions. |
| `GL_MIRRORED_REPEAT` | Wraps to `0.5`, but mirrored — even tiles flip horizontally relative to odd tiles. |
| `GL_CLAMP_TO_EDGE` | Clamps to `1.0`. Out-of-range coordinates return the edge texel. |
| `GL_CLAMP_TO_BORDER` | Returns a configurable border color (set with `glTexParameterfv(..., GL_TEXTURE_BORDER_COLOR, color)`) for out-of-range coordinates. |

### Choosing a mode

- **`GL_REPEAT`** is for tileable textures — bricks, grass, woodgrain. The texture is designed to repeat seamlessly, and you want the GPU to tile it across a surface that needs more than one repetition.
- **`GL_MIRRORED_REPEAT`** is also for tileable surfaces, but produces seamless tiling even when the source texture isn't perfectly tileable: alternating mirrored copies hide the seams.
- **`GL_CLAMP_TO_EDGE`** is for textures that aren't supposed to repeat — photographs, lookup tables, simulation fields. Out-of-range reads return the edge texel, which avoids visible artifacts at borders.
- **`GL_CLAMP_TO_BORDER`** is rare. Useful when out-of-range reads should clearly be "outside the texture" — for masks, where outside might be black or transparent.

### Why this matters even if your coordinates "stay in range"

Sometimes you think your coordinates are in `[0, 1]`, but they aren't quite — floating-point math, edge effects from filtering, animated UV scrolling. Wrapping handles those edge cases. The mode you pick determines what visual artifacts (or absence of them) appear at the boundary.

### Our project

Our field texture uses `GL_CLAMP_TO_EDGE` on both axes. The simulation field is a fixed grid with hard boundaries — we don't want sampling near the edges to wrap to the opposite side and produce phantom continuity. Edge clamping keeps the visualization honest.

---

## Part 13 summary

- A **texture** is a chunk of GPU memory laid out as a grid of texels, designed for fast sampling. Its **internal format** (`GL_R32F`, `GL_RGBA8`, etc.) determines what each texel holds. Distinct from a buffer (linear bytes, no sampling features).
- **Texture units** are numbered slots that decouple the shader (which references a unit number through a sampler) from the C++ side (which binds a texture to a unit). The OpenGL spec guarantees at least 16 fragment-shader units; modern GPUs offer more.
- **Binding a texture to a unit** is two steps: `glActiveTexture(GL_TEXTURE0 + N)` selects the active unit, then `glBindTexture(target, handle)` binds the texture into that unit's slot.
- A **sampler** is a GLSL uniform that holds a texture unit number, not a handle. Set with `glUniform1i(loc, unitNumber)`. The sampler's type (`sampler2D`, `sampler3D`, etc.) must match the bound texture's target and basic format.
- The shader reads texels with **`texture(sampler, uv)`**. Returns `vec4` regardless of internal format. Coordinates are in `[0, 1]`. For integer-coordinate reads without filtering, use `texelFetch`.
- **Filtering** controls what the sampler returns when a coordinate falls between texels. `GL_NEAREST` (closest texel, blocky) or `GL_LINEAR` (4-texel weighted blend, smooth). Set independently for minification and magnification.
- **Wrapping** controls what the sampler returns when a coordinate falls outside `[0, 1]`. `GL_REPEAT` (tile), `GL_MIRRORED_REPEAT` (mirrored tile), `GL_CLAMP_TO_EDGE` (clamp to nearest valid coord), `GL_CLAMP_TO_BORDER` (return a fixed border color).
- Our project uses an `R32F` 2D texture, on unit 0, with `GL_LINEAR` filtering and `GL_CLAMP_TO_EDGE` wrapping.

---

# Part 14: GLSL — the language

We've been writing GLSL throughout the document — the shader source code — but always in the context of *what the shader does* rather than *what GLSL looks like*. This Part covers the language itself: the type system, vector construction, swizzles, and the built-in math library.

GLSL syntax is similar to C and C++ — `if`/`else`, `for`, `while`, `return`, function declarations, structs, comments — but the type system and built-in functions are heavily oriented toward graphics math. This Part focuses on the parts that aren't already obvious from C-family experience.

## 14.1 Scalar and vector types

GLSL's type system has four families: scalars, vectors, matrices, and samplers. Plus the usual `void` and user-defined `struct`s.

### Scalars

| Type | Meaning |
|---|---|
| `bool` | true / false |
| `int` | signed 32-bit integer |
| `uint` | unsigned 32-bit integer |
| `float` | 32-bit IEEE float |
| `double` | 64-bit IEEE float |

`double` is rarely used. Consumer GPUs dedicate far fewer ALU resources to double-precision math than to single-precision; the actual throughput drop is hardware-specific — typically somewhere between 1/2× and 1/32× of float throughput, with most consumer cards landing on the slower end. Check your GPU's datasheet if you need to know the exact ratio. Few graphics calculations need that extra precision anyway. Stick to `float` unless you know you need `double`.

### Vectors

GLSL has 2-, 3-, and 4-component vector types for each basic numeric type:

| Component type | 2-component | 3-component | 4-component |
|---|---|---|---|
| `float` | `vec2` | `vec3` | `vec4` |
| `int` | `ivec2` | `ivec3` | `ivec4` |
| `uint` | `uvec2` | `uvec3` | `uvec4` |
| `bool` | `bvec2` | `bvec3` | `bvec4` |
| `double` | `dvec2` | `dvec3` | `dvec4` |

By far the most common are `vec2`, `vec3`, `vec4`. Integer vectors come up for things like pixel coordinates (`ivec2`) or counts. Boolean vectors come from per-component comparisons (`lessThan(a, b)` returns a `bvec` for vector inputs).

### Matrices

GLSL has square and non-square matrix types:

- **Square**: `mat2`, `mat3`, `mat4` — 2×2, 3×3, 4×4 floats.
- **Non-square**: `mat2x3` (2 columns, 3 rows), `mat3x4`, `mat4x2`, etc.

Matrices are stored **column-major** in memory by default — the first 4 floats of a `mat4` are the first column, not the first row. (Most external matrix libraries — GLM, Eigen — also store column-major, so they line up.)

For doubles: `dmat2`, `dmat3`, `dmat4`, etc.

### Samplers

Sampler types (`sampler2D`, `sampler3D`, `samplerCube`, etc.) — covered in Part 13.4.

### What GLSL doesn't have

GLSL is deliberately small. Things you'd find in C++ but not in GLSL:

- No `char`, `short`, `long`, `long long` — only the four scalar widths above.
- No pointers, no references, no `&` of any kind.
- No dynamic allocation (`new` / `malloc` / etc.). All variables are stack-style; the compiler maps them to GPU registers or local memory.
- No string type, no I/O, no exceptions.
- No templates, no operator overloading, no inheritance. Structs exist but are simple value types.

These restrictions exist because GLSL runs on GPU hardware that doesn't support pointers or heap memory in the conventional sense. Shaders are essentially "pure functions over their inputs" — no side effects beyond writing to outputs.

### Precision qualifiers

You may see `highp`, `mediump`, `lowp` keywords in shader code, especially mobile / WebGL examples:

```glsl
mediump float u_Brightness;
```

These hint at numeric precision and matter on mobile GPUs (where `lowp` floats save power). On desktop GPUs they're essentially ignored — everything runs at full precision regardless. Most desktop shaders omit them.

## 14.2 Vector construction

You build vectors with **constructor expressions** — `vecN(...)` with arguments that, in total, supply the right number of components.

### Explicit components

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);
vec4 b = vec4(0.5, 1.0, 0.0, 1.0);
```

### Splat from a scalar

A single scalar argument splats to all components:

```glsl
vec3 zero = vec3(0.0);          // (0, 0, 0)
vec4 gray = vec4(0.5);          // (0.5, 0.5, 0.5, 0.5)
```

### Extending a smaller vector

You can extend a vector by appending more components:

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);
vec4 b = vec4(a, 1.0);          // (1, 2, 3, 1) — extend vec3 with one float
vec4 c = vec4(0.0, vec3(1.0));  // (0, 1, 1, 1) — prepend a float to a vec3
```

The arguments must total exactly the target's component count. `vec4(vec3(1.0))` is an error — only 3 components for a 4-component target.

### Truncating

You can't construct a smaller vector from a larger one directly — `vec2(someVec4)` is illegal. Use a **swizzle** (next section) to take just the components you want:

```glsl
vec4 a = vec4(1.0, 2.0, 3.0, 4.0);
vec2 b = a.xy;                   // (1, 2)
```

### Type conversion

Constructors also convert between types component-wise:

```glsl
ivec2 ipos = ivec2(2, 3);
vec2  fpos = vec2(ipos);         // int → float, (2.0, 3.0)
ivec3 floored = ivec3(vec3(1.7, 2.3, -0.5));   // truncates toward zero: (1, 2, 0)
```

### Component-wise arithmetic

Operators on vectors work **component-wise**:

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);
vec3 b = vec3(10.0, 20.0, 30.0);

vec3 sum    = a + b;       // (11, 22, 33)
vec3 prod   = a * b;       // (10, 40, 90)  — element-wise multiply, not dot product
vec3 scaled = a * 2.0;     // (2, 4, 6)
```

The `*` between two vectors is **not** a dot product. If you want a dot product, use `dot(a, b)` (covered in 14.4).

For matrix × vector — `mat4 m * vec4 v` — the operator is real matrix multiplication, not component-wise. The math library knows the difference based on operand types.

## 14.3 Swizzles

A **swizzle** is shorthand for picking and reordering components of a vector to make a new one. It looks like a member access:

```glsl
vec4 a = vec4(1.0, 2.0, 3.0, 4.0);

vec2 b = a.xy;       // (1, 2)
vec2 c = a.yx;       // (2, 1)  — components swapped
vec3 d = a.xxx;      // (1, 1, 1)  — splat-from-x
vec3 e = a.zyx;      // (3, 2, 1)  — reversed
vec4 f = a.wzyx;     // (4, 3, 2, 1)  — fully reversed
```

We've used swizzles informally in earlier parts (`v_TexCoord.yx`); this is the formal coverage.

### Three component-name sets

GLSL provides three interchangeable sets of component names:

| Set | Names | Used for |
|---|---|---|
| Positional | `.x`, `.y`, `.z`, `.w` | spatial vectors |
| Color | `.r`, `.g`, `.b`, `.a` | colors |
| Texture coord | `.s`, `.t`, `.p`, `.q` | texture coordinates |

They're identical at the language level — `a.x` and `a.r` and `a.s` mean the same thing. Pick whichever set reads best for the role of the vector. Importantly, the sets are language-equivalent, not type-tagged: GLSL doesn't know that `position` is "spatial" and `color` is "color." Nothing stops you from writing `position.rgb` — it's legal but confusing to read. The convention is purely for human signal:

```glsl
vec4 position = vec4(1.0, 2.0, 3.0, 4.0);
vec3 a = position.xyz;   // canonical for a position
vec3 b = position.rgb;   // legal but misleading — implies it's a color
```

The one hard rule: you can't **mix sets within a single swizzle**. `a.xg` is illegal — pick one naming set per swizzle expression.

### Length and rules

A swizzle can be 1 to 4 components long. The result type matches:

```glsl
float one    = a.x;       // 1 component → float
vec2  two    = a.zy;      // 2 components → vec2
vec3  three  = a.zyx;     // 3 components → vec3
vec4  four   = a.wzyx;    // 4 components → vec4
```

You can repeat components on the right side (read side):

```glsl
vec3 splat = a.xxx;       // legal — read x three times
```

### Swizzles on the left side

Swizzles can also appear on the **left** of an assignment, to write specific components:

```glsl
vec4 c;
c.rgb  = vec3(1.0);       // sets r, g, b — leaves a alone
c.a    = 0.5;             // sets a
c.xyzw = c.wzyx;          // reverses c in place
```

But components **cannot repeat** on the left side — there's no rule for which value would win:

```glsl
c.xx = vec2(1.0, 2.0);    // ERROR: which one wins, the first or second?
```

### Cost

Swizzles are essentially free at runtime. They compile to register reorderings — no actual computation, no memory access. Use them freely.

## 14.4 Built-in math functions

GLSL has a large set of built-in functions. Most are scalar/vector math; some are vector-specific (geometric); a few are texture-specific (covered in Part 13).

### Common scalar / vector math

| Function | What it does |
|---|---|
| `abs(x)` | absolute value |
| `sign(x)` | -1, 0, or 1 depending on sign |
| `floor(x)`, `ceil(x)`, `round(x)`, `trunc(x)` | rounding variants |
| `fract(x)` | fractional part: `x - floor(x)` |
| `mod(x, y)` | floating-point modulo |
| `min(a, b)`, `max(a, b)` | min / max |
| `clamp(x, lo, hi)` | force `x` into `[lo, hi]` |
| `mix(a, b, t)` | linear blend: `a*(1-t) + b*t` |
| `step(edge, x)` | 0 if `x < edge`, 1 otherwise |
| `smoothstep(e0, e1, x)` | smooth Hermite-cubic step from 0 to 1 between `e0` and `e1` |

### Power and exponential

| Function | What it does |
|---|---|
| `pow(x, y)` | `x` to the power `y` |
| `exp(x)`, `exp2(x)` | `e^x`, `2^x` |
| `log(x)`, `log2(x)` | natural log, log base 2 |
| `sqrt(x)`, `inversesqrt(x)` | √x and 1/√x (the latter is faster than `1.0 / sqrt(x)` and common in normalization) |

### Trigonometry

| Function | What it does |
|---|---|
| `sin(x)`, `cos(x)`, `tan(x)` | trig (radians) |
| `asin(x)`, `acos(x)`, `atan(x)` | inverse trig |
| `atan(y, x)` | full-range arctangent (like C's `atan2`) |
| `radians(deg)`, `degrees(rad)` | unit conversion |

### Geometric (vector-specific)

| Function | What it does |
|---|---|
| `length(v)` | the magnitude of `v` (a scalar) |
| `distance(a, b)` | `length(a - b)` |
| `normalize(v)` | `v` divided by its length — unit vector |
| `dot(a, b)` | dot product (a scalar) |
| `cross(a, b)` | cross product (vec3 × vec3 → vec3 only) |
| `reflect(I, N)` | reflect incident vector `I` around normal `N` |
| `refract(I, N, eta)` | refract `I` through normal `N` with refractive ratio `eta` |

These take vectors as input. `length`, `distance`, `dot` return scalars; the rest return vectors.

### Vectorization

Most scalar-input functions also work component-wise on vectors:

```glsl
vec3 a = vec3(-0.5, 0.7, 1.5);
vec3 b = abs(a);                     // (0.5, 0.7, 1.5)
vec3 c = clamp(a, 0.0, 1.0);         // (0.0, 0.7, 1.0)
vec3 d = mix(vec3(0.0), vec3(1.0), 0.5);  // (0.5, 0.5, 0.5)
```

The function applies independently to each component, returning a vector of the same shape. This is true of `abs`, `sign`, `floor`, `clamp`, `mix`, `step`, `smoothstep`, all of trigonometry, and most others.

The vector-specific ones (`length`, `dot`, `cross`, `normalize`) take whole vectors and don't apply component-wise.

### Texture sampling functions

`texture(sampler, uv)` and friends — covered in Part 13.5.

### Common patterns

A few combinations show up over and over in shaders:

```glsl
// Normalize a value into [0, 1] with hard clamping at the edges
float t = clamp((value - lo) / (hi - lo), 0.0, 1.0);

// Smooth normalization — soft edges
float t = smoothstep(lo, hi, value);

// Blend two colors
vec3 result = mix(colorA, colorB, t);

// Unit direction from one point to another
vec3 dir = normalize(target - origin);

// "How aligned are these two directions?" — between -1 and 1
float align = dot(normalize(a), normalize(b));
```

You'll see these (or close variants) in almost every fragment shader you write.

---

## Part 14 summary

- GLSL has **scalars** (`bool`, `int`, `uint`, `float`, `double`), **vectors** (`vec2`–`vec4` plus integer / bool / double variants), **matrices** (`mat2`–`mat4`, plus non-square forms), and **samplers** (Part 13).
- The language is intentionally small: no pointers, no dynamic allocation, no strings, no templates. Shaders are pure value-style functions over their inputs.
- Vectors are built with **constructor expressions** — explicit components, splat-from-scalar, or extending a smaller vector. Operators on vectors are **component-wise**; matrix × vector is real matrix multiplication.
- **Swizzles** pick and reorder components with `.xyzw`, `.rgba`, or `.stpq` notation. Free at runtime — they compile to register reorderings. Repetition allowed on the right (read) side; not on the left (write) side.
- The **built-in math library** is rich. Most scalar functions also apply component-wise to vectors. Key picks: `clamp`, `mix`, `smoothstep`, `length`, `normalize`, `dot`, the trig family, `pow`, `sqrt`. The vector-specific functions (`length`, `dot`, `normalize`, `cross`) operate on whole vectors and return scalars or single vectors, not component-wise outputs.

---

# Part 15: Practical

The earlier Parts each focused on one slice of OpenGL — buffers, attributes, shaders, textures, and so on. This Part puts the slices back together by walking through the real project end to end, and then collects the practical concerns that show up when you actually write OpenGL code: common pitfalls, how to debug a shader, and how to modify the renderer without breaking things.

## 15.1 Walkthrough: setting up the quad and its texture

The renderer's first job at startup is to build everything the field-display draw call will need: a quad of geometry, an empty texture, and a recipe (VAO) tying them together.

### The quad's geometry

Four corner vertices, each carrying a 2D position and a 2D texture coordinate, packed back-to-back in one C array:

```cpp
float vertices[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,  // bottom-left   — pos (-1,-1), tex (0,0)
     1.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
     1.0f,  1.0f, 1.0f, 1.0f,  // top-right
    -1.0f,  1.0f, 0.0f, 1.0f,  // top-left
};
unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
```

This is the **interleaved layout** from Part 6.5 — one vertex chunk = `4 floats = 16 bytes` (position + texcoord). Total 4 vertices = 64 bytes. The 6 indices form 2 triangles covering the rectangle (Part 6.8).

The position values `(±1, ±1)` are clip-space coordinates with the convention that `w = 1` (Part 5.3); in our setup the vertex shader writes them straight to `gl_Position` without any matrix math.

### Creating the buffer objects and the VAO

Three handles — one VAO, one VBO, one IBO (Parts 6.3, 6.7, 7.7):

```cpp
GLuint vao, vbo, ibo;
glGenVertexArrays(1, &vao);
glGenBuffers(1, &vbo);
glGenBuffers(1, &ibo);
```

After these calls: three handles exist, no GPU memory yet (Part 6.3).

Now bind the VAO to start recording attribute configuration into it (Part 7.7):

```cpp
glBindVertexArray(vao);
```

Bind the VBO and upload the vertex data:

```cpp
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
```

`GL_STATIC_DRAW` because the quad's geometry never changes after this (Part 6.4).

Bind the IBO. Because a VAO is currently bound, this binding is captured *into* the VAO (Part 7.7's special IBO behavior):

```cpp
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
```

Now configure the two attributes (Part 7.3):

```cpp
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
glEnableVertexAttribArray(1);
```

Slot 0: 2 floats, stride 16 bytes, offset 0 — the position. Slot 1: 2 floats, stride 16 bytes, offset 8 — the texcoord. Each `glVertexAttribPointer` captures the currently-bound VBO into the VAO for that attribute (Part 7.7's snapshot moment). Each `glEnableVertexAttribArray` turns the slot on (Part 7.6).

Finally, unbind:

```cpp
glBindVertexArray(0);
```

After this the VAO holds a complete recipe. To draw later, just bind the VAO and call `glDrawElements` — no need to re-bind buffers or re-set attributes.

### Creating the field texture

The field is a 2D grid of 32-bit floats. The internal format `GL_R32F` matches that exactly (Part 13.1):

```cpp
GLuint fieldTex;
glGenTextures(1, &fieldTex);
glBindTexture(GL_TEXTURE_2D, fieldTex);

glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
```

Generate the handle, bind to `GL_TEXTURE_2D`, configure filtering (Part 13.6) and wrapping (Part 13.7), then allocate storage with the chosen internal format. The final `nullptr` means "allocate but don't upload anything" — the texture starts empty; the simulation will fill it later.

After this block runs once at startup, the texture is allocated and ready to receive frame data via `glTexSubImage2D`.

## 15.2 Walkthrough: the field shaders

Both shader files are tiny — but they exercise most of what Parts 8, 9, 10, 13, and 14 covered.

### `field.vert`

```glsl
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 v_TexCoord;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    v_TexCoord  = aTexCoord;
}
```

- **`#version 460 core`** — declare the GLSL version and profile (Part 3.1).
- **`layout (location = 0) in vec2 aPos`** — attribute slot 0 is a `vec2`. The number `0` matches the C++ side's `glVertexAttribPointer(0, ...)` call from 15.1. `vec2` matches the configured `size = 2, type = GL_FLOAT`. The `a` prefix is the project's convention for attributes (Part 8.6).
- **`layout (location = 1) in vec2 aTexCoord`** — slot 1, same setup, the texture coordinate.
- **`out vec2 v_TexCoord`** — a user-declared output (Part 8.3). It will be interpolated across the triangles by the rasterizer (Part 9.5) and read by the fragment shader. The `v_` prefix marks it as a varying (Part 8.6).
- **`gl_Position = vec4(aPos, 0.0, 1.0);`** — extend the 2D position into a clip-space `vec4` (Part 8.4 for `gl_Position`, Part 14.2 for `vec4(vec2, ...)` construction). Padding `z = 0` puts the vertex on the near-far plane; `w = 1` means the upcoming perspective divide (Part 9.3) is a no-op.
- **`v_TexCoord = aTexCoord;`** — pass-through. Each vertex's texcoord is forwarded; rasterization will compute one interpolated value per fragment (Part 9.5).

### `field.frag`

```glsl
#version 460 core
in  vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Field;
uniform float u_RangeMin;
uniform float u_RangeMax;

void main()
{
    float raw   = texture(u_Field, v_TexCoord.yx).r;
    float range = u_RangeMax - u_RangeMin;
    float t     = range > 1e-6 ? (raw - u_RangeMin) / range : 0.0;
    FragColor   = vec4(turbo(t), 1.0);
}
```

(The shader also defines a `turbo(float t)` helper at the top — a user function that takes a scalar in `[0, 1]` and returns a `vec3` color via polynomial math. Its body is omitted here; what matters for the walkthrough is the call in `main()`.)

- **`in vec2 v_TexCoord`** — the interpolated tex coord arriving from the vertex shader (Part 10.2). Matches by name and type.
- **`out vec4 FragColor`** — the user-declared color output (Part 10.4). `vec4` is RGBA in `[0, 1]`.
- **Three uniforms** (Part 12.1): one sampler tied to a texture unit (Part 13.4) and two `float`s. The C++ side will set all three before each draw.
- **`turbo(...)`** — a user function (declared above; body elided). Takes a `float t`; returns a `vec3` color. Uses GLSL math features from Part 14.
- **`texture(u_Field, v_TexCoord.yx).r`** — sample the field texture (Part 13.5). The `.yx` swizzle (Part 14.3) flips the texture coordinates; this fixes a coordinate-system orientation. The `.r` extracts the red channel — `GL_R32F` textures only have meaningful data in `.r`.
- **The ternary on `range`** — guards against a zero-range field (would otherwise divide by zero).
- **`FragColor = vec4(turbo(t), 1.0);`** — extend the `vec3` color with `alpha = 1` and write it.

Each fragment shader invocation runs the full polynomial in `turbo()` once, on its own interpolated `t` — and produces one pixel of the rendered field.

## 15.3 Walkthrough: drawing one frame

Here's what the renderer actually does, per frame, for the field-display draw. The earlier Parts now make every line obvious.

```cpp
// 1. Update the texture with this frame's simulation values
glBindTexture(GL_TEXTURE_2D, fieldTex);
glTexSubImage2D(GL_TEXTURE_2D, 0,  0, 0,  width, height,  GL_RED, GL_FLOAT, fieldPtr);

// 2. Activate the field's shader program
glUseProgram(fieldShader);

// 3. Bind the texture to unit 0 and tell the sampler uniform
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, fieldTex);
glUniform1i(samplerLoc, 0);

// 4. Set the colormap range uniforms for this frame
glUniform1f(rangeMinLoc, currentMin);
glUniform1f(rangeMaxLoc, currentMax);

// 5. Bind the VAO containing the quad's recipe
glBindVertexArray(quadVao);

// 6. Draw
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
```

Each step in concept terms:

| Step | What it does | Concepts |
|---|---|---|
| 1 | Replace the texture's contents with this frame's data | Part 13.1, texture upload |
| 2 | Set the active shader program | Part 4.4, two-phase pattern |
| 3 | Bind texture to unit 0; tell the sampler to read from there | Part 13.3, 13.4 |
| 4 | Push per-frame uniforms | Part 12.3 |
| 5 | Activate the geometry recipe | Part 7.7 |
| 6 | Trigger the GPU pipeline | Part 5.4, 9.x |

What happens on the GPU after step 6 (the draw):

1. The GPU reads 6 indices from the IBO (recorded in the VAO).
2. For each index (some indices repeat — e.g. our `0, 1, 2, 2, 3, 0`), it fetches the corresponding vertex's position and texcoord from the VBO.
3. The vertex shader runs 4 times — once per unique vertex — writing `gl_Position` and `v_TexCoord` outputs.
4. Primitive assembly groups the 6 indices into 2 triangles (Part 9.1).
5. Clipping (Part 9.2) — both triangles are inside the clip cube; nothing trimmed.
6. Perspective divide (Part 9.3) — `w = 1`, so xyz unchanged.
7. Viewport transform (Part 9.3) — convert NDC into actual pixel positions.
8. Rasterization (Part 9.4) — generate one fragment per covered pixel, with `v_TexCoord` interpolated for each (Part 9.5).
9. The fragment shader runs once per fragment: samples the field, applies the colormap, writes a color.
10. The color goes to the framebuffer (Part 11.1) — onto the screen.

That's the whole render path for one field frame.

## 15.4 Common pitfalls

A short list of things that bite OpenGL programmers regularly. Most have been called out in their respective Parts; this is a single place to scan when something's not working.

### State-related

- **Forgetting to bind the program before setting uniforms.** `glUniform*` writes to whichever program is currently active. If no program is bound (or the wrong one), your uniform set is wasted. Always `glUseProgram(...)` first.
- **Mixing `glUniform*` and `glProgramUniform*` styles.** `glUniform*` operates on the active program; `glProgramUniform*` takes the program handle as an argument and doesn't need binding. Both work, but if you alternate between them carelessly (e.g. update one program's uniforms with `glUniform*` while a different one is bound), the values land on the wrong program. Pick one style and stick to it.
- **Forgetting to bind the VAO before configuring attributes.** `glVertexAttribPointer` records into the currently-bound VAO. If no VAO is bound, the configuration goes nowhere useful — you'll get nothing rendered.
- **Re-binding a VBO after `glVertexAttribPointer` and expecting it to take effect.** The VAO captured the VBO at the moment of the call (Part 7.7). Changing `GL_ARRAY_BUFFER` later does not update the VAO's recorded source.

### Configuration-related

- **Mismatched stride or offset on interleaved data.** A common symptom: garbled positions, scrambled colors. Recompute carefully — `stride = bytes per vertex`, `offset = bytes before this attribute within each vertex chunk`.
- **Not enabling an attribute slot.** `glVertexAttribPointer` configures, but `glEnableVertexAttribArray` is what turns the slot on. Forget the enable and the slot reads default zeros — usually visible as collapsed or scrambled geometry (every vertex's position is `(0, 0)`, every texcoord is `(0, 0)`, etc.).
- **`glGetUniformLocation` returns `-1` and you didn't notice.** Setting a uniform at `-1` is a silent no-op (Part 12.2). Either you typo'd the name, or the GLSL compiler optimized the uniform away because it isn't used in the shader's `main()`.

### Texture-related

- **Sampler reads from the wrong unit.** A sampler holds a unit number (Part 13.4), set with `glUniform1i`. If you forget to set it, it defaults to `0`. If two samplers default to `0` they'll alias on the same texture.
- **Sampler type doesn't match the texture target.** A `sampler2D` reading a unit with no 2D texture bound (or a `sampler3D` reading a unit with a 2D texture) is undefined behavior.

### Compile / link

- **Shader compile or program link silently failed.** The draw runs but nothing shows. Check `GL_COMPILE_STATUS` after each `glCompileShader` and `GL_LINK_STATUS` after `glLinkProgram` (Part 3.5).

## 15.5 Debugging shaders

Because shaders run on the GPU, in massive parallel, with no `printf`, debugging them needs different techniques than CPU code.

### Visualize values by writing them to the output

The most useful trick: write the value you want to inspect into `FragColor`. Run, look at the screen, infer what's going on.

A few common patterns:

```glsl
// Visualize tex coords as colors:
FragColor = vec4(v_TexCoord, 0.0, 1.0);
// You should see a smooth gradient: red along one axis, green along the other.

// Check if a value is positive vs. negative:
FragColor = (someValue > 0.0) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
// Green or red, no in-between.

// Visualize a scalar in [0,1] as grayscale:
FragColor = vec4(vec3(value), 1.0);
```

If the gradient looks right, your inputs are reaching the shader correctly. If you see solid colors or garbage, something upstream is wrong.

### Check compile and link errors

Always read these logs when a shader silently doesn't render:

```cpp
GLint ok;
glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
if (!ok) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    fprintf(stderr, "Shader compile error:\n%s\n", log);
}

glGetProgramiv(program, GL_LINK_STATUS, &ok);
if (!ok) {
    char log[1024];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    fprintf(stderr, "Program link error:\n%s\n", log);
}
```

GLSL error messages are usually clear about line numbers and the problem.

### Check for OpenGL errors

Drop `glGetError()` after suspicious blocks of code:

```cpp
GLenum err = glGetError();
if (err != GL_NO_ERROR) {
    fprintf(stderr, "GL error: 0x%x\n", err);
}
```

Common error codes: `GL_INVALID_ENUM`, `GL_INVALID_VALUE`, `GL_INVALID_OPERATION` (often "you didn't bind something correctly"), `GL_OUT_OF_MEMORY`.

### Use a graphics debugger

For deeper investigation, dedicated graphics debuggers like RenderDoc let you capture a frame, inspect every draw call, see exactly which vertices were submitted, what uniforms were set, and what each fragment produced. This is the heavy artillery — worth learning when fragment-color tricks aren't enough.

## 15.6 How to modify safely

Some habits that keep the renderer from breaking when you change it.

- **Change one thing at a time.** Modify one shader, or one buffer setup, or one uniform — then run. If the screen breaks, you know what caused it. Multiple changes at once make bisection painful.
- **Always check status after compile and link**, even when you "know" the change is small. Typos are silent in OpenGL.
- **When adding a uniform, use it in `main()` immediately.** Otherwise the compiler optimizes it away and `glGetUniformLocation` returns `-1` (Part 12.2).
- **When adding an attribute, configure both sides.** Add the `layout (location = N) in` declaration in the shader *and* the `glVertexAttribPointer(N, ...)` plus `glEnableVertexAttribArray(N)` in C++. Forgetting either side means the slot has zero or undefined data.
- **Keep your shader's expectations and your C++ setup in sync.** If you change a `vec2 in` to a `vec3 in`, the C++ side's `size=2` becomes wrong. If you change attribute layout, the stride and offsets change.
- **Use version control.** Shader bugs can be subtle and hard to revert manually. A clean git history of "before / after" is invaluable when a change introduces a bug you didn't expect.

---

## Part 15 summary

- Walking through the project end to end shows the concepts from Parts 5–14 working together: VBO + IBO + VAO recipe (Part 6, 7), texture upload and sampler (Part 13), vertex and fragment shaders (Part 8, 10), uniforms (Part 12), and the per-frame draw call (Part 5.4, 9).
- **Pitfalls** cluster around state (program not bound, VAO not bound), configuration mismatches (stride/offset, enable forgotten), and silent failures (`-1` uniform locations, compile errors not checked).
- **Debugging** shaders is mostly visual — write values to `FragColor` and look at the screen. Always check compile/link status and `glGetError()` when something silently breaks.
- **Modify safely** by changing one thing at a time, keeping both sides (shader + C++) in sync, and using version control to bisect when something breaks.
