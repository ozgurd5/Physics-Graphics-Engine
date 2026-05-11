# Graphics Engine — Function-Level Walkthrough

A deep look at the two C++ source files, layer by layer: what the C++ does, what OpenGL does, what the GPU does, what crosses the C/C++ boundary into the physics engine, and the subtleties that aren't obvious from the code.

Two parts:
- **Part 1 — `graphics_engine.{hpp,cpp}`**: the renderer. Six functions: `compile_shader`, `create_quad`, `create_arrow_buffer`, `update_field`, `create_field_texture`, `update_arrows`.
- **Part 2 — `main.cpp`**: the application driver. The `extern "C"` interop with the C physics engine, file-scope defaults, the `runtime_controls` struct, the helper functions (`pick_solver`, `pick_precond`, `scan_min_max`, `compute_velocity_magnitude`, `reload_scenario`), the ImGui control panel (`draw_control_panel`), and the `main()` frame loop.

---

# Part 1 — `graphics_engine.{hpp,cpp}`

## `compile_shader`

```cpp
unsigned int graphics_engine::compile_shader(const char* vert_path, const char* frag_path);
```

**Job:** read two GLSL source files from disk, compile them into a vertex shader and a fragment shader, link them into a single shader program, and return the program's GL handle.

### CPU side

1. `read_file(vert_path)` opens the file, slurps its bytes into a `std::string`, and returns it. The string sits in heap memory (allocated by `std::string`'s internal buffer).
2. `vert_source.c_str()` returns a `const char*` pointer into the string's buffer. The pointer is only valid while `vert_source` is alive.
3. The pointer is taken by `&` and passed to OpenGL — `glShaderSource` wants a `const char* const*` (a pointer to an array of strings), so we give it a pointer to our single pointer.

No GPU activity yet. We're handing text to the driver.

### OpenGL driver side

The OpenGL driver is part of the graphics driver — a vendor-supplied user-mode library (NVIDIA, AMD, Intel) that translates GL calls into GPU commands.

1. `glCreateShader(GL_VERTEX_SHADER)` asks the driver to allocate a "shader object" — a small bookkeeping record in driver memory. Returns an integer handle (just a name; not a pointer).
2. `glShaderSource(vert, 1, &vert_chars, nullptr)` copies the GLSL source text into the driver's internal storage for that shader object.
   - The `1` is the count of source strings. You can pass more than one string and the driver concatenates them — useful for prepending `#define`s.
   - The `nullptr` for lengths means "use `strlen` on each string" — fine for null-terminated text.
3. `glCompileShader(vert)` invokes the **GLSL compiler that lives inside the driver**. The compiler:
   - Parses GLSL syntax.
   - Type-checks (`vec2`, `vec4`, `sampler2D`, etc.).
   - Generates intermediate representation, then GPU machine code. The machine code is vendor-specific: NVIDIA SASS, AMD GCN/RDNA, Intel ISA. Two different cards can produce different binaries from the same GLSL.
   - Stores the compiled binary in the shader object.

Same flow for the fragment shader.

4. `glCreateProgram()` allocates a **program object** — a container that will eventually hold one compiled shader per pipeline stage.
5. `glAttachShader(program, vert)` and `glAttachShader(program, frag)` register the two compiled shaders with the program.
6. `glLinkProgram(program)` is the second compilation step:
   - Checks that the vertex shader's outputs (`out vec2 v_tex_coord;`) match the fragment shader's inputs (`in vec2 v_tex_coord;`) by name and type. A mismatch is a link error.
   - Assigns slots for varyings — the GPU hardware needs to know which output position carries which value across the rasterizer.
   - Resolves uniform locations.
   - Produces the final executable: a compiled, linked program ready for the GPU.
7. `glDeleteShader(vert)` and `glDeleteShader(frag)` flag the individual shader objects for deletion. The driver doesn't free them immediately — they're released once the program no longer needs them. The program has its own copy of the linked binary.

The function returns the program handle. From here on, `glUseProgram(program)` will activate this program for subsequent draw calls.

### GPU side

The compiled program lives in GPU-accessible memory (either VRAM on discrete cards or system RAM on integrated graphics). When a draw call references it:

- The GPU's command processor switches its shader stages to point at this program's vertex and fragment binaries.
- Each stage runs on the GPU's **shader cores** — physical execution units (thousands of them on modern cards: NVIDIA "CUDA cores," AMD "stream processors," Intel "execution units"). The same shader code runs on many cores simultaneously, each handling a different vertex or fragment.

### Subtleties

- **The compiler is in the driver.** Every install of the graphics driver ships with its own GLSL compiler. You don't pre-compile GLSL like C++ — it gets compiled at program startup, every time. (SPIR-V, Vulkan's binary intermediate, is the modern workaround.)
- **Errors are silent unless you ask.** Neither `glCompileShader` nor `glLinkProgram` prints anything on failure. To know if compilation succeeded, you have to call `glGetShaderiv(shader, GL_COMPILE_STATUS, &ok)` and `glGetShaderInfoLog(...)`. Our code skips that check — fine for stable shaders we trust; not OK in production.
- **The `1` in `glShaderSource(vert, 1, &vert_chars, nullptr)` looks redundant** because we always pass one string. The API supports multiple to let you assemble shader source from pieces — for example, prepending `#version` and `#define` lines from C++ without modifying the on-disk file.
- **`glDeleteShader` after attach + link is the standard cleanup pattern.** The shader object becomes orphaned but its compiled bytes stay alive inside the program. When the program is later deleted, the orphaned shaders go with it.

---

## `create_quad`

```cpp
void graphics_engine::create_quad();
```

**Job:** allocate GPU memory for a single full-screen rectangle (two triangles), upload its vertices and index list, and record an attribute layout that says "vertex = 2 floats for position, 2 floats for texture coordinates."

### The geometry

A quad has 4 corners. Each corner carries two pieces of data:

```
vertex 0:  (-1, -1)  /  (0, 0)   ← bottom-left,   tex coord top-left
vertex 1:  ( 1, -1)  /  (1, 0)   ← bottom-right,  tex coord top-right
vertex 2:  ( 1,  1)  /  (1, 1)   ← top-right,     tex coord bottom-right
vertex 3:  (-1,  1)  /  (0, 1)   ← top-left,      tex coord bottom-left
```

The positions are in **Normalized Device Coordinates (NDC)** — the space the rasterizer expects, where [-1, +1] in both axes covers the entire framebuffer. We don't apply a view or projection matrix; the quad is already in screen space.

GPUs don't draw quads directly. We split into 2 triangles, and instead of repeating the shared vertices, we index them:

```
indices = { 0, 1, 2,   2, 3, 0 }
          └─triangle─┘ └─triangle─┘
          first half   second half
```

6 indices, 4 unique vertices. A degenerate-quad would store 6 full vertices (96 bytes for our format); indexed costs 4 vertices + 6 indices = 64 + 24 = 88 bytes — a saving even for this tiny case, much bigger savings for complex meshes.

### CPU side

`vertices` is a `constexpr float[16]` (4 vertices × 4 floats each). `indices` is a `constexpr unsigned int[6]`. Both live on the stack — they're temporary; once uploaded to the GPU we never need them on the CPU again.

### The three GPU objects: VBO, IBO, VAO

These three abbreviations come up constantly in OpenGL. They are easy to confuse because their names sound similar but their roles are completely different.

**VBO — Vertex Buffer Object.** A raw region of GPU memory holding the actual **vertex data**: positions, texture coordinates, normals, colors, whatever the shader needs per vertex. The bytes have no inherent meaning — they're just floats (or other numeric types) packed end-to-end. A VBO is identified by an `unsigned int` **handle** (a name, not a pointer).

