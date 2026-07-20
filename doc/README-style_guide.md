# Refractor Software C Style and Philosophy Guide

Much of the code written here at Refractor Software is written in C. Our approach
to writing C largely combines the philosophy of Eskil Steenberg with our own
amendments where we think they make sense and thus earn their keep.

---

## 1. Core Philosophy

### Plan for Control

**"In the beginning you always want results, and in the end all you want is control." - Eskil Steenberg**

On any nontrivial project, you start wanting quick results and will quickly find
yourself wanting fine control as the project grows. Thus, don't fear complexity.
Instead of walking into a kitchen and demanding an omlette, ask what *you* can
make inside that kitchen (Steenberg paraphrase). Embrace the complexity you'll
eventually need instead of fighting it.

A great (and very relevant) example is memory. Garbage collection is convenient until
performance matters, at which point the *lack* of control becomes the problem and
manual `free()` becomes the thing you wish for. Manual memory management is a
*solvable* problem precisely because you hold the control needed to solve it.

### Communication with Code

Code is not only instructions to the computer - it is a description, to you, of
what the computer does. If you misunderstand the code, you misunderstand the
program. So:

- Everything should be unambiguous and explicit.
- You *want* the compiler to error loudly rather than guess. More errors means
  more communication and less ambiguity. An error is the compiler catching you
  before the bug ships.
- Cleverness that makes you *feel* smart while doing something subtly wrong is
  the root of more evil than premature optimization.

### Crashes are good

A visible crash with a debugger pointing at the fault is a gift. Silent failure -
a swallowed exception, a gracefully handled error, a log nobody reads, quietly
corrupted data - is the worst outcome, because nothing forces you to fix it. Make
failures loud and immediate. (See §9 on tooling that deliberately makes latent
bugs crash on contact.)

### Small footprint, zero unwrapped dependencies

- Keep your technology footprint as small as possible. The fewer things your code
  requires, the fewer things can break, and the longer it survives.
- Wrap every external dependency behind your own interface. Any company, OS, or
  library can disappear or change a decision you don't control. If you call a raw
  external API in a million places, someone else's bad decision becomes your
  problem everywhere at once. Wrap it once and you keep full control.
- Write code to last decades, not days.

Steenberg targets **C89** for maximum portability across unknown future compilers.
Refractor Software targets **C17**, on the reasoning that it is well past being
"new," for our use cases, is broadly implemented, fixes the defects C99
introduced, and that we ship almost entirely to modern platforms. The portability
philosophy is unchanged - C17 is simply where the "simplest compiler that still
exists everywhere I care about" line is drawn today. Still avoid leaning on exotic
or bleeding-edge features; the goal is a small, stable footprint, not feature
maximalism. Specifically, try not to rely on features that exist only in C17 but not
in C++17 or newer, and vice versa.

### Simple language, powerful tools

A language should be as simple as possible; you then do complex things *with* it
and build great tools on top of it. C has the most mature tooling of any language
precisely *because* it is simple enough to parse. A simple core makes it feasible
to write your own parsers, debuggers, doc generators, and code generators - and
those tools pay back the "extra typing" of an explicit language many times over.

### Build a mountain; build a small house on it

Don't fear writing code, and don't be deterred by "that's too hard." You learn by
*implementing*, not by gluing libraries together. Code you wrote yourself never
ages out from under you, and you are the expert who can fix it.

Think in layers. Each piece of technology is part of a mountain, not an end
product. Build **huge technology** (libraries) and **small applications** on top
of it. An interface library plus a sound library makes the actual music player
trivial - and both are reusable for the next application. Keep the application surface
small and the foundation deep.

### Fix it now

Rewrite code you dislike *immediately*. It will never be easier to change than it
is today, because usage only makes code harder to touch. This keeps technical
debt near zero. Professionally: don't show a flashy 99%-done hack, because then
nobody understands why the last 1% takes months. Show only finished, debugged
work so estimates stay honest, and write everything "for real" from the start.

---

## 2. Declarations and Naming

Wide code is good code. Long, descriptive beat terseness after you've been away for
a while. The hard part is bugs, not typing.

### The module-object-verb pattern

Name symbols like a directory structure: most general to most specific, left to right.

```
module_object_verb ()
```

This groups related functionality, makes names predictable, and means a name
often tells you not just *what* a function does but *which file* it lives in.

