# Coding Standards

This project's **C++ graphics code** follows these rules. Apply them at all times.

The **C fluid simulation under `src/fluid_physics/`** is owned by a separate developer and is **read-only** — these rules do not govern that code, and it must not be modified, reformatted, or refactored. Read it only for understanding.

## Formatting

### Indentation

Four spaces per level. Never tabs. Never two spaces.

### Braces

**Always on a new line** (Allman style), no exceptions.

```cpp
// BAD
if (ready) {
    do_thing();
}

// GOOD
if (ready)
{
    do_thing();
}
```

### Pointer and reference declarations

Asterisk and ampersand sit on the **type** side, not the name side.

```cpp
// GOOD
int* ptr;
const my_class& reference;
shader_program* shader = nullptr;

// BAD
int *ptr;            // C tradition (name side)
int * ptr;           // separated
```

Write **one declaration per line** when the type contains `*` or `&` — `int* a, b;` declares `a` as a pointer but `b` as a plain `int`, which is almost never what you want.

### `const` placement

**West const** — `const` on the left of what it modifies.

```cpp
// GOOD
const int* ptr;
const my_class& reference;
const std::vector<int>& list;

// BAD (east const)
int const* ptr;
my_class const& reference;
```

### Variable initialization

**Copy initialization** by default — matches C# syntax, easy to read.

```cpp
// GOOD
int width = 800;
float ratio = 16.0f / 9.0f;
auto count = vertices.size();

// Avoid the older direct-init form for new code:
// int width(800);   // function-call syntax; subject to most-vexing-parse
```

Be aware: copy init silently allows narrowing — `int x = 3.14;` truncates to 3 with no warning. Pay attention when assigning floating-point to integer or wider types to narrower ones.

### Default member initializers

When declaring a field in a class or struct, initialize it inline. Do not rely on the constructor to remember to do it.

```cpp
// GOOD
class renderer
{
private:
    int width = 0;
    int height = 0;
    bool initialized = false;
    GLFWwindow* window = nullptr;
};

// BAD — every constructor must remember every field
class renderer
{
public:
    renderer() : width(0), height(0), initialized(false), window(nullptr) {}

private:
    int width;
    int height;
    bool initialized;
    GLFWwindow* window;
};
```

Inline defaults guarantee a known starting state even if a future constructor forgets a field.

### Header guards

Always `#pragma once` at the top of every header. No traditional `#ifndef` / `#define` / `#endif` guards.

```cpp
// GOOD
#pragma once

class shader_program { ... };
```

### Alignment

**No alignment spacing.** Never pad operators with extra spaces to align values across lines.

```cpp
// BAD
constexpr const char* loading    = "loading";
constexpr const char* level_fail = "level_fail";

// GOOD
constexpr const char* loading = "loading";
constexpr const char* level_fail = "level_fail";
```

### Magic numbers

Extract to a named variable or `constexpr` — prefer variable.

### Blank lines

**Max one blank line** between members. No consecutive blank lines inside functions or blocks.

### Regions

**No `#pragma region` blocks.** If you feel the need for one, the class probably needs to be split instead.

### Enums

Always `enum class`, never plain `enum`. One value per line, with a trailing comma. Never write an enum on a single line.

```cpp
// BAD
enum class out_of_lives_result { got_lives, dismissed };

// GOOD
enum class out_of_lives_result
{
    got_lives,
    dismissed,
};
```

## Comments

Comments exist only for code with **high cognitive load** or **non-obvious intent**. Good naming and structure should carry the weight. Do not narrate what the code does.

```cpp
// BAD — states the obvious
// Close the view when the level loads
level_manager.on_level_loaded += [this] { close(); };

// GOOD — explains why, not what
// Sequence must complete before input is re-enabled
canvas.set_interactable(false);
```

**Never write Doxygen documentation comments** — `/** */` blocks, `///` lines, `@brief`, `@param`, `@return`, `@see`, or any structured doc comment. Not on public functions, not on private helpers, not anywhere. They restate what a well-named function already says, inflate the file, and discourage the real fix: improving the name.

```cpp
// BAD — Doxygen comment narrates the obvious and bloats the header
/// @brief The next feature the player is progressing toward, or null if all are unlocked.
feature_unlock_data* get_next_locked_feature();

// GOOD — the function name carries the meaning
feature_unlock_data* get_next_locked_feature();
```