```
VBO for our quad (64 bytes total):
┌─────────────────────────────────────────────────────────┐
│  pos_x  pos_y  tex_u  tex_v   ← vertex 0 (16 bytes)     │
│   -1     -1     0      0                                │
├─────────────────────────────────────────────────────────┤
│   pos_x  pos_y  tex_u  tex_v   ← vertex 1 (16 bytes)    │
│    1     -1     1      0                                │
├─────────────────────────────────────────────────────────┤
│   pos_x  pos_y  tex_u  tex_v   ← vertex 2 (16 bytes)    │
│    1      1     1      1                                │
├─────────────────────────────────────────────────────────┤
│   pos_x  pos_y  tex_u  tex_v   ← vertex 3 (16 bytes)    │
│   -1      1     0      1                                │
└─────────────────────────────────────────────────────────┘
```

**IBO — Index Buffer Object.** Also called EBO (Element Buffer Object) — same thing, two names. A region of GPU memory holding **indices into the VBO**. Each index is just an integer: "use vertex N from the VBO." The IBO is what lets us share vertices between triangles instead of duplicating them.

```
IBO for our quad (24 bytes total — 6 uint32 indices):
┌────┬────┬────┬────┬────┬────┐
│ 0  │ 1  │ 2  │ 2  │ 3  │ 0  │
└────┴────┴────┴────┴────┴────┘
 └─ triangle 1 ─┘└─ triangle 2 ─┘
```

The first three indices (0, 1, 2) form one triangle; the second three (2, 3, 0) form the other. Vertices 0 and 2 are shared — without the IBO we'd have to repeat their 16 bytes each, paying 96 bytes of vertex data instead of 64.

VBO and IBO are structurally identical — both are just byte buffers in GPU memory. The difference is what they're used for and which **binding target** they get bound to:
- VBO → `GL_ARRAY_BUFFER` (vertex attribute data)
- IBO → `GL_ELEMENT_ARRAY_BUFFER` (indices)

The target tells OpenGL how to interpret the bytes when a draw call runs.

**VAO — Vertex Array Object.** This one is different. A VAO is **not** GPU data — it's a **bookkeeping record** that ties together:

1. Which VBO each vertex attribute pulls from.
2. The layout of each attribute (how many floats, where they sit in the VBO, the stride between vertices).
3. Which attribute slots are enabled.
4. The IBO binding (this state lives in the VAO — uniquely, since most other state is global).

In other words, a VAO is the answer to the question "given a draw call, where does the GPU find the data and how does it parse it?" Without a bound VAO, the GPU has no idea where vertices come from.

A VAO is set up once during initialization and bound at draw time. Binding the VAO "snaps" all the recorded settings back into the OpenGL state machine in a single call. This is why our draw code is just:

```cpp
glBindVertexArray(quad_vao);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
glBindVertexArray(0);
```

We don't need to re-bind buffers, re-specify attribute formats, or re-enable slots — the VAO did all of that during setup and remembers it.

A typical flow:
- **Once per object at startup**: create VAO, create VBO, create IBO, bind VAO, bind VBO and upload, bind IBO and upload, set up attribute pointers, unbind VAO.
- **Every frame at draw time**: bind VAO, issue draw call, unbind VAO.

### OpenGL driver side

1. `glGenVertexArrays(1, &quad_vao)` reserves a **Vertex Array Object (VAO)** handle. The VAO is empty at this point — it just exists as a name.

2. `glGenBuffers(1, &quad_vbo)` and `glGenBuffers(1, &quad_ibo)` reserve handles for the **VBO** and **IBO**. Both start as zero-byte buffers — no GPU memory allocated yet.

3. `glBindVertexArray(quad_vao)` makes our VAO the **currently bound** one. OpenGL is a state machine: many calls don't take their target as a parameter; they operate on whatever's currently bound. Until we unbind, subsequent attribute-related calls are recorded into this VAO.

4. `glBindBuffer(GL_ARRAY_BUFFER, quad_vbo)` binds the VBO to the `GL_ARRAY_BUFFER` target (the slot used for vertex data).

5. `glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW)`:
   - `sizeof(vertices)` is 64 bytes (16 floats × 4 bytes each).
   - The driver copies those 64 bytes from CPU memory into a freshly allocated GPU memory region owned by the VBO.
   - `GL_STATIC_DRAW` is a usage hint: "I'm going to write this data once and the GPU will read it many times." The driver may use this to place the buffer in fast read-only memory — VRAM on a discrete GPU.

6. `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ibo)` + `glBufferData(...)` does the same for the index list (24 bytes — 6 uint32s).

7. `glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0))` configures **attribute slot 0** — the slot the vertex shader reads as `layout(location = 0) in vec2 a_position;`. Parameters:
   - **0** — slot index.
   - **2** — components per attribute (vec2).
   - **GL_FLOAT** — each component is a 32-bit float.
   - **GL_FALSE** — don't normalize (only meaningful when source data is integer; here it's already float).
   - **`4 * sizeof(float)` = 16** — **stride**: bytes between the start of one vertex and the start of the next. Our vertices are interleaved (position + tex_coord packed together), so one vertex = 16 bytes.
   - **`reinterpret_cast<void*>(0)`** — **offset**: bytes from the start of the buffer to the first attribute of this kind. Position starts at the beginning of each vertex, so offset 0.

8. `glEnableVertexAttribArray(0)` turns slot 0 on. Slots are off by default; if you forget to enable them the vertex shader sees zeros.

9. `glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)))` does the same for slot 1 (the tex_coord). Same stride (still 16 bytes per vertex), but offset 8 — texture coordinates start halfway through each vertex, after the position.

10. `glEnableVertexAttribArray(1)`.

11. `glBindVertexArray(0)` unbinds the VAO — we're done recording. The VAO now remembers everything: which VBO, which IBO, which slots, which formats, which offsets.

### GPU side

After these calls, GPU memory holds:
- 64 bytes of vertex data (4 vertices, interleaved).
- 24 bytes of index data (6 unsigned ints).
- A VAO record (in driver memory) describing how to fetch attributes from those buffers.

When `glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr)` is later called from `draw`:
1. The GPU's **vertex puller** reads 6 indices from the IBO.
2. For each index N, it uses the VAO recipe to fetch vertex N's 16 bytes from the VBO and split them into a 2-float position (slot 0) and a 2-float texture coordinate (slot 1).
3. The vertex shader runs once per vertex, in parallel across many shader cores.
4. The output of the vertex shader is fed to the rasterizer; the fragment shader runs once per covered pixel.

### Subtleties

- **The VAO records the IBO binding, not just attribute formats.** Unusually, `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...)` is stored in the bound VAO. This is why binding only the VAO at draw time is enough — the IBO comes along. The VBO binding for `GL_ARRAY_BUFFER`, by contrast, is global; the VAO records "use whatever was bound when this attribute was configured" indirectly through the pointer state.
- **Stride matters because of interleaving.** With an interleaved layout (position + tex_coord per vertex), stride is the full vertex size. If we used a separated layout (all positions in one buffer, all tex_coords in another), the stride for each would be smaller.
- **The `reinterpret_cast<void*>(N)` is OpenGL's legacy way of saying "byte offset N."** The parameter is typed `const void*` for backward compatibility with the old fixed-function pipeline where it actually was a pointer to client memory. Now it's a byte offset into the bound VBO. Modern GL bindings use a separate `glVertexAttribFormat` + `glVertexAttribBinding` pair to avoid this ugliness.

---

## `create_arrow_buffer`

```cpp
void graphics_engine::create_arrow_buffer();
```

**Job:** set up a **dynamic VBO** for line segments — same structure as the quad's VBO, but **empty**. Data will be poured in every frame by `update_arrows`.

### How it differs from `create_quad`

| | `create_quad` | `create_arrow_buffer` |
|---|---|---|
| Has an IBO? | Yes (6 indices) | No |
| Initial data upload? | Yes, 64 bytes | No, 0 bytes |
| Re-uploaded per frame? | Never | Yes, every frame |
| Usage hint when later filled | `GL_STATIC_DRAW` | `GL_DYNAMIC_DRAW` |
| Attribute layout | 2 (position + tex_coord) | 1 (position only) |