- Prefer matched antonyms: `create`/`destroy`, not `create`/`remove`.
  (The opposite of `remove` is `add`; the opposite of `add` is `subtract`. Don't
  mix families.)
- `module_object_create ()` reads regularly and sorts/groups cleanly;
  `create_object ()` does not.

The `module_object` skeleton is the default, but the full three-part form is not
mandatory everywhere:

- Use the module prefix for subsystem-like features.
- Omit it for foundational code (e.g. math libraries), where a bare
  `object_verb ()` or even `verb ()` reads better.
- Let them blend when the module *is* the main object - e.g. a `camera` module
  whose primary object is also a `camera`, giving a clean `camera_* ()` surface.
- Mix `module_object_verb ()` with `module_verb ()` and `object_verb ()` as the
  situation needs, rather than forcing every name into the maximal form.

When the verb is the salient thing and the object is effectively the variant
being selected, put the verb first and fold the type into the tail:

```c
simd_add_f32vec4 ()
simd_add_s32vec4 ()
```

This keeps related operations (`add`) adjacent while still encoding the operand
type unambiguously in the name - manual, explicit overloading without the
compiler guessing for you.

### Reuse names religiously

Pick names and stick to them everywhere. Familiarity is the payoff: code you
wrote long ago reads instantly because nothing is a surprise.

- `i`, `j`, `k` are always integer loop counters.
- A short float temporary is always `f` (and `f2`, `f3` for more) - never reused
  as anything else, so it can never be accidentally treated as an integer.
- Words like `count`, `length`, `found`, `next`, `previous`, `array` always mean
  the same thing.
- Postfix `_func` on things used as function pointers; `_internal` on things
  private to a module.

### Casing conventions

| Kind | Convention | Example |
|------|------------|---------|
| Macros / defines | `UPPER_WITH_UNDERSCORES` | `MAX_ENTITIES` |
| Types | `lower_with_underscores` | `render_graph`, `f32vec3` |
| Functions | `lower_with_underscores ()` | `camera_view_set ()` |
| Variables | `camelCase` | `someVariable`, `entityCount` |

- **Space** between the function name and the opening parenthesis:
  `camera_view_set (...)`, never `camera_view_set(...)`.

### Declaration and Definition

When declaring a function (header or elsewhere):

```c
/**
 * Documentation comment if necessary.
 *
 * @param param1 Description
 * @param param2 Description
 * @param param3 Description
 * @param param4 Description
 *
 * @return Description
 *
 * @see Anything else, if necessary, omit otherwise
 */
return_type function (param1, param2,
    param3, param4, ...);

// Wrapping determined by when you hit the wrap limit, so it may be more or less
// than 2 parameters, don't take this contrived-for-example number of params before
// wrapping as gospel
```

When defining a function (translation unit or elsewhere):

```c
return_type
function (param1, param2,
    param3, param4, ...)
{
    // Function body
}

// Again, wrapping determined by when you hit the wrap limit, so it may be more or less
// than 2 parameters, don't take this contrived-for-example number of params before
// wrapping as gospel
```

Take note of the usage of line breaks. This helps with searching for definitions (can grep `^function`)
and I think it looks better too.

### Spacing matters (for searchability, not just looks)

Consistency here is a hard rule because it makes `grep` reliable:

- Always write the call form the same way (`func (` consistently), so searching
  for a function name plus ` (` finds every call and nothing else. A missing space
  in one call site hides it from your search.
- Put spaces around assignment (`a = b`), so searching `a =` doesn't also match
  `data =`. It isn't waterproof (`+=` won't match), but it's strictly better.
- Mixing two spacing styles in the same codebase is the unforgivable sin. Pick
  one and never deviate.

---

## 3. Types and Primitives

Use explicit-width types as the default vocabulary:

| Category | Types |
|----------|-------|
| Unsigned integers | `u8`, `u16`, `u32`, `u64` |
| Signed integers | `s8`, `s16`, `s32`, `s64` |
| Floats | `f32`, `f64` |
| Boolean | `b8` (a `typedef` over `u8`) |

Conventions:

- **Default to unsigned.** Use signed types only when a value can genuinely be
  negative.
