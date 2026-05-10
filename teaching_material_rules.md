# Teaching Material Rules

Rules to follow when creating any educational or teaching document. Derived from feedback while writing `teaching_material.md`.

---

## Process

### 1. Plan before writing
Always start with a **table of contents** — headers and subheaders only, no content. Get user approval before writing any actual content. Once approved, write at most **1–3 headers per output**, then stop and ask for questions or approval before continuing.

Do not dump multiple sections at once. Do not start writing without alignment on the structure.

### 2. Revise after structural changes
After renumbering, adding sections, moving content, or replacing a section: do a revision pass on every previous section that might be affected. Specifically look for:

- Forward references that now point to the wrong Part number.
- Terms that now appear before being defined (because earlier content was moved).
- Format inconsistencies introduced by the change.
- Stale code references or examples that no longer apply.

The user should not have to point these out — they should be caught by you proactively after every restructure.

### 3. Cross-references must survive structural changes
Every "covered in Part X" reference is fragile — it can break the moment Parts are renumbered. After every restructure:

- Re-check every "Part N" reference in the document.
- Update broken ones to point to the new Part numbers.
- For references that don't need a specific number, prefer durable phrasing like "covered later" or "covered in a dedicated section." Specific is more useful when it's right; durable is safer when in doubt.

---

## Content principles

### 4. Build foundations before detail
**Define a concept before using it.** If you find yourself using a term that hasn't been introduced (e.g. "the bound IBO" before "binding" and "IBO" are explained), stop. Either introduce the term inline with a brief definition, or move its introduction to an earlier section.

The reader should never need outside knowledge for the document to make sense. If a term needs prior knowledge, that knowledge belongs earlier in this document.

### 5. Minimize use of future concepts
Even when foundations come first, **do not lean on future concepts within a section**. Every reference to something not-yet-introduced is a tax on the reader — they have to hold a placeholder for a concept they don't fully understand yet.

Use future concepts only when:

- They are genuinely needed for the section to make sense.
- The reference is brief — a one-line inline definition or a "covered in Part X" pointer, no more.
- You don't stack multiple of them in the same paragraph or list.

If a section pulls in three or more future concepts at once, that's a signal: the section is in the wrong place, or it's doing too much. Restructure it. Move it later, simplify it, or strip it down to the level the reader currently has. Don't paper over the gap with a chain of forward references.

When you do reference a future concept, prefer the **descriptive name** over the technical acronym ("vertex buffer" rather than "VBO" before VBOs are introduced). Save the acronym for when its dedicated section establishes it.

### 6. Don't skip topics
If you list something, explain it. If you reference a term, define it. If a stage appears in a diagram, give it a description in the surrounding text.

"Simple" means using plain language. It does **not** mean leaving content out. Simple and detailed are not opposites — both are required at the same time.

### 7. Generalist teaching, not question-driven patches
Do not structure a document around specific questions the user asked. A good generalist explanation answers specific questions **naturally, as a side effect**.

If a user question exposes a missing topic, fix it by improving the relevant general section. Do not add a question-shaped patch (a Q&A block, a "your specific question is answered here" call-out). Generalize the gap.

**The document is for everyone — not just the current conversation.** The reader of the document does not know what was discussed in the chat. Never write phrases like "the question that came up earlier," "as we discussed," "you asked about this," or any wording that assumes the reader knows the conversation context. The document is a self-contained artifact for any reader; the chat is the private channel for the current user. Information that started as a chat question enters the document only after being stripped of "you," "we," and "earlier" — generalized to apply to anyone who picks up the document cold.

### 8. Match level of detail to position
Introduction and foundation sections should be simple maps — high-level shape, minimal detail.

Deep explanations belong in their own dedicated sections later. If a concept is important enough to dwell on, give it its own header — don't bury it as a sub-paragraph of another topic.

A complex 12-stage diagram does not belong in the introductory part. A 6-stage map does. The 12-stage detail belongs in the dedicated Part for those stages.

### 9. Match section length to importance
A foundational concept that genuinely takes 200 words shouldn't get 800. A complex topic that takes 1000 shouldn't get 200. Uneven length signals uneven thought, and the user notices.

If you find one section much longer than its neighbors, ask: is this section actually that much more important, or have I padded it with examples and asides? If it's the second, trim. If you find a section much shorter than its neighbors, ask: have I skipped topics or treated it superficially?

---

## Format principles

### 10. Keep format consistent across categories
When you establish a format for a category of items (e.g. *Runs / Job / Used for / We use this*), apply it to **every** item in that category — including items that seem awkward to fit.

Reformat the awkward ones to fit the pattern. Do not break the pattern for one item.

### 11. No file-and-line code references
Do not use volatile references like `[main.cpp:42](src/main.cpp:42)` or `Renderer::CompileShader in graphics_engine.cpp`. Codebases change; teaching material should remain stable across edits to the code.

Refer to project components **generically** instead: "our renderer," "the simulation library," "the shader-compile helper." Names and paths can shift; the teaching value should not.

### 12. Diagrams must match the abstraction level of their context
A diagram far more detailed than the surrounding text is overload — the reader gets blasted with information the prose hasn't earned yet. A diagram far simpler than the surrounding text is a tease — it under-represents content the reader is otherwise reading carefully.

When placing a diagram, ask: does this diagram show the same kind and depth of information as the section it lives in? If not, simplify or expand it until it does.

Also: every box, label, or symbol in a diagram must be explained in the surrounding text. Don't show a stage in a diagram and then never describe it.

### 13. Code samples must be realistic, not pseudo-code
When showing code in the document, use real OpenGL / GLSL — real type names, real function signatures, real constants. Don't write `// pseudo: read indices` or invented helper functions like `compileShader(file)`.

Pseudo-code creates an extra layer the reader has to mentally translate to know "what does this look like in real code?" Real code skips that step. The example *is* the explanation.

---

## Audience calibration

### 14. Calibrate to the audience — and respect their stated preferences
The current audience is a developer with engine experience (originally Unity / C#) but new to **low-level graphics APIs**. Adjust:

- Assume general programming and software-engineering knowledge.
- Do **not** assume any graphics-specific background.
- Do **not** make comparisons to Unity or C#. That comparison was explicitly forbidden, even though it might seem natural for relating concepts.
- Use plain general programming concepts and concrete code in C++ when you need to illustrate something.

If the user changes audience or unblocks a comparison later, update this rule and continue.