### OpenGL driver side

1. `glGenVertexArrays(1, &arrow_vao)` — VAO handle.
2. `glGenBuffers(1, &arrow_vbo)` — VBO handle, zero-byte region.
3. `glBindVertexArray(arrow_vao)`.
4. `glBindBuffer(GL_ARRAY_BUFFER, arrow_vbo)`.
5. `glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0))` — slot 0, vec2, stride 8 (just one vec2 per vertex; no tex_coord). The arrow vertex shader reads only `a_position`.
6. `glEnableVertexAttribArray(0)`.
7. `glBindVertexArray(0)`.

**No `glBufferData` call.** The VBO exists but has zero bytes allocated. The first `glBufferData` will happen later in `update_arrows`.

### Why no IBO?

For `GL_LINES`, the GPU reads vertices in **consecutive pairs**: vertices 0-1 form a line, 2-3 form another, 4-5 form another. There's no sharing of endpoints between segments — every arrow's shaft and two wings are independent line segments with their own pair of endpoints.

If we tried to share endpoints with an IBO, the savings would be tiny (the only shared vertex is the arrow's tip, which appears in 3 segments — shaft end, wing-a start, wing-b start). Not worth the index lookup overhead and the IBO bookkeeping.

### Subtleties

- **The buffer being zero bytes is not the same as "no buffer."** The buffer object exists; it just has no allocated storage yet. The first `glBufferData` from `update_arrows` will allocate.
- **The VAO's layout is established now, once, at startup.** Every frame, `update_arrows` only needs to upload data — the GPU already knows how to interpret it (one vec2 per vertex, stride 8, slot 0).

---

## `update_field`

```cpp
void graphics_engine::update_field(const float* data, int width, int height, float range_min, float range_max);
```

**Job:** upload a 2D array of scalar floats from CPU memory into a GPU texture, so the fragment shader can sample it when drawing the field quad. Also stash the value range for the colormap normalization.

### CPU side

- `data` points to a `width × height` array of floats — the simulation's smoke / pressure / velocity-magnitude grid.
- `range_min` / `range_max` are stashed in members. The fragment shader will read them as uniforms during `draw` and use them to normalize each raw float into a `[0, 1]` value for the Turbo colormap.
- If the texture doesn't exist yet (`field_texture == 0`) or the simulation resized, we call `create_field_texture(width, height)` first to allocate.

### OpenGL driver side

1. `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)`:
   - OpenGL's default unpack alignment is 4 bytes — it assumes each row of pixel data is aligned to a 4-byte boundary. For our `R32F` format (one 4-byte float per texel), the natural row size is already 4-aligned, so this call is defensive.
   - The "unpack" here means "when copying from CPU memory into GPU memory" (the direction is upload). The reverse is `GL_PACK_ALIGNMENT`, used for reads.

2. `glBindTexture(GL_TEXTURE_2D, field_texture)` puts our texture handle in the "current 2D texture" slot.

3. `glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, data)`:
   - **`GL_TEXTURE_2D`** — target.
   - **`0`** — mipmap level (only level 0 here; we don't generate mipmaps).
   - **`0, 0`** — x and y offset into the texture; we write starting at the lower-left corner.
   - **`width, height`** — how many texels to write.
   - **`GL_RED`** — source format: one channel per texel.
   - **`GL_FLOAT`** — source type: each channel is a 32-bit float.
   - **`data`** — pointer to the CPU-side data.
   - Effect: the driver copies `width × height × 4` bytes from the CPU pointer into the texture's GPU memory.

4. `glBindTexture(GL_TEXTURE_2D, 0)` unbinds — defensive cleanup.

### GPU side

The texture's storage is a chunk of GPU memory laid out for fast 2D access — internally the driver may use **tiled** or **swizzled** storage (Z-order curve, Morton order) so that nearby texels are nearby in memory regardless of which direction the sampler walks. This is opaque to us.

When the fragment shader does `texture(u_field, v_tex_coord.yx).r`:
1. The GPU's **texture unit** computes which texel(s) to read based on `v_tex_coord.yx` (a vec2 in [0, 1]).
2. With `GL_LINEAR` filtering, the sampler reads the four nearest texels and bilinearly interpolates between them.
3. Returns a `vec4` with the red channel populated; the shader reads `.r` to extract the float.

### Subtleties

- **`glTexSubImage2D` vs `glTexImage2D`** — the former writes into already-allocated storage; the latter reallocates. We use `glTexSubImage2D` here because `create_field_texture` already allocated the storage. `glTexImage2D` every frame would be slow — possibly freeing and reallocating GPU memory each call.
- **`GL_RED` as source format is confusing.** "Red" is just OpenGL's name for "the first/only channel." The texture stores single floats; the API insists on naming channels after RGBA slots. The actual stored format is whatever `create_field_texture` specified as internal format (`GL_R32F`); the source format describes the layout of the CPU-side data.
- **Range is uploaded as uniforms, not baked into the texture.** We could have normalized the data on the CPU before upload (divide by `range_max - range_min`), but keeping the raw values in the texture lets the colormap range change every frame without re-uploading the data. The fragment shader handles normalization.
- **The shader's `v_tex_coord.yx` swizzle** swaps S and T axes. The simulation indexes its arrays as `smoke[i * y + j]`, which makes the first row in memory correspond to the first column of cells (sim `i = 0`). OpenGL's default texture S axis is the "fast" one (along memory rows), which means it lines up with sim `j`. The `.yx` swap re-orients so that screen-X matches sim-i.

---

## `create_field_texture`

```cpp
void graphics_engine::create_field_texture(int width, int height);
```

**Job:** allocate GPU memory for the scalar-field texture and configure how it'll be sampled. Called once at startup and again any time the simulation grid resizes.

### CPU side

- If a texture already exists (`field_texture != 0`), delete it first via `glDeleteTextures(1, &field_texture)`. This frees the old GPU memory before we allocate new memory.
- Store the new `width` and `height` in `field_width` / `field_height` so future `update_field` calls can detect size changes.

### OpenGL driver side

1. `glGenTextures(1, &field_texture)` — reserve a texture handle.

2. `glBindTexture(GL_TEXTURE_2D, field_texture)` — bind it as the current 2D texture so subsequent calls configure this one.

3. **Filter modes** — what happens when sampling at a sub-texel coordinate:
   - `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)`: when minification (the texture is shown smaller than native), bilinearly interpolate between the 4 nearest texels.
   - `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)`: same when magnifying (texture shown larger).
   - `GL_LINEAR` is what makes the field look smooth rather than blocky. The alternative `GL_NEAREST` would produce sharp pixel boundaries — sometimes desirable for pixel art, not for a smooth fluid field.

4. **Wrap modes** — what happens when sampling outside [0, 1]:
   - `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)`: out-of-range S (x) coordinates clamp to the edge texel.
   - `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)`: same for T (y).
   - The alternatives are `GL_REPEAT` (tile the texture), `GL_MIRRORED_REPEAT` (tile with mirror flips), `GL_CLAMP_TO_BORDER` (return a configurable border color). Clamping is safest for our case — sub-pixel sampling at the edges shouldn't see "the other side" of the field.

5. `glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr)`:
   - **`GL_TEXTURE_2D`** — target.
   - **`0`** — mipmap level; we only have level 0.
   - **`GL_R32F`** — **internal format**: how the GPU stores each texel. `R` = one channel, `32F` = 32-bit float. Per texel: 4 bytes.
   - **`width, height`** — texture dimensions.
   - **`0`** — the legacy "border" parameter, always 0.
   - **`GL_RED, GL_FLOAT`** — format and type of source data (would matter if we were uploading, but...).
   - **`nullptr`** — no data. The driver just allocates `width × height × 4` bytes of GPU memory; contents are undefined.

6. `glBindTexture(GL_TEXTURE_2D, 0)` — unbind.

### GPU side

After the call, the GPU has a chunk of memory sized `width × height × 4` bytes, organized in driver-chosen tile order, marked as `R32F`. Its contents are undefined until `update_field` writes something — the first render before `update_field` would sample garbage.

### Subtleties

- **`GL_R32F` is a single-channel float format.** Useful for storing arbitrary numerical fields — not just colors. Other variants: `GL_R8` (8-bit unsigned byte, 0-255), `GL_R16F` (half float), `GL_R32I` (32-bit signed integer). Each affects per-texel byte cost and precision.
- **Bilinear filtering on a float texture is meaningful.** Each filtered sample is a weighted average of 4 neighbors — for our smooth fluid field, this is exactly what we want.
- **No mipmaps.** We'd need to call `glGenerateMipmap(GL_TEXTURE_2D)` after each upload to produce them. Mipmaps help with minification quality at the cost of ~33% extra memory and per-frame generation work. For a 256×256 texture shown at ~800×800 on screen (3× magnification), mipmaps would never be sampled — they exist for the minification case only.
- **`GL_CLAMP_TO_EDGE` vs the default `GL_REPEAT`**: floating-point rounding in the fragment shader can produce sample coordinates slightly outside [0, 1] at the screen edges. With `GL_REPEAT`, those samples would wrap to the opposite side — you'd see the right edge of the field appearing at the left edge as a one-pixel ghost. `GL_CLAMP_TO_EDGE` is the safe choice for any texture meant to display a finite image rather than a tiling pattern.

---

## `update_arrows`

```cpp
void graphics_engine::update_arrows(const float* u, const float* v, int width, int height, int stride, float scale);
```

**Job:** compute on the CPU the line-segment vertices of a velocity-arrow glyph field, then upload them to the dynamic arrow VBO.

This is the most complex of the six — it combines CPU-side geometry generation, a double loop over the staggered velocity field, and a growable upload pattern.

### Geometry of one arrow

For each sampled cell in the velocity field, we emit:
- **One line segment** for the shaft (base → tip).
- **Two line segments** for the arrowhead (each goes tip → wing endpoint).

Total: 3 line segments = 6 vertices = 12 floats (each vertex is 2 floats).

### The double loop

```cpp
for (int i = stride / 2; i < width; i += stride)
{
    for (int j = stride / 2; j < height; j += stride)
    {
        // per-cell arrow generation
    }
}
```

We sample one cell out of every `stride × stride` block, starting at `(stride/2, stride/2)` so arrows are centered within their blocks. With a 256×256 grid and stride 8, we generate `32 × 32 = 1024` arrows per frame.

### Per-cell arithmetic

1. **Average the staggered velocity to the cell center.** The fluid solver stores `u` on vertical cell faces (between cells) and `v` on horizontal faces. The cell-centered velocity is the average of two adjacent face values:
   ```
   u_center = 0.5 * (u[(i)*y + j] + u[(i+1)*y + j])
   v_center = 0.5 * (v[i*(y+1) + j] + v[i*(y+1) + (j+1)])
   ```

2. **Convert sim cell `(i, j)` to NDC.** Cell `(i, j)`'s center is at `(i + 0.5, j + 0.5)` in sim coordinates. NDC maps [0, width] → [-1, +1], so:
   ```
   base_x = 2 * (i + 0.5) / width - 1
   base_y = 2 * (j + 0.5) / height - 1
   ```

3. **Compute tip displacement.** Velocity × scale gives an NDC offset:
   ```
   delta_x = u_center * scale
   delta_y = v_center * scale
   tip_x = base_x + delta_x
   tip_y = base_y + delta_y
   ```

4. **Push the shaft.** Four floats: base, then tip.

5. **Build the arrowhead** (only if the arrow has nonzero length):
   - **Unit direction** `(dir_x, dir_y) = (delta_x, delta_y) / len`.
   - **Perpendicular** `(perp_x, perp_y) = (-dir_y, dir_x)` (90° counterclockwise rotation of the direction).
   - **Walk back from the tip** by `head_len = arrowhead_ratio * len` along `-dir` to find the arrowhead base.
   - **Sweep sideways** by `head_len * arrowhead_half_spread` along `±perp` to get the two wing endpoints.
   - Push 8 floats: two `tip → wing` line segments.

6. **Zero-length fallback.** If the velocity is below `min_arrow_length`, push 8 zeros (4 copies of `tip`) so every cell contributes exactly 12 floats. This makes the `vertices.reserve(...)` calculation upfront accurate.

### CPU side — buffer allocation strategy

`vertices` is a `std::vector<float>` that we `reserve()` to the worst-case size:
```
(width / stride) * (height / stride) * 12 floats
```

`reserve()` calls `std::vector::reserve` to pre-allocate the backing array. Each `push_back` then just bumps the size counter and writes — no reallocation, no copying. Without `reserve`, the vector would grow geometrically and copy data each time it expanded, causing visible per-frame jitter.

### OpenGL driver side — the grow/reuse pattern

```cpp
glBindBuffer(GL_ARRAY_BUFFER, arrow_vbo);
if (vert_count > arrow_vbo_capacity)
{
    glBufferData(GL_ARRAY_BUFFER, byte_size, vertices.data(), GL_DYNAMIC_DRAW);
    arrow_vbo_capacity = vert_count;
}
else
{
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, vertices.data());
}
```

Two paths:

**Grow path** — when the new vertex count exceeds the buffer's current capacity:
- `glBufferData` **reallocates** the buffer to a larger size, freeing the old storage and allocating new.
- Copies the new data into the freshly allocated storage.
- `GL_DYNAMIC_DRAW` is a usage hint: "I will write to this often; the GPU will read from it for drawing." The driver may use this hint to place the buffer in memory that's mappable from the CPU side (slower for the GPU to read but fast for the CPU to write).
- We track the new capacity in `arrow_vbo_capacity`.

**Reuse path** — when the new data fits in the existing buffer:
- `glBufferSubData` **writes into existing storage** in place. No allocation, no copy aside from the data itself.
- Much cheaper than reallocating, especially when the size doesn't actually change frame-to-frame.

This pattern — track capacity, reallocate only when needed, otherwise reuse — is the standard technique for dynamic GPU buffers.

### GPU side at draw time

`draw` later calls `glDrawArrays(GL_LINES, 0, arrow_vert_count)`:

1. The GPU's vertex puller reads vertices **in pairs**: 0-1 is one line segment, 2-3 is the next, 4-5 the next, etc. With our 12-float-per-arrow layout: vertices 0-1 are the shaft, 2-3 are wing A, 4-5 are wing B.
2. The vertex shader runs once per vertex — for arrows it's almost trivial, just `gl_Position = vec4(a_position, 0.0, 1.0)` (the position is already in NDC).
3. The rasterizer turns each pair into a thin line of fragments using a coverage algorithm (Bresenham-style with anti-aliasing).
4. The fragment shader runs for each covered fragment and outputs solid white.

### Subtleties

- **Why CPU geometry?** Could be done on the GPU with a geometry shader or instanced rendering. CPU is simpler, the per-frame work is modest (~12K floats), and there's no measurable benefit to moving it. A production renderer would more likely use compute shaders.
- **The `if (stride < 1) stride = 1;` clamp** stops a zero or negative stride from causing infinite loops or undefined `width / stride`. This is the one place we mutate a parameter, which is why `stride` is not declared `const` in the signature.
- **The zero-length fallback** matters for buffer math but not for visuals — zero-length line segments rasterize to nothing. The fallback is a CPU-side housekeeping detail that lets `reserve()` correctly predict the final size.
- **`GL_DYNAMIC_DRAW` is a hint, not a contract.** The driver is free to ignore it. In practice most drivers use it to choose between VRAM and shared system memory placement.
- **No view/projection matrix anywhere.** Both the quad and the arrows are computed directly in NDC. The vertex shaders are trivial because the CPU has already done the only spatial transform that matters (sim cell → NDC).

---

## A pattern across all six

If you read them in order, three recurring patterns appear:

1. **Allocate once, fill many times.** `create_field_texture` allocates GPU memory; `update_field` overwrites its contents. `create_arrow_buffer` configures the VAO; `update_arrows` fills the VBO. Static structure (formats, layouts, sizes) is set up at startup. Dynamic content is the per-frame work.

2. **State machine usage.** Almost every OpenGL call leans on the "currently bound" object. `glBindTexture`, `glBindBuffer`, `glBindVertexArray`, `glUseProgram` — each one updates a global slot. Then state-setting calls (`glTexParameteri`, `glVertexAttribPointer`, etc.) target whatever was last bound. This is why bookkeeping discipline matters in OpenGL — leaving the wrong object bound can break the next unrelated call.

3. **The CPU drives, the GPU executes.** The CPU side composes commands and uploads data. The GPU side runs shader programs on many cores in parallel against that data. The two sides communicate only through buffers and textures (data) and uniforms (small per-draw parameters). Nothing in these six functions actually *runs* on the GPU — they all just prepare GPU resources. The execution happens later in `draw`.

---

# Part 2 — `main.cpp`

`main.cpp` is the **glue layer** between two worlds. On one side is the C physics engine (`src/fluid_physics/`, read-only, written in C11). On the other is the C++ renderer (`graphics_engine.{hpp,cpp}`, covered in Part 1). `main.cpp` wires them together, owns the UI state, runs the per-frame loop, and serves as the program's entry point.

Top-down, it contains:
- An `extern "C"` block that pulls in the C physics headers.
- File-scope `constexpr` constants for window size, fluid defaults, and timing.
- A `runtime_controls` struct holding the UI's mutable state.
- Two dispatcher helpers: `pick_solver`, `pick_precond`.
- Two data-shaping helpers for the renderer: `scan_min_max`, `compute_velocity_magnitude`.
- A simulation-reset helper: `reload_scenario`.
- The ImGui widget submitter: `draw_control_panel`.
- `main()` — initialization, frame loop, cleanup.

---

## The `extern "C"` block

```cpp
extern "C"
{
    #include "types.h"
    #include "core.h"
    #include "scenarios.h"
    #include "preconditioners.h"
}
```

**Job:** make C functions callable from C++ code despite the two languages having different rules for how function symbols appear in object files.

### Why this exists — name mangling

C++ supports function overloading: two functions with the same name but different parameter types coexist as distinct functions. The linker doesn't natively understand "two functions with the same name," so the compiler decorates each function's symbol with extra characters that encode its full type. This decoration is called **name mangling**.

A C++ function declared as `void fluid_step(FluidContext*, ScenarioParams, Scenario)` becomes a symbol like `_Z10fluid_stepP12FluidContext14ScenarioParams8Scenario` in the compiled `.o` file. Different overloads → different mangled symbols → no ambiguity at link time.

**C has no name mangling.** `fluid_step` in C is just `fluid_step` in the binary. The C compiler emitted that exact symbol when it compiled `core.c`.

Now consider compiling `main.cpp` without `extern "C"`:
1. The C++ compiler reads `core.h` and sees `void fluid_step(...)`.
2. It assumes (correctly for a C++ context) that the function is C++ code.
3. At the call site `fluid_step(fluid_context, params, scenario);`, it emits a call to `_Z10fluid_step...` (the mangled name).
4. The linker tries to resolve `_Z10fluid_step...` — but the actual symbol in `core.c.o` is just `fluid_step`. **Link error: undefined symbol.**

`extern "C"` is the fix. It tells the C++ compiler: "the names inside this block use **C linkage** — do not mangle them, look them up under their bare names." With `extern "C"` wrapping the header, the C++ compiler emits a call to `fluid_step` directly, the linker finds the C-compiled symbol, everything connects.

### Why the headers were not authored with `extern "C"` already

In a properly-designed C library meant to be consumable from both C and C++, the header would self-protect with:

```c
#ifdef __cplusplus
extern "C" {
#endif

// ... declarations ...

#ifdef __cplusplus
}
#endif
```

This guard would make the header work seamlessly from C (where `extern "C"` doesn't exist) and from C++ (where it kicks in). The fluid physics headers don't have this guard — they were written purely with C consumers in mind. So the **C++ caller is responsible for wrapping the include**, which is what `main.cpp` does.

### A second thing the wrapper does

Beyond suppressing name mangling, `extern "C"` also forces the declarations inside to follow **C calling convention**. The differences between C and C++ calling conventions are minor on most platforms — argument passing, register usage — but they exist. Wrapping the headers ensures the C++ side calls the C functions the way the C compiler expected them to be called.

### What's actually in those four headers

- `types.h` — the core struct `FluidContext` and the function-pointer typedefs `PressureSolver` and `Preconditioner`.
- `core.h` — declares `fluid_create_context`, `fluid_destroy_context`, `fluid_step`, `fluid_setup_physics`, and the three solver functions.
- `scenarios.h` — declares the `Scenario` struct, the `ScenarioType` enum, and `load_scenario`.
- `preconditioners.h` — declares the three preconditioner functions and the `PrecondType` enum.

All four are pulled in because `main.cpp` references types or functions from each.

---

## File-scope constants

```cpp
constexpr float pi = 3.14159265358979323846f;

constexpr int window_width = 800;
constexpr int window_height = 800;
constexpr const char* window_title = "Fluid Simulation";

constexpr size_t default_grid_size = 256;
constexpr float default_dt = 0.016f;
// ... more defaults ...

constexpr double title_update_interval = 0.5;
```

`constexpr` declares a value **known at compile time**. The compiler can substitute it into call sites directly — no runtime storage allocated, no memory address, no overhead at run time. It's the modern, type-safe replacement for C-style `#define` constants.

What lives here:
- **`pi`** — used by the auto-omega formula in the frame loop. `float` precision is sufficient for this calculation.
- **Window dimensions and title** — passed to `graphics_engine`'s constructor once at startup. Centralizing them at file scope makes them easy to change without hunting through the code.
- **Fluid defaults** — passed to `fluid_create_context` once. They document the simulation's initial state.
- **`title_update_interval`** — the gap (in seconds) between FPS updates in the window titlebar. Half a second is fast enough to feel live and slow enough that the number doesn't flicker.

The cost is zero at runtime. Every reference to `window_width` becomes the literal `800` at compile time.

---

## `runtime_controls` struct

```cpp
struct runtime_controls
{
    visual_mode mode = visual_mode::smoke;
    ScenarioType scenario_type = KARMAN_VORTEX;
    int substeps_per_frame = 5;
    int arrow_stride = 8;
    float arrow_scale = 0.04f;
    bool auto_omega = true;
    bool request_reset = false;
    bool request_rebuild_scenario = false;

    int solver_index = 0;
    int preconditioner_index = 1;
};
```

A passive data struct. Public fields, no methods, no invariants — exactly the AGENTS.md description of a `struct` rather than a `class`.

The struct's role: **hold the UI's mutable state across frames.** Every field falls into one of two categories:

**Persistent settings** — the user's choices that survive frame-to-frame. `mode` (which visualization), `scenario_type` (which test case), `substeps_per_frame` (how fast to sim), `arrow_stride` / `arrow_scale` (how dense the arrows are), `auto_omega` (whether to recompute SOR's omega), `solver_index` / `preconditioner_index` (which physics implementations).

**One-shot flags** — set by a button click on one frame, consumed by the main loop the next frame, then cleared. `request_reset` and `request_rebuild_scenario` work this way.

Each field has an **inline default** (the `= value` next to the declaration). When `main()` writes `runtime_controls controls;`, the resulting object starts with every field initialized to its default — no constructor needed. This is the AGENTS.md "default member initializers" rule in action; the alternative (constructors that initialize each field) would mean adding code that future-you might forget to update when adding a new field.

The choice of `int` for `solver_index` / `preconditioner_index` rather than a proper enum is dictated by **ImGui's API**: `ImGui::Combo` takes an `int*` and writes the selected index back. Passing an enum pointer wouldn't compile. So we keep the int, then convert to a function pointer (via `pick_solver` / `pick_precond`) at the use site.

---

## `pick_solver` and `pick_precond`

```cpp
[[nodiscard]] PressureSolver pick_solver(const int index)
{
    switch (index)
    {
        case 1: return solve_pressure_rbgs;
        case 2: return solve_pressure_sor;
        default: return solve_pressure_pcg;
    }
}
```

Map an integer (the user's combo-box selection) to a **function pointer** — the address of an actual physics function defined in C.

### What a function pointer is

`PressureSolver` is a `typedef` in `types.h` for a function-pointer type — roughly:

```c
typedef void (*PressureSolver)(FluidContext* ctx, float* p, const float* div);
```

Reading the typedef: "PressureSolver is a name for the type `pointer to a function that takes (FluidContext*, float*, const float*) and returns void`."

When the C compiler compiled `solve_pressure_pcg`, it placed the function's machine code at some address in the binary's `.text` section. That address — say `0x400a20` — is what a `PressureSolver` variable holds. Calling through the pointer (`ctx->pressure_solver(ctx, p, div)`) just performs an indirect jump to that address.

### Why this is useful

Inside `fluid_step`, the physics engine doesn't know which solver will run. It just calls `ctx->pressure_solver(...)`. The actual function is whichever one we plugged in via `fluid_setup_physics` or by direct assignment.

This is **runtime polymorphism without classes**. C++ would use virtual functions or `std::function` to achieve the same thing. C uses raw function pointers. Either way, the runtime decision of "which implementation to call" is made by changing a pointer rather than by writing branching code inside the caller.

### Why `[[nodiscard]]`

If you call `pick_solver` and throw away the return value, you've done nothing useful. The whole point is the function pointer it returns. `[[nodiscard]]` makes the compiler warn when a call site ignores the return.

`pick_precond` follows the exact same pattern for preconditioners.

---

## `scan_min_max`

```cpp
void scan_min_max(const float* data, const size_t count, float& out_min, float& out_max);
```

Walk an array of floats once, find the smallest and largest values, return them through reference parameters.

### The +/- infinity trick

```cpp
out_min = +infinity;
out_max = -infinity;
for each value:
    if (value < out_min) out_min = value;
    if (value > out_max) out_max = value;
```

Starting from `+infinity` for the min and `-infinity` for the max is a small idiom that avoids a "first iteration" special case. Any real number is less than `+infinity` and greater than `-infinity`, so the first value seen will replace both initial values correctly. Without this trick you'd need either a flag for "have we seen any values yet" or a separate first-iteration assignment.

`std::numeric_limits<float>::infinity()` produces the IEEE-754 positive infinity bit pattern (all exponent bits set, all mantissa bits zero). Comparing any finite float to it works as expected.

### The NaN/Inf fallback

```cpp
if (!std::isfinite(out_min) || !std::isfinite(out_max))
{
    out_min = 0.0f;
    out_max = 1.0f;
}
```

If the simulation produces NaN or Inf values (which can happen for unstable parameter combinations — extreme dt, low viscosity, certain solver/scenario combos), the scan would propagate them into the range. The renderer would then divide by NaN, the colormap would crash or display garbage.

Falling back to `[0, 1]` keeps the renderer working. The user sees a uniform color (probably the midpoint of the colormap) instead of a crash — and can recover by hitting Reset.

This is one of the few places in the code where defensive programming is justified: an external system (the C solver) can hand us pathological values, and we're at the boundary.

### Why reference parameters

`out_min` and `out_max` are **reference parameters**, written `float&`. The function modifies the caller's variables in place. The signature says "these are non-nullable lvalues you'll receive output through." If we'd used `float*`, the signature would have said "null is OK," which it isn't.

Could the function return a `struct { float min; float max; }` instead? Yes, and that would arguably be cleaner. The current code prefers the out-parameter idiom — that's a stylistic choice consistent with the rest of the file.

---

## `compute_velocity_magnitude`

```cpp
void compute_velocity_magnitude(const FluidContext* fluid_context, float* out,
                                float& out_min, float& out_max);
```

Walk every cell in the simulation grid, compute the magnitude of the velocity vector at that cell, write it into `out[]`, and report the min/max along the way.

### The staggered-grid average

```cpp
const float u_center = 0.5f * (fluid_context->u[i * height + j] +
                               fluid_context->u[(i + 1) * height + j]);
const float v_center = 0.5f * (fluid_context->v[i * height_plus_one + j] +
                               fluid_context->v[i * height_plus_one + (j + 1)]);
```

The fluid physics uses a **staggered MAC grid**: u-velocity values live on the vertical faces between columns of cells; v-velocity values live on the horizontal faces between rows. There's no "u at cell (i, j)" — there's u at the left face and u at the right face.

To get a cell-centered velocity (what the renderer wants to display), we **average the two adjacent face values**. For cell `(i, j)`:
- u_center = average of u at face `i` (left) and u at face `i+1` (right).
- v_center = average of v at face `j` (bottom) and v at face `j+1` (top).

The indexing `i * height + j` and `i * height_plus_one + j` reflects the array sizes — the u array has size `(width+1) * height` (one extra column for the right-most face), and the v array has size `width * (height+1)` (one extra row for the top face).

Why staggered? Numerical stability in the pressure-velocity coupling. The physics engine's job; the renderer's job is just to undo the staggering for display.

### The magnitude computation

```cpp
const float magnitude = std::sqrt(u_center * u_center + v_center * v_center);
out[i * height + j] = magnitude;
if (magnitude < out_min) out_min = magnitude;
if (magnitude > out_max) out_max = magnitude;
```

Pythagorean theorem on the velocity components. Write the result to the cell-centered output array. Update min/max in the same pass — no second walk needed.

### A subtle additional guard

```cpp
if (!std::isfinite(out_min) || !std::isfinite(out_max) || out_max <= out_min)
{
    out_min = 0.0f;
    out_max = 1.0f;
}
```

The extra condition `out_max <= out_min` handles the case where every cell has the **same** magnitude — including the all-zero case right after a reset. Without this guard, range = 0, and the fragment shader's normalization (raw - range_min) / (range_max - range_min) would divide by zero. Falling back to `[0, 1]` keeps things stable.

This is one of the **hot loops** in the application — runs every frame in the velocity-magnitude and field+vectors modes. For a 256×256 grid, that's ~65,000 iterations per frame. The compiler will likely vectorize the inner arithmetic with SIMD.

---

## `reload_scenario`

```cpp
[[nodiscard]] Scenario reload_scenario(FluidContext* fluid_context,
                                        ScenarioParams& params,
                                        const ScenarioType type,
                                        const PressureSolver solver,
                                        const PrecondType precond);
```

Reset the simulation to a fresh state and reconfigure it for a chosen scenario.

### Step 1: zero everything

```cpp
std::memset(fluid_context->u, 0, u_count * sizeof(float));
std::memset(fluid_context->v, 0, v_count * sizeof(float));
std::memset(fluid_context->p, 0, cells * sizeof(float));
// ... 10 more arrays ...
```

`std::memset(ptr, byte_value, count)` writes `count` copies of `byte_value` starting at `ptr`. We pass `0` as the byte value, so every byte gets cleared.

This works for our purposes because:
- **`float` zero** in IEEE-754 is all bits zero — so `memset(ptr, 0, n*sizeof(float))` does correctly zero out an array of floats.
- **`uint8_t solid` zero** means "this cell is fluid, not solid" — exactly what we want for a fresh start.

`memset` is **dramatically faster than a per-element loop** because modern implementations use SIMD or even non-temporal stores to clear large regions. For our 256×256 grid with 13 arrays each holding 256KB+, the savings are substantial. The C++ standard library detail: most implementations of `memset` are written in hand-optimized assembly per CPU architecture.

We can use `memset` here because all these arrays are **plain old data** (`float` or `uint8_t`). For a C++ object array with destructors or non-trivial copy semantics, `memset` would corrupt the objects. Floats and bytes have no such concerns.

### Step 2: re-init for the chosen scenario

```cpp
Scenario scenario = load_scenario(type, fluid_context, &params);
fluid_setup_physics(fluid_context, params, solver, precond);
scenario.init(fluid_context, params);
return scenario;
```

- `load_scenario` reads the `ScenarioType` enum and returns a `Scenario` struct containing three function pointers: `init`, `apply_sources`, `apply_boundaries`. Each scenario (Lid-Driven, Karman, Airfoil, Urban City) has its own set of three functions; `load_scenario` picks the right trio.
- `fluid_setup_physics` plugs in the chosen pressure solver and preconditioner, and computes scenario-derived parameters (e.g., the Reynolds number from the inlet velocity and grid scale).
- `scenario.init(fluid_context, params)` runs the scenario's one-time setup: placing obstacles in the `solid` mask, optionally seeding the initial smoke field, etc.

The returned `Scenario` struct is the caller's only handle to the scenario's per-step callbacks (`apply_sources`, `apply_boundaries`). `fluid_step` needs it every frame.

### Why `[[nodiscard]]`

Forgetting to capture the return would silently break the simulation — `fluid_step` would call whatever stale `Scenario` struct main has. The attribute forces a compile warning at any call site that drops the return.

### Why `ScenarioParams& params` and not `ScenarioParams* p`

References are non-null, pointers can be null. The scenario init code dereferences `params` unconditionally — null would crash. Encoding "this can't be null" in the type catches misuse at the call site rather than in production.

---

## `draw_control_panel`

```cpp
void draw_control_panel(runtime_controls& controls, FluidContext* fluid_context,
                        ScenarioParams& params);
```

Issue all the ImGui widget calls for this frame's control panel.

### Immediate-mode UI

Most UI libraries are **retained-mode**: you describe a widget tree once (a window, with buttons and labels), the library stores it, you wire up event handlers for clicks, and the library renders the tree until you change it.

ImGui is **immediate-mode**. There is no stored widget tree. Every frame, you describe the entire UI from scratch by calling functions. Widget state, event handling, and rendering are all combined into a sequence of plain function calls. If you don't call `ImGui::Button("Reset")` this frame, no button gets drawn this frame.

Every ImGui function does two things simultaneously:
1. **Submits geometry** — adds vertices to ImGui's internal vertex buffer (which will be uploaded and drawn when `ImGui::Render` runs later).
2. **Handles input** — reads the current mouse/keyboard state and decides whether the user just interacted with this widget.

So `ImGui::Button("Reset")`:
- Pushes vertices for a button-shaped rectangle and the text "Reset" into the vertex buffer.
- Checks if the mouse is hovering over those vertices and if it was clicked.
- Returns `true` once if the click happened, `false` otherwise.

### Reading state vs writing state

For display-only widgets, you just call them:

```cpp
ImGui::Text("Reynolds: %.2f", fluid_context->reynolds);
ImGui::Text("OpenMP threads: %d", omp_get_max_threads());
```

These read from C++ state and submit text geometry. No state is mutated.

For widgets that let the user **modify** state, you pass a pointer (or reference, for some types) to the variable they should write back to:

```cpp
ImGui::SliderFloat("Inlet Velocity", &params.inlet_velocity, 0.0f, 5.0f);
```

The slider reads `params.inlet_velocity` to know where its handle should start, and writes back to that same memory location if the user drags it.

Some widgets return `true` on change and let you react inline:

```cpp
if (ImGui::Combo("Pressure Solver", &controls.solver_index, solver_items, IM_ARRAYSIZE(solver_items)))
    fluid_context->pressure_solver = pick_solver(controls.solver_index);
```

The body runs only on the frame the user actually changed the combo — most frames it's a no-op.

### `static` arrays

```cpp
static const char* scenario_items[] = { "Lid-Driven", "Karman Vortex", "Airfoil", "Urban City" };
```

A `static` local variable persists across calls — allocated once, never re-initialized. Without `static`, we'd create this array every frame (~60 times per second). With `static`, it's created once and reused. The contents never change, so this is safe and cheap.

### Conditional widgets

```cpp
ImGui::Checkbox("Auto Omega", &controls.auto_omega);
if (controls.auto_omega)
    ImGui::Text("Omega: %.4f (auto)", fluid_context->omega);
else
    ImGui::SliderFloat("Omega", &fluid_context->omega, 1.0f, 1.99f);
```

The slider only exists when the checkbox is off. When it's on, a read-only text display takes its place. In retained-mode UIs this would require creating and destroying widget objects on every toggle. In immediate-mode, you just `if`/`else` your function calls — the widget only gets submitted when the branch is taken.

Same idea for the Karman obstacle controls — they only appear when the Karman scenario is selected.

### The integer trampoline pattern

```cpp
int mode = (int)controls.mode;
ImGui::RadioButton("Smoke", &mode, (int)visual_mode::smoke);
ImGui::RadioButton("Pressure", &mode, (int)visual_mode::pressure);
// ...
controls.mode = (visual_mode)mode;
```

`ImGui::RadioButton` takes `int*` (the selected value) and `int` (this radio's value). Our `controls.mode` is a `visual_mode` enum, not an int. So we:
1. Copy the enum's integer value into a temporary `mode`.
2. Pass `&mode` to each radio button — they all modify the same temporary.
3. Cast the temporary back to the enum and store it.

This is the **integer trampoline** that immediate-mode-meets-strongly-typed-enum forces on us. The cost is one int copy per frame; the readability win of keeping `controls.mode` strongly typed is worth it.

### One-shot button flags

```cpp
if (ImGui::Button("Reset"))
    controls.request_reset = true;
```

`ImGui::Button` returns `true` exactly once — on the frame the click happened. We use that to set a flag on the controls struct. The main loop reads the flag next, takes the action (calling `reload_scenario`), and clears the flag. This separates "user requested" from "action taken" by exactly one frame, which is fine for human-interactive purposes.

---

## `main` — the application entry point

This is where everything comes together. The function divides into three phases: **initialization**, the **frame loop**, and **cleanup**.

### Initialization

```cpp
graphics_engine engine(window_width, window_height, window_title);
```

Constructs the renderer on the stack. The constructor — covered in Part 1 — initializes GLFW, creates the window, loads OpenGL function pointers, compiles the shaders, builds the quad geometry, sets up the dynamic arrow buffer, and initializes ImGui. Everything graphical comes online here.

`engine` is a **stack variable**. When `main` exits, its destructor runs automatically, releasing every GPU resource, the GLFW window, and the ImGui context. This is RAII (Part 1 covers the pattern in detail).

```cpp
FluidContext* fluid_context = fluid_create_context(
    default_grid_size, default_grid_size,
    default_dt, default_dx,
    default_density, default_viscosity,
    default_poisson_iter, default_threshold);
```

Asks the C physics engine to heap-allocate a `FluidContext`. This is **C code, not C++**. The returned pointer points at memory allocated by C's `calloc` (inside the physics implementation). There is no destructor. We must call `fluid_destroy_context` ourselves to free it — which we do, manually, at the bottom of `main`.

```cpp
runtime_controls controls;
ScenarioParams params;
Scenario scenario = reload_scenario(fluid_context, params, controls.scenario_type,
                                     pick_solver(controls.solver_index),
                                     pick_precond(controls.preconditioner_index));
```

Three more stack values. `controls` and `params` default-construct (their fields take their inline defaults). `scenario` is initialized by `reload_scenario`, which clears every array in the fluid context and re-runs the chosen scenario's init.

```cpp
std::vector<float> velocity_magnitudes((size_t)fluid_context->num_cells, 0.0f);
```

Pre-allocate the work buffer that `compute_velocity_magnitude` will fill. Sized to one float per simulation cell. Allocating it **once**, outside the loop, avoids re-allocating heap memory every frame.

The `std::vector` constructor used here takes `(count, initial_value)` — allocates a buffer of `num_cells` floats, all initialized to `0.0f`.

```cpp
double last_time = glfwGetTime();
double title_timer = 0.0;
```

GLFW provides a monotonic clock via `glfwGetTime()` — returns seconds since GLFW was initialized as a `double`. We use this for frame-time measurement and FPS calculation.

### The frame loop

```cpp
while (!engine.should_close())
```

`should_close()` asks GLFW whether the user has signaled close (clicked the X, hit Alt+F4, etc.). GLFW polls window events when we call `glfwPollEvents` later in the frame; that's when the close flag gets set. After the flag is set, `should_close()` returns `true` and the loop exits.

#### Time delta

```cpp
const double now = glfwGetTime();
const double delta_time = now - last_time;
last_time = now;
```

Measure how long the previous frame took. `delta_time` is the wall-clock time between the start of this frame and the start of the previous frame. Used for the FPS readout and for the title-update timer — not for the physics, which uses its own fixed `dt`.

#### Auto-omega

```cpp
if (controls.auto_omega)
    fluid_context->omega = 2.0f / (1.0f + std::sin(pi / (float)fluid_context->x));
```

If auto-omega is enabled, recompute the optimal SOR relaxation parameter every frame. The formula `2 / (1 + sin(π / N))` is the theoretical optimum for SOR on an N×N regular grid. Recomputing per frame is cheap (one sin, one division) and ensures the value stays right even if the user rebuilds the scenario at a different grid size.

#### Physics steps

```cpp
for (int s = 0; s < controls.substeps_per_frame; ++s)
    fluid_step(fluid_context, params, scenario);
```

Run the simulation one or more times. Each `fluid_step` advances the simulation by `fluid_context->dt` simulated seconds — for the default `dt = 0.016`, one step covers 16ms of simulated time.

This is **physics-render decoupling by design**: simulated time is independent of wall-clock time. With `substeps_per_frame = 5`, each rendered frame advances the simulation by ~80ms of simulated time. The sim plays back faster than real-time. Lower the substeps and the sim slows down.

A real-time game engine would typically use a **fixed-timestep accumulator** pattern that keeps simulated time in sync with wall-clock time even when frames take variable amounts of real time. This is a visualization tool, not a game — we don't care about sync.

#### ImGui frame

```cpp
engine.begin_ui();
draw_control_panel(controls, fluid_context, params);
```

`begin_ui()` opens the ImGui frame — sets up internal state so the upcoming ImGui calls have something to write into. Then `draw_control_panel` issues every widget call. After it returns, all UI input has been collected and all UI geometry has been submitted into ImGui's internal vertex buffer.

#### Reset handling

```cpp
if (controls.request_reset || controls.request_rebuild_scenario)
{
    scenario = reload_scenario(...);
    controls.request_reset = false;
    controls.request_rebuild_scenario = false;
}
```

The control panel's "Reset" and "Rebuild Solids" buttons set these flags. The main loop consumes them here: zero the sim, re-init, clear the flags so the reset only happens once.

Note: this comes **after** the physics step but before the render. So in the reset-frame, the physics ran one last step on the old data, then we resetted everything before drawing. The very next frame, the renderer sees the freshly-reset state.

#### Field upload by mode

```cpp
const int width = (int)fluid_context->x;
const int height = (int)fluid_context->y;

switch (controls.mode)
{
    case visual_mode::smoke:
        engine.update_field(fluid_context->smoke, width, height, 0.0f, 1.0f);
        break;
    // ...
}
```

Based on the user's choice of visualization, decide what data to upload to the field texture:
- **Smoke** — upload the smoke density field directly. Range is fixed `[0, 1]` because smoke is bounded.
- **Pressure** — scan the pressure field for its actual min/max, fallback to `[0, 1]` if the range is degenerate, then upload.
- **Velocity magnitude** and **Field + Vectors** — compute per-cell magnitudes into the pre-allocated work buffer, then upload. Both visualization modes share this branch via case fall-through (the C++ way of saying "two cases, same body").
- **Vectors only** — no field upload at all. The renderer will skip the field pass.

#### Arrow buffer rebuild

```cpp
if (controls.mode == visual_mode::velocity_vectors_only ||
    controls.mode == visual_mode::field_plus_vectors)
    engine.update_arrows(fluid_context->u, fluid_context->v, width, height,
                         controls.arrow_stride, controls.arrow_scale);
```

For the two modes that draw arrows, rebuild the arrow VBO from the current velocity field. (Covered in detail in Part 1.)

#### Draw

```cpp
engine.draw(controls.mode);
```

Run the actual GL render passes — clear, conditional field pass, conditional arrow pass, ImGui draw data, swap buffers, poll events. This is the moment when all the work accumulated this frame becomes visible on screen.

`glfwPollEvents()` inside `draw` is the call that drains the OS event queue — keyboard, mouse, window close requests, resize events. This is where `should_close()` actually learns the user clicked the X.

#### Title update

```cpp
title_timer += delta_time;
if (title_timer > title_update_interval)
{
    char title_buffer[80];
    std::snprintf(title_buffer, sizeof(title_buffer),
                  "Fluid Simulation - %.1f FPS",
                  1.0 / (delta_time > 0.0 ? delta_time : 1.0));
    glfwSetWindowTitle(engine.window(), title_buffer);
    title_timer = 0.0;
}
```

Accumulate elapsed time in `title_timer`. When it crosses the threshold (half a second), format the current FPS into a stack-allocated `char[80]` buffer using `snprintf` (safe — it truncates rather than overflows), then ask GLFW to update the titlebar.

The `delta_time > 0.0 ? delta_time : 1.0` guard avoids a divide-by-zero on the very first frame when `delta_time` could be `0`.

After the update, reset `title_timer` so we wait another half-second.

### Cleanup

```cpp
fluid_destroy_context(fluid_context);
return 0;
```

After the frame loop exits, free the C-side fluid context. This calls the C engine's deallocator, which frees every internal array.

Then `main` returns. The C++ stack locals destruct in **reverse order of construction**:
1. `velocity_magnitudes` — `std::vector` destructor frees the heap buffer.
2. `scenario` — trivial destruct (just function pointers).
3. `params` — trivial destruct.
4. `controls` — trivial destruct.
5. `engine` — destructor runs: shuts down ImGui, deletes every GL handle, destroys the GLFW window, calls `glfwTerminate`.

The fact that `engine` constructs first and destructs last is the standard RAII pattern: the longest-lived resource is the outermost. By the time `main` returns its `0`, every resource is released and the process exits cleanly.

---

## Patterns across main.cpp

Three themes recur through the file:

1. **One-frame propagation.** UI changes don't take effect instantly — they're stored as struct fields, then read by the main loop on the next iteration. `request_reset` is set by an ImGui button on frame N, the reload happens on frame N+1. This decoupling keeps the ImGui call sequence linear (no callbacks reaching into other systems) and makes the data flow easy to reason about.

2. **The C/C++ seam.** `extern "C"`, `FluidContext*` lifetime managed by hand, `memset` on plain-data arrays, function pointers as runtime polymorphism — all the C-isms cluster around the boundary with the physics engine. On the C++ side, `engine` and `velocity_magnitudes` use RAII; on the C side, the context is heap-allocated and explicitly freed. The file is the only place these two worlds touch.

3. **The renderer is a slave to the sim.** Every frame: simulate first, then look at what the user wants to see, prepare exactly that data for the GPU, then draw. The renderer never asks the simulation for anything; the application drives the data flow. Switching visualization modes is a CPU-side `switch` statement deciding which field to upload — the renderer is the same shape whether we're showing smoke or pressure or velocity arrows.