- **Vectors** append `vecN` to the base type: `f32vec3`, `s32vec4`.
- **Matrices** append `matNxM`: `f32mat3x2`, and `f32mat4x4` even when `N == M`
  (always spell out both dimensions - no `mat4` shorthand).

### `sizeof` is an operator, not a function

Think of `sizeof` like a cast, not a call.

Prefer sizing from the *variable* rather than spelling out the type:

```c
/* Fragile: silently wrong if a's type changes. */
a = malloc (sizeof (float) * 10);

/* Robust: follows a's type automatically. */
a = malloc (sizeof (*a) * 10);
```

If you later change `a`'s type, the second form's allocation tracks it; the
literal-type form silently under- or over-allocates.

---

## 4. Booleans and State

Booleans are fine as stack-local temporaries and return values, but storing them
in structures is discouraged. Prefer, in order:

1. **Split data by state** (data-oriented design, after Mike Acton): keep things
   in separate arrays/structures by which state they're in, rather than carrying
   a flag on each element and branching.
2. **If logical state must be stored, use bitfields**, typed with the boolean
   type (which is just a `typedef` over `u8`):

   ```c
   b8 isVisible : 1;
   b8 isDirty   : 1;
   ```

This keeps structures small (see §7 on why memory size dominates performance) and
avoids scattering branch-driving flags through hot data.

This also connects to a broader rule: **don't store derived data twice.** Storing
`width`, `length`, *and* `area` invites the three from drifting out of sync, with
no canonical truth. Recompute `width * length` - it's always consistent, and math
is cheaper than the extra memory access anyway. When you genuinely must cache an
expensive result, hide it behind an opaque handle (§5) so setters can keep the
cache consistent - "hardcoded to never bug."

---

## 5. Opaque Types, Handles, and "Objects" in C

Object orientation as "code + data bundled together" is a fiction: CPUs keep code
and data separate (executable memory is read/execute; data is read/write). So
model objects as **a handle plus functions that operate on it**:

```c
thing * t = thing_create ();
thing_do_something (t);
thing_destroy (t);
```

Benefits: it's explicit (a function operating on data, not an object "calling"
itself), and a function can take *two* handles symmetrically rather than one
object privileging another.

### How to make the handle opaque

The user should be able to hold the handle and pass it back to your API, but not
read or write its internals - so you can reimplement the internals freely without
touching any calling code.

There are a few ways to do this, and it depends on what you want your code to do.
The first form is using an opaque pointer, for which there are two main forms:

#### Steenberg's form - `typedef void` for the public handle:

```c
/* Public header. Note: void, NOT void*. */
typedef void thing;

thing * thing_create (void);
void    thing_do_something (thing *t);
```

```c
/* Implementation file only. */
typedef struct {
    /* real fields */
} thing_internal;
```

The `typedef void Thing` (not `typedef void* Thing`) detail is deliberate: the
pointer stays visible in every signature (`thing *`), which keeps call sites
honest and supports the shared-header polymorphism trick (§6).

#### Forward-declared incomplete type:

```c
/* Public header. */
typedef struct thing thing;

thing * thing_create (void);
void    thing_do_something (thing *t);
```

```c
/* Implementation file. */
struct thing {
    /* real fields */
};
```

Rationale: a distinct incomplete type means the compiler rejects passing the
wrong handle type to a function - `thing *` and `widget *` are not interchangeable,
whereas everything as a `void *` allow mistakes. You get the same opacity *plus*
type safety at the boundary.

**When to use which:** Use the incomplete-struct form by default for the type
checking. Fall back to Steenberg's `typedef void` form when you specifically want
the polymorphism that void handles enable - i.e. when you expect to cast a family
of related structs through a shared leading header (§6). Reach for it
deliberately, not by default.

#### Integer handles - wrap `u64` or another combination of integers:

```c
/* Public header. */
typedef struct thing_handle { u64 _id; } thing_handle;

thing_handle thing_create (void);
void         thing_do_something (thing_handle t);
```

```c
/* Implementation file. */
struct thing {
    /* real fields */
};

/* Then the library looks up the thing and does whatever with it internally. */
```

Integer handles are great for many cases and many reasons. In particular, they are
resistant to the object being deleted (so they're good to save in a struct somewhere
and forget about it, because checking the handle after it's been recycled will safely
return an invalid value or no-op), as long as the handle in question is serial/generational,
which in our code it basically always is. Basically, this pattern is far more resistant
to the "dangling pointer" issue.

