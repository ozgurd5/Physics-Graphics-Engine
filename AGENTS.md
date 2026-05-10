# Coding Standards

This project's **C++ graphics code** follows these rules. Apply them at all times.

The **C fluid simulation under `src/fluid_graphics/`** is owned by a separate developer and is **read-only** — these rules do not govern that code, and it must not be modified, reformatted, or refactored. Read it only for understanding.

## Formatting

- **Braces on new line** (Allman style) — always, no exceptions.
- **No alignment spacing.** Never pad operators with extra spaces to align values across lines.

```cpp
// BAD
constexpr const char* Loading   = "Loading";
constexpr const char* LevelFail = "LevelFail";

// GOOD
constexpr const char* Loading = "Loading";
constexpr const char* LevelFail = "LevelFail";
```

- Use `auto` almost everywhere. Explicit types only when the type is genuinely unclear from context.
- No magic numbers. Extract to a named variable or `constexpr` — prefer variable.
- **Max one blank line** between members. No consecutive blank lines inside functions or blocks.
- **No `#pragma region` blocks.** If you feel the need for one, the class probably needs to be split instead.
- **Enums always use one value per line** with a trailing comma. Never write an enum on a single line. Enums grow — one-liners break down the moment a second developer adds a value or a reader wants to scan the options.

```cpp
// BAD
enum class OutOfLivesResult { GotLives, Dismissed };

// GOOD
enum class OutOfLivesResult
{
    GotLives,
    Dismissed,
};
```

## Comments

Comments exist only for code with **high cognitive load** or **non-obvious intent**. Good naming and structure should carry the weight. Do not narrate what the code does.

```cpp
// BAD — states the obvious
// Close the view when the level loads
levelManager.OnLevelLoaded += [this] { Close(); };

// GOOD — explains why, not what
// Sequence must complete before input is re-enabled
canvas.SetInteractable(false);
```

**Never write Doxygen documentation comments** — `/** */` blocks, `///` lines, `@brief`, `@param`, `@return`, `@see`, or any structured doc comment. Not on public functions, not on private helpers, not anywhere. They restate what a well-named function already says, inflate the file, and discourage the real fix: improving the name.

```cpp
// BAD — Doxygen comment narrates the obvious and bloats the header
/// @brief The next feature the player is progressing toward, or null if all are unlocked.
FeatureUnlockData* GetNextLockedFeature();

// GOOD — the function name carries the meaning
FeatureUnlockData* GetNextLockedFeature();
```

If the name cannot carry the meaning, rename the function or split it. Do not fall back to Doxygen.

## Naming

- **Private members use plain camelCase** — no `_` prefix or suffix, no `m_` prefix. Leave third-party or pre-existing code as-is (e.g. the renderer in `src/graphics_engine.hpp` uses `m_*` — that style is grandfathered, do not reformat it).
- Always check `std::function` (or any other callable that may be empty) before invoking. Use `if (callback) callback(...)` — never call a possibly-empty callable.
- **Names must be short *and* understandable — both, not one.** A name is short enough when it carries no filler and long enough when a reader does not have to guess what it refers to. Never collapse a meaningful noun to one or two letters. `f` for `feature`, `fud` for `featureUnlockData`, `lm` for `levelManager`: none of these save the human anything, and the computer does not care. Type out the word.

```cpp
// BAD
bool IsUnlocked(const FeatureUnlockData& f) { ... }
for (const auto& f : features) { ... }

// GOOD
bool IsUnlocked(const FeatureUnlockData& feature) { ... }
for (const auto& feature : features) { ... }
```

The only tolerated single-letter names are integer loop indices in nested numeric iteration (`for (int i = 0; ...)`, `for (int j = 0; ...)`) where there is no domain object to name. Everything else spells out the word.

## Visibility

Default to `private`. Promote a member to `protected` only when there is a real subclass relationship that uses it; promote to `public` only for genuine cross-class API. Avoid `friend` — if `friend` feels right, the type probably needs to be split or the member made `public`.