If the name cannot carry the meaning, rename the function or split it. Do not fall back to Doxygen.

## Naming

This project uses **snake_case** for almost everything. The only universally-uppercase tokens are macro definitions (which we use sparingly).

- **Types and structs:** `shader_program`, `texture_2d`, `render_pass`, `out_of_lives_flow`.
- **Functions and methods:** `is_unlocked()`, `get_next_locked_feature()`, `register_flow()`.
- **Variables and parameters:** `width`, `frame_count`, `current_feature`.
- **Members:** `int frame_count = 0;`. **No `m_` prefix, no `_` prefix or suffix.**
- **Enum class values:** `vis_mode::smoke`, `out_of_lives_result::got_lives`.
- **Files:** `graphics_engine.hpp`, `shader_program.hpp`, `render_pass.cpp`.
- **Namespaces:** `graphics`, `physics_bridge`.
- **Constants and `constexpr`:** `constexpr int max_lights = 32;`.
- **Macros (rare; only conditional compilation or platform shims):** `OPENGL_DEBUG`, `PLATFORM_WINDOWS`.

### Names must be short *and* understandable

A name is short enough when it carries no filler and long enough when a reader does not have to guess what it refers to. Never collapse a meaningful noun to one or two letters. `f` for `feature`, `fud` for `feature_unlock_data`, `lm` for `level_manager`: none of these save the human anything, and the computer does not care. Type out the word.

```cpp
// BAD
bool is_unlocked(const feature_unlock_data& f) { ... }
for (size_t i = 0; i < features.size(); ++i)
{
    auto& f = features[i];
    // ...
}

// GOOD
bool is_unlocked(const feature_unlock_data& feature) { ... }
for (size_t i = 0; i < features.size(); ++i)
{
    auto& feature = features[i];
    // ...
}
```

The only tolerated single-letter names are integer loop indices in nested numeric iteration (`for (size_t i = 0; ...)`, `for (size_t j = 0; ...)`) where there is no domain object to name. Everything else spells out the word.

### Callable invocation

Always check `std::function` (or any other callable that may be empty) before invoking. Use `if (callback) callback(...);` — never call a possibly-empty callable.

## Function Parameters

Express null-vs-non-null and copy-vs-reference through the parameter type itself. The signature is the documentation.

- **`int`, `float`, `bool`, small enums, small structs (~16 bytes):** pass by value — `void foo(int x)`.
- **Larger types you don't modify:** pass by const reference — `void foo(const my_class& x)`.
- **Larger types you do modify:** pass by mutable reference — `void foo(my_class& x)`.
- **Genuinely nullable inputs:** pass by pointer — `void foo(my_class* x)`. The pointer-ness signals that `nullptr` is a valid input.

```cpp
// BAD — `mesh*` here is non-null in practice; the pointer hides the contract
void render(mesh* m, float dt);

// GOOD
void render(const mesh& m, float dt);   // can't be null
void render_optional(const mesh* m);    // null is valid input
```

A reader should be able to tell from the signature alone whether they need to handle null — references say "no", pointers say "maybe".

## Visibility

Default to `private`. Promote a member to `protected` only when there is a real subclass relationship that uses it; promote to `public` only for genuine cross-class API. Avoid `friend` — if `friend` feels right, the type probably needs to be split or the member made `public`.

## struct vs class

Use `struct` for **passive data** — types with public fields and no invariants to maintain, like `vec3`, `vertex`, `render_config`, `scenario_params`. Use `class` for types with encapsulated state, private members, or invariants. The language-level difference is only the default access (`struct` defaults to public, `class` to private), but the keyword itself signals intent at the declaration.

```cpp
// GOOD
struct vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class shader_program
{
public:
    shader_program(const std::string& vert_src, const std::string& frag_src);
    ~shader_program();
    void bind() const;

private:
    GLuint program = 0;
};
```

## Defensive Programming

Do not write defensive code by default. Let exceptions surface — they reveal real problems. Avoid unnecessary null checks. Reserve defensive patterns for critical core systems only.

## Single Source of Truth

When two pieces of code compute the same value, they will drift. The author of one will not remember to update the other. Always extract shared logic to one place; everything else delegates.

Most common case: function overloads. One overload is the canonical implementation; the others are thin wrappers that forward arguments to it. Never write the same calculation in two overloads.