The tradeoff is that it requires the library to hold onto the resource internally somehow,
either through its own static data or through some kind of context object or memory block, so
it depends on whether that's something your library is doing. If your library's job is to
allocate something and spit that allocated memory out, and let the client program be in
full control of that thing's lifetime and capabilites, then an integer handle is probably
overkill or detrimental.

---

## 6. Structs, Memory Layout, and Inheritance

A struct is just **offsets plus a size**; field names vanish at compile time.

### Shared-header "inheritance"

Because the first member of a struct sits at offset zero, a pointer to the struct
*is* a pointer to its first member. Put a common header first in every related
struct, and a `header *` can be cast to the right concrete type after inspecting
`header.type`:

```c
typedef struct entity_header {
    u32 type;
    /* shared fields: position, ids, next/previous, ... */
} entity_header;

struct entity_block { entity_header head; /* block fields */ };
struct entity_actor { entity_header head; /* actor fields */ };
```

A function takes `entity_header *`, reads `head.type`, and casts to the specific
struct to reach its unique fields. This is the case where the `typedef void`
handle form (§5) shines, since it expects exactly this kind of casting.

### Alignment and padding

Members want naturally aligned addresses, so the compiler inserts padding:

- `struct { u8 a; u32 b; }` is **8 bytes, not 5** - `b` wants a 4-aligned slot.
- `sizeof` always reports a size that tiles cleanly into an array, so padding
  persists even after you reorder fields.

Use this knowledge two ways:

1. **Tuck small fields into padding for free** - three `u8`s can share the gap
   before a `u32` at no size cost.
2. **Reorder fields largest-to-smallest to shrink structs** - a poorly ordered
   `u8 / u32 / u8` struct can be 12 bytes; reordered, 8.

Never deliberately misalign data (e.g. writing a `u32` through a `u8 *` at an odd
offset). On some hardware it crashes or is dramatically slow because the OS must
fix it up.

---

## 7. Memory Is the Whole Game

### Pointers are addresses; types are step sizes

Memory is one giant numbered array of bytes; a pointer is a house number. The
pointer's *type* is how big each house is, so `p + 1` advances by `sizeof (*p)`. A
pointer to one thing is implicitly a pointer to an array (the next house, and the
next). `x [3]` means "step 3 houses forward, read" - which is why indexing starts
at 0 ("stay put").

### Memory is slow; math is nearly free

Approximate costs:

| Location | Cost |
|----------|------|
| Register | ~0 cycles |
| L1 cache | ~2–3 cycles |
| L2 cache | ~10–15 cycles |
| Main memory | up to ~200 cycles |

A SIMD core can do four or even eight multiplies per cycle, so a single 200-cycle memory
fetch can cost the equivalent of ~800-1600 multiplies. **Optimization is mostly: touch
less memory, and keep what you touch in cache.** Prefer recomputing over storing;
prefer small, packed structures.

### Arrays beat linked lists

Linked-list nodes scatter across memory (cache misses) and waste space on `next`
pointers. A growable array packs elements adjacently - prefetched together, faster
to traverse.

- Grow in chunks (e.g. +16) or by doubling so reallocation is rare. Even
  doubling never uses more than ~2× memory, no worse than a linked list's pointer
  overhead.
- **Fast unordered removal:** swap the last element into the removed slot,
  decrement the count.
- **Ordered removal:** walk backward from the end; you're already scanning
  in-cache to find the element, so shift as you go. Still beats a linked list,
  since you usually have to *find* the element first anyway.

### `realloc` is good, not bad

Virtual memory hands out ~4 KB blocks and remaps them, so a large `realloc` often
only needs to fix the tail rather than copy everything. Combined with chunked
growth, `realloc` runs rarely and gives you contiguous, cache-friendly storage.

### Advanced allocation tricks

- **Flexible array member (C11):** put the count and a trailing `data[]` in one
  struct and allocate `sizeof (struct) + n * sizeof (element)` in a single call -
  one allocation, contiguous, and reading the count likely prefetches the array.
  The flexible member must be last.
- **Co-allocating two structs** in one block (`malloc (sizeof (a) + sizeof (b))`,
  then casting past `a` to a `b *`) gives adjacency, but is dangerous: `a`'s size
  may not satisfy `b`'s alignment. Only with eyes open.