## Defensive Programming

Do not write defensive code by default. Let exceptions surface — they reveal real problems. Avoid unnecessary null checks. Reserve defensive patterns for critical core systems only.

## Single Source of Truth

When two pieces of code compute the same value, they will drift. The author of one will not remember to update the other. Always extract shared logic to one place; everything else delegates.

Most common case: function overloads. One overload is the canonical implementation; the others are thin wrappers that forward arguments to it. Never write the same calculation in two overloads.

```cpp
// BAD — both overloads independently re-derive the same math
glm::vec3 ToWorld(glm::ivec2 cell, glm::ivec2 offset)
{
    auto x = startX + (cell.x - centerX) * spaceX + offset.x * offsetX;
    return { x, startY, ... };
}

glm::vec3 ToWorld(float gridX, float gridY)
{
    auto x = startX + (gridX - centerX) * spaceX;
    return { x, startY, ... };
}

// GOOD — float overload owns the math; int overload is a thin wrapper
glm::vec3 ToWorld(glm::ivec2 cell, glm::ivec2 offset = {})
{
    return ToWorld(static_cast<float>(cell.x), static_cast<float>(cell.y), offset);
}

glm::vec3 ToWorld(float gridX, float gridY, glm::ivec2 offset = {})
{
    // canonical formula here, including offset
}
```

## Intermediate Variables

When a calculation has more than one conceptual step, give each step a named local. The local's name is the explanation; long inline expressions force the reader to parse arithmetic to recover meaning.

```cpp
// BAD
auto posZ = gridStartPosition.z - ((gridHeight - 1) - gridY) * spaceModifier.y + offset.y * offsetSpaceModifier.y;

// GOOD
auto rowsFromTop = (gridHeight - 1) - gridY;
auto rowOffset = offset.y * offsetSpaceModifier.y;
auto posZ = gridStartPosition.z - rowsFromTop * spaceModifier.y + rowOffset;
```

The bar is "does naming this step explain something the bare arithmetic doesn't." Don't introduce a local for every sub-expression — single multiplications or additions don't qualify; multi-step formulas do.

## Existing Code

When working in an existing file, match its style: comment frequency, blank line usage, naming conventions, use of `auto`, etc. Do not rewrite or reformat code that isn't broken or directly related to the task. This applies doubly to anything in `src/fluid_graphics/`, which is read-only.

## Identifiers and Keys

Never use string literals as identifiers, keys, or type names. Always use `enum class`.

String keys require memorizing exact spellings and offer no compiler safety — that is bad engineering. `enum class` values are scoped, refactorable, discoverable, and typo-proof.

```cpp
// BAD
flowManager.GetFlow("LevelFail");
flowManager.RegisterFlow("Menu", std::make_unique<MenuFlow>());

// GOOD
flowManager.GetFlow(FlowType::LevelFail);
flowManager.RegisterFlow(std::make_unique<MenuFlow>()); // flow declares its own FlowType
```

This applies to flow types (`FlowType`), view types (`ViewType`), shader/material kinds, pool types, and any future registry or lookup system. Use `enum class`, never plain `enum` — plain `enum` leaks values into the surrounding scope and implicitly converts to `int`.

## Namespaces and Using Directives

Never write fully qualified type names inline. Always add a `using` declaration (or namespace alias) at the top of the file and use the short name.

Inline fully qualified names are unreadable and make the code look broken. If a type name conflicts, alias it — do not inline the namespace.

```cpp
// BAD
auto* menuView = dynamic_cast<screwed::ui::views::menu::MenuView*>(ui.GetView(ViewType::Menu));

// GOOD
using screwed::ui::views::menu::MenuView;
// ...
auto* menuView = dynamic_cast<MenuView*>(ui.GetView(ViewType::Menu));
```