```cpp
// BAD — both overloads independently re-derive the same math
glm::vec3 to_world(glm::ivec2 cell, glm::ivec2 offset)
{
    auto x = start_x + (cell.x - center_x) * space_x + offset.x * offset_x;
    return { x, start_y, ... };
}

glm::vec3 to_world(float grid_x, float grid_y)
{
    auto x = start_x + (grid_x - center_x) * space_x;
    return { x, start_y, ... };
}

// GOOD — float overload owns the math; int overload is a thin wrapper
glm::vec3 to_world(glm::ivec2 cell, glm::ivec2 offset = {})
{
    return to_world((float)cell.x, (float)cell.y, offset);
}

glm::vec3 to_world(float grid_x, float grid_y, glm::ivec2 offset = {})
{
    // canonical formula here, including offset
}
```

## Intermediate Variables

When a calculation has more than one conceptual step, give each step a named local. The local's name is the explanation; long inline expressions force the reader to parse arithmetic to recover meaning.

```cpp
// BAD
auto pos_z = grid_start_position.z - ((grid_height - 1) - grid_y) * space_modifier.y + offset.y * offset_space_modifier.y;

// GOOD
auto rows_from_top = (grid_height - 1) - grid_y;
auto row_offset = offset.y * offset_space_modifier.y;
auto pos_z = grid_start_position.z - rows_from_top * space_modifier.y + row_offset;
```

The bar is "does naming this step explain something the bare arithmetic doesn't." Don't introduce a local for every sub-expression — single multiplications or additions don't qualify; multi-step formulas do.

## Existing Code

When working in an existing file, match its style: comment frequency, blank line usage, naming conventions, etc. Do not rewrite or reformat code that isn't broken or directly related to the task.

This applies doubly to `src/fluid_physics/` — read-only, do not edit at all.

## Identifiers and Keys

Never use string literals as identifiers, keys, or type names. Always use `enum class`.

String keys require memorizing exact spellings and offer no compiler safety — that is bad engineering. `enum class` values are scoped, refactorable, discoverable, and typo-proof.

```cpp
// BAD
flow_manager.get_flow("level_fail");
flow_manager.register_flow("menu", std::make_unique<menu_flow>());

// GOOD
flow_manager.get_flow(flow_type::level_fail);
flow_manager.register_flow(std::make_unique<menu_flow>()); // flow declares its own flow_type
```

This applies to flow types (`flow_type`), view types (`view_type`), shader/material kinds, pool types, and any future registry or lookup system. Use `enum class`, never plain `enum` — plain `enum` leaks values into the surrounding scope and implicitly converts to `int`.

## Namespaces and Using Directives

Never write fully qualified type names inline. Always add a `using` declaration (or namespace alias) at the top of the file and use the short name.

Inline fully qualified names are unreadable and make the code look broken. If a type name conflicts, alias it — do not inline the namespace.

```cpp
// BAD
auto* menu = dynamic_cast<screwed::ui::views::menu::menu_view*>(ui.get_view(view_type::menu));

// GOOD
using screwed::ui::views::menu::menu_view;
// ...
auto* menu = dynamic_cast<menu_view*>(ui.get_view(view_type::menu));
```

**Never `using namespace` in a header** — it pollutes every translation unit that includes the header. In `.cpp` files, prefer narrow `using` declarations (a single type or function) over `using namespace`. Never `using namespace std;` anywhere.

## Loops

**Default to traditional index loops.**

```cpp
for (size_t i = 0; i < vertices.size(); ++i)
{
    // ...
}
```

Use `size_t` for the index when iterating containers — `vec.size()` returns `size_t`, and `int` triggers signed/unsigned comparison warnings.

**Range-for loops are permitted** when iterating an entire collection without needing the index, and when the form genuinely reads cleaner. They are heavily used in modern C++ and the standard library — you will need to read them fluently — but you don't need to reach for them by default.

```cpp
// Permitted, when index isn't needed
for (auto& vertex : vertices)
{
    process(vertex);
}
```

If you do use range-for, prefer `const auto&` for read-only iteration to avoid silent expensive copies of large objects.

## Lambdas

Capture lists must be **explicit**. Spell out which surrounding variables the lambda depends on, and whether each is captured by value or reference.