### Stride

Don't assume tight packing. Pass **pointer + count + stride** (bytes to advance
per element) instead of just pointer + count. The same routine then works on
tightly packed data, on interleaved data (e.g. a color field inside a larger
struct), or on padded buffers - no copying into a temporary. Stride makes
functions dramatically more versatile; use it.

---

## 8. Functions and Control Flow

### Long functions are good

Sequential code reads like a book, top to bottom. Splitting logic across many tiny
functions is like a Choose-Your-Own-Adventure - you lose the thread. A long,
linear function lets you scroll and always know the exact state (e.g. whether
blending is currently enabled) at any point.

A codebase full of `Manager`, `Controller`, and `Handler` is a warning sign: it
*handles* other code rather than *doing* something. Sometimes these are necessary
or useful for complex systems (*especially* in game engines), but in most cases
they aren't. Prefer functions that clearly do a concrete thing.

(Reference point: a fly-by-wire control system structured as one main loop calling
~100 leaf functions that call nothing further - one level of interaction, nothing
hidden - was found bug-free after decades.)

### API design: outside-in

Design the interface you *want* first, then fill in the implementation behind it.
A good interface lets you swap implementations without touching callers - exactly
what you need for code meant to live a long time.

### Use the Simplest Types

In any API, try to use the simplest and most primitive possible types that are
sensible for what that API is doing. Obviously there will be functions that take
a concrete object (like taking a `render_graph *`), but for example you may want
to consider taking `f32 *xyz` or `f32 x, f32 y, f32 z` instead of, or better yet
*alongside*, taking/requiring `f32vec3`. This can be quite nice in some cases,
compared to needing to convert between types (e.g., calling a function that just
needs `x` and `y`, but all you have is an `f32vec3`), but as stated it's not always
practical, and/or offering a mix is most useful.

---

## 9. Macros and Tooling

Macros carry a whiff of C++ danger; use them sparingly. Two patterns earn their
place:

### Type-combinatorial code generation

When you genuinely need the same algorithm across many type combinations (e.g.
multiply arrays of every integer/float permutation), a macro avoids hand-copying
and the typos that come with it. Accept that debuggers handle macro-expanded code
poorly, and only reach for this when the combination count is genuinely large.

### `__FILE__` / `__LINE__` instrumentation

These two standard macros power excellent debugging tools. (`__func__` exists in
C11/C99 as an identifier; the old `__FUNCTION__` is a non-standard extension -
prefer `__func__` if you want the function name, but treat it accordingly.)

- **Memory debugger:** a macro rewrites `malloc` into
  `debug_malloc(size, __FILE__, __LINE__)`, recording where every allocation and
  free happened, so leaks become a printout. Extend it with a magic-number
  sentinel after each block to catch overruns, and per-allocation comments to
  track specific allocations.
- **Self-describing serialization:** tag each packed field with a name; in debug
  builds, verify the name/type on unpack so an off-by-one in a binary protocol
  produces a precise "on line X you read a float called Y but the data is an int
  called Z" instead of silent corruption.

### Making really fucked-up call sites more useable

Because of limitations, C APIs can be a... little rough to use at times. There
can be cases where macros genuinely make a C API easier to call. The `flecs` library
is full of examples like this, such as:

```c
/* Kind of cheating with a macro, but this is way more ergonomic and causes fewer mistakes
 * than typing out what it expands to manually. */
ECS_COMPONENT(world, component_type);
```

In the example above, `flecs` defines this such that it expands to a single function call,
just making other things *within* the process of calling that function a hell of a lot easier
to deal with.

Just use your best judgement. Generally speaking, any function that would probably
be templated in C++ *might* have a good reason to do something like this, but even
then that's not a catch-all rule (for example, our handling of "overloadable" functions
earlier wouldn't do this).

### Cases involving just a type, and not a bunch of data

A common pattern for us is that where a C++ template would look like this:

```cpp
do_something<some_type>();
```

We might instead do:

```c
do_something(DO_SOMETHING_PARAM(some_type));
```

We do this a lot because we try to not generate a bunch of inline code using macros,
which is what this would probably do if it were a real macro:

```c
DO_SOMETHING(some_type);
```