**Never `using namespace` in a header** — it pollutes every translation unit that includes the header. In `.cpp` files, prefer narrow `using` declarations (a single type or function) over `using namespace`. Never `using namespace std;` anywhere.

## Async and Threads

Do not manufacture async where plain callbacks work. If you are reacting to a single event, store and call the callback directly — no `std::async`, no `std::future`, no `std::promise`, no flag-plus-condition-variable patterns just to wait.

Never use `std::promise` to bridge a callback into a future when a callback alone would do. Never wrap waits in RAII just to manage subscriptions. All of these patterns invent complexity that does not need to exist.

Only reach for `std::thread` / `std::async` / `std::future` when the work genuinely needs to run off the main thread (e.g. asset loading, file I/O, an offline bake). If cancellation is expected, propagate a stop token (or equivalent flag) explicitly.

```cpp
// BAD — manufacturing async from a callback
std::promise<void> retryReady;
view.OnRetryRequested += [&] { retryReady.set_value(); };
retryReady.get_future().wait();

// GOOD — plain callback subscription
void RegisterRetry()
{
    view.OnRetryRequested += [this] { OnRetry(); };
}

void OnRetry()
{
    view.OnRetryRequested -= /* this handler */;
    // handle the event
}
```

## Singletons

Never cache singleton instances in local variables. Call `Instance()` directly at the point of use.

Caching buys nothing — `Instance()` is a static reference read, not an expensive lookup. Caching it adds a variable that implies the reference is reused in a meaningful way, which is misleading.

```cpp
// BAD
auto& fm = FlowManager::Instance();
fm.RegisterFlow(std::make_unique<LoadingFlow>());
fm.RegisterFlow(std::make_unique<GameplayFlow>());

// GOOD
FlowManager::Instance().RegisterFlow(std::make_unique<LoadingFlow>());
FlowManager::Instance().RegisterFlow(std::make_unique<GameplayFlow>());
```

## File Layout

Each type lives in a header (`.hpp`) and source (`.cpp`) pair. Headers declare; sources define. Keep inline definitions in headers only when they are short, hot, or templated.

When a header contains more than one type, order them: enums first, then structs, then classes. Smaller, simpler types come before larger, more complex ones.

If multiple types of the same kind exist, group them by context. If context is equal, sort from shortest to longest.

```cpp
enum class OutOfLivesResult
{
    GotLives,
    Dismissed,
};

struct HeartCost { ... };

class OutOfLivesFlow : public BaseFlow { ... };
```

In source files, include the matching `.hpp` first (a blank line apart from other includes — this verifies the header is self-sufficient). Match the existing project's grouping convention for the remaining includes.

## Object Lifetime and Resources

Use **RAII** for every resource. The constructor acquires; the destructor releases. Never call `new`/`delete` directly in user code. Use:

- `std::unique_ptr<T>` for single-owner heap objects.
- `std::shared_ptr<T>` only when ownership is genuinely shared.
- Value semantics by default for small types.
- A dedicated wrapper class for OpenGL/GPU handles (`Shader`, `Texture`, `VertexBuffer`, `Framebuffer`, etc.) that releases the handle in its destructor — never expose raw `GLuint` ownership outside the wrapper.

```cpp
// BAD — manual lifetime, leak risk on early return
GLuint shader = glCreateProgram();
// ...
glDeleteProgram(shader);

// GOOD — RAII wrapper destroys the GL object on scope exit
ShaderProgram shader{vertSource, fragSource};
// ...
// no manual delete needed
```

GPU-resource wrappers must be **non-copyable** (`= delete` the copy constructor and copy assignment). Make them move-only when ownership transfer is meaningful; make them non-movable when the type represents a single fixed instance (the `Renderer` in `src/graphics_engine.hpp` is non-copyable and non-movable for this reason).

## General Mindset

Good code is readable code. Prefer better naming and structure over compensating with comments. Change only what needs to change.