```cpp
// GOOD — clear what the lambda captures and how
on_loaded += [this, &cache] { reload(cache); };

// BAD — capture-all hides the dependencies
on_loaded += [&] { reload(cache); };       // implicitly grabs `this`, `cache`, anything else in scope
```

Why it matters: a lambda can outlive the scope that created it (e.g., when stored as a callback). If it captured a reference to a local that's now out of scope, you have a dangling reference and a hidden crash. Explicit captures make lifetime obvious at the declaration.

For very short throwaway lambdas passed directly to a `std::sort` comparator or `std::for_each`, `[&]` is acceptable — but the moment a lambda is **stored** or **returned**, write the capture list explicitly.

## Type Conversions

C-style casts are the project default for numeric and related-type conversions.

```cpp
// GOOD
int x = (int)some_float;
float ratio = (float)width / (float)height;
size_t count = (size_t)signed_count;
```

Three named casts have **no C-style equivalent** and must still be used when their semantics matter:

- **`dynamic_cast<derived*>(base_ptr)`** — runtime-checked downcast in a polymorphic class hierarchy. Returns `nullptr` if the object isn't actually that type. The C++ equivalent of C# `as`. Requires the type to have at least one virtual function.

  ```cpp
  if (auto* button = dynamic_cast<menu_button*>(view))
  {
      button->click();
  }
  ```

- **`reinterpret_cast<T*>(p)`** — raw bit-level reinterpretation. Use for low-level work like serialization, GL/Vulkan handle conversions, or pointer-to-integer crossings. Stands out at code review, which is the point.

- **`const_cast<T&>(x)`** — strip or add `const`. Almost always a code smell — reserve it for interfacing with old APIs that lack const-correctness.

If a conversion is anything more interesting than a numeric truncation or a related-type cast, reach for the named cast. C-style cannot express runtime polymorphic checks, bit reinterpretation, or const stripping with any clarity.

## Async and Threads

Do not manufacture async where plain callbacks work. If you are reacting to a single event, store and call the callback directly — no `std::async`, no `std::future`, no `std::promise`, no flag-plus-condition-variable patterns just to wait.

Never use `std::promise` to bridge a callback into a future when a callback alone would do. Never wrap waits in RAII just to manage subscriptions.

Only reach for `std::thread` / `std::async` / `std::future` when the work genuinely needs to run off the main thread (e.g. asset loading, file I/O, an offline bake). If cancellation is expected, propagate a stop token (or equivalent flag) explicitly.

```cpp
// BAD — manufacturing async from a callback
std::promise<void> retry_ready;
view.on_retry_requested += [&] { retry_ready.set_value(); };
retry_ready.get_future().wait();

// GOOD — plain callback subscription
void register_retry()
{
    view.on_retry_requested += [this] { on_retry(); };
}

void on_retry()
{
    view.on_retry_requested -= /* this handler */;
    // handle the event
}
```

## Singletons

Never cache singleton instances in local variables. Call `instance()` directly at the point of use.

Caching buys nothing — `instance()` is a static reference read, not an expensive lookup. Caching it adds a variable that implies the reference is reused in a meaningful way, which is misleading.

```cpp
// BAD
auto& fm = flow_manager::instance();
fm.register_flow(std::make_unique<loading_flow>());
fm.register_flow(std::make_unique<gameplay_flow>());

// GOOD
flow_manager::instance().register_flow(std::make_unique<loading_flow>());
flow_manager::instance().register_flow(std::make_unique<gameplay_flow>());
```

## File Layout

Each type lives in a header (`.hpp`) and source (`.cpp`) pair. File names match the primary type they declare, in snake_case: `shader_program.hpp` declares `shader_program`. Headers declare; sources define. Keep inline definitions in headers only when they are short, hot, or templated.

When a header contains more than one type, order them: enums first, then structs, then classes. Smaller, simpler types come before larger, more complex ones.

If multiple types of the same kind exist, group them by context. If context is equal, sort from shortest to longest.

```cpp
#pragma once

enum class out_of_lives_result
{
    got_lives,
    dismissed,
};

struct heart_cost { ... };

class out_of_lives_flow : public base_flow { ... };
```

In source files, include the matching `.hpp` first (a blank line apart from other includes — this verifies the header is self-sufficient). Match the existing project's grouping convention for the remaining includes.

## Object Lifetime and Resources

Use **RAII** for every resource. The constructor acquires; the destructor releases. **Never call `new` / `delete` directly** in user code.