When using a macro just to do something with a type (e.g., fill out some kind of generic
struct that wants info about a type, or that needs to do repetitive and easy-to-fuck-up
work with something), this is typically a suitable pattern. It's certainly plenty readable
and your debugger probably isn't going to break on it as long as you're not doing anything
incredibly stupid inside one.

Cases where we use this: Basically any time we need info about a type at runtime and we would
use a template in C++ to solve that. It's a common pattern and hasn't caused issues for us.

### Make latent bugs crash on contact

Tools that force every allocation onto its own guarded page (e.g. GFlags on
Windows, equivalents on Linux) turn silent overruns into immediate crashes. They
make the whole system unstable while enabled, so run them periodically on a
dedicated machine rather than always. This is the "crashes are good" principle
operationalized: surface the bug instead of letting it hide.

---

## 10. Files and Organization

- In most cases, expose only **one public header per library** (e.g. `module.h`),
  backed by many `.c` files, so users never wrestle with a pile of includes.
- In some cases, such as interfaces whose libraries are expected to be used together but
  don't make sense as a single API (in case someone wants one and not the other, or if
  they're related but solving different sets of problems), they can be split into multiple
  headers, with one header per major library API.
  - This does NOT mean segmenting a library's API into headers by functionality!
    So, `foo_bar_*()` and `foo_baz_*()` should go in the shared `foo.h`, *not*
    in separate `foo_bar.h` and `foo_baz.h`.
  - Rather this is about multiple APIs contained in one library: for example a
    platform abstraction library exposing a general operating system API `platform/os.h`
    and a graphics device interface API `platform/gdi.h`.
- Keep a separate `*_internal.h` for declarations shared *within* the module but
  hidden from the outside world.
- Place the public header where dependent code can reach it easily; keep the
  implementation files together out of the way.
- Generally speaking, include path should be `module/module.h`, *not* a plain `module.h`,
  in order to avoid lookup/naming issues later if/when a platform or dependency later down
  the line decides to have its own headers with a similar module header name.

---

## 11. Things to Avoid

- **Hidden behavior of any kind.** If it's implicit, it's a future bug.
- **Operator overloading and implicit conversions** that let a reader guess
  wrong. `dot (a, b)` and `mul (a, b)` can't be confused the way `a * b` can. Write
  the explicit suffix (`0.3f`, or `simd_add_f32vec4`) instead of trusting
  overload resolution.
- **Implicit/undeclared things** that fail silently (a stray capital letter
  reading back a default zero). Mandatory declarations and loud errors are a
  feature.
- **Storing derived data twice** (§4) - drift and inconsistency.
- **Linked lists by reflex** (§7) - reach for packed arrays first.
- **Booleans stored in hot structures** (§4) - split by state or use bitfields.
- **`Manager`/`Controller`/`Handler` sprawl** (§8) - write code that does
  something.
- **Showing unfinished hacks as if done** (§1) - it destroys estimate honesty.

---

## Quick Reference Card

| Topic | Rule |
|-------|------|
| Language | C11; small footprint, no exotic features |
| Dependencies | Wrap everything external behind your own interface |
| Primitives | `u8…u64`, `s8…s64`, `f32/f64`, `b8`; default unsigned |
| Vectors / matrices | `f32vec3`, `f32mat4x4` (always `NxM`) |
| Types | `lower_with_underscores` |
| Functions | `lower_with_underscores ()`, space before `(` |
| Variables | `camelCase` |
| Macros | `UPPER_WITH_UNDERSCORES` |
| Naming pattern | `module_object_verb ()`, prefix optional, verb-first when "overloading" |
| Opaque handle | `typedef struct thing thing;` (void form for header polymorphism) |
| `sizeof` | Size from the variable: `sizeof (*a)`, not `sizeof (type)` |
| Booleans | Don't store; split by state or bitfield `b8 x : 1;` |
| Derived data | Recompute, don't cache (unless truly expensive) |
| Containers | Growable arrays + `realloc`, not linked lists |
| Iteration | Pass pointer + count + **stride** |
| Functions | Long and linear is fine; avoid `*Manager`/`*Handler` |
| Errors & crashes | Make them loud and immediate |
| Tooling | `__FILE__`/`__LINE__` instrumentation; guard-page allocators |
| Workflow | Fix it now; build deep libraries, shallow apps; show only finished work |