### Smart pointers

Choose ownership intentionally — the smart pointer type is documentation for who owns the object.

- **`std::unique_ptr<T>`** — single owner. Default for owned heap objects. No copying allowed; transfer ownership with `std::move`.
- **`std::shared_ptr<T>`** — shared ownership through reference counting. Use only when ownership is *genuinely* shared between unrelated systems. Carries a small atomic counter; cycles silently leak (use `std::weak_ptr` to break them).
- **Raw `T*` (or `T&`)** — non-owning observation. "I just look at this; someone else owns it."

```cpp
// GOOD — clear single ownership
auto shader = std::make_unique<shader_program>(vert_src, frag_src);

// GOOD — clear shared ownership
auto cached_texture = std::make_shared<texture_2d>(load("assets/wood.png"));
material_a.set_texture(cached_texture);
material_b.set_texture(cached_texture);

// GOOD — non-owning observer
void render(const mesh* m);   // pointer when null is valid
void render(const mesh& m);   // reference when not
```

The `unique_ptr` vs `shared_ptr` decision is made **per case**, not by blanket policy. Default to `unique_ptr`; only reach for `shared_ptr` when you can identify multiple unrelated owners.

### GPU and OS handles

Wrap every OpenGL handle, GLFW window, file descriptor, and similar OS-managed resource in a small RAII type that releases the handle in its destructor. Never expose raw `GLuint` ownership outside the wrapper.

```cpp
// BAD — manual lifetime, leak risk on early return
GLuint shader = glCreateProgram();
// ...
glDeleteProgram(shader);

// GOOD — RAII wrapper destroys the GL object on scope exit
shader_program shader{ vert_source, frag_source };
// destructor cleans up automatically
```

GPU-resource wrappers must be **non-copyable** (`= delete` the copy constructor and copy assignment). Make them move-only when transferring ownership is meaningful; make them non-movable when the type represents a single fixed instance (the `graphics_engine` in `src/graphics_engine.hpp` is non-copyable and non-movable for this reason).

## Shaders (GLSL)

Use **Hungarian-style stage prefixes** for cross-stage shader identifiers:

- **`a_`** — vertex attributes (inputs from a vertex buffer): `a_position`, `a_tex_coord`, `a_normal`, `a_color`.
- **`v_`** — varyings (vertex shader `out` / fragment shader `in`): `v_tex_coord`, `v_world_pos`, `v_color`.
- **`u_`** — uniforms: `u_field`, `u_range_min`, `u_model`, `u_time`.
- **`frag_color`** — the fragment shader's color output.

The rest of the name follows snake_case: `a_tex_coord`, not `aTexCoord` or `a_TexCoord`. Built-in identifiers like `gl_Position` and `gl_FragCoord` are not user-defined and keep their spec form.

Function-local variables in GLSL follow the same rules as C++ locals: plain snake_case, no stage prefixes, no single-letter names except integer loop indices.

```glsl
// vertex shader
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_tex_coord;
out vec2 v_tex_coord;

uniform mat4 u_view_projection;

void main()
{
    gl_Position = u_view_projection * vec4(a_position, 0.0, 1.0);
    v_tex_coord = a_tex_coord;
}

// fragment shader
in vec2 v_tex_coord;
out vec4 frag_color;

uniform sampler2D u_field;

void main()
{
    frag_color = texture(u_field, v_tex_coord);
}
```

Why prefixes here but not for C++ members? In C++, `m_` says "this is a member" — already obvious from class context. In GLSL, the prefix carries actual semantic information about data flow (where the value comes from), and the `in` / `out` / `uniform` qualifiers can be easy to overlook in long shaders. The prefix is a useful redundancy.

Standard formatting rules (4-space indentation, Allman braces, no alignment padding, no magic numbers, comments only for non-obvious intent) apply to GLSL the same as to C++.

## Convention Defaults

If a convention is not defined in these rules, choose the **most common and popular** one for the language or context. Look to: the dominant tutorials, the most widely-used books, major open-source projects in the space, or the language's standard library and canonical examples.

Avoid one-off hybrids that don't match any established style — they confuse readers who already know the field, and they're hard to justify in code review.

When in doubt, ask.

## General Mindset

Good code is readable code. Prefer better naming and structure over compensating with comments. Change only what needs to change.
