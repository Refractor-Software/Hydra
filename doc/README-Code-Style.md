# Refractor Software C/C++/C# Style and Philosophy

At Refractor Software we write a lot of our code in C and C++. C is pretty self-contained but C++ is rather... infamous... for spiraling out of control without a strict guide to back it up. Therefore we have this style and philosophy guide that contributors should read and internalize before working on any Refractor Software C/C++/C# code.

## 1. Commandments

### Plan for Control

A wise man once said:

> In the beginning you always want results, and in the end all you want is control.
>
> - *Eskil Steenberg*

On any nontrivial software engineering project, there will be a great degree of complexity to deal with, and you need *control* over that complexity in order to do anything effective with it. If you're so scared of that complexity that you can't work with it, you are not cut out for this and will be better off spending your time elsewhere. If you can work with it, welcome to the team.

**Example: Memory management.** Garbage collection is convenient until you hit performance bottlenecks due to memory consumption and GC pauses, at which point the *lack of control* over memory becomes the problem, and manually being able to manage it is what you need. Manual memory management is a *harder problem* than an automatic garbage collector, because you *must think harder* about it. Manual memory management is also a *more solvable problem* than an automatic garbage collector, because you *have the control* that you need to solve it.

### Code is Communication

If code was just instructions to the computer, we'd still be writing assembly. But, it is not just that - it's a description, to you and others, of what the computer will do with your program. If you understand the code, you understand the program, and vice versa.

Therefore:

- It should be unambiguous what you are doing.
- Anything non-trivial should be explicit.
- Be pragmatic, not clever.

### Embrace Failure

**Fail fast, fail early, fail often.** Visible errors and crashes that tools (like debuggers) can understand are a divine gift. Swallowed exceptions/errors, quietly corrupted data, and other silent failures, are the worst outcomes - nothing forces you to fix them.

Thus, make important failures *loud and immediate*. It's just software. As long as it's not shipping to customers yet, let it crash; nothing's going to break.

### Small Footprint

Keep the technical footprint as small as possible. The fewer dependencies you have, the fewer things can break, and the longer your code survives.

Wrap all external dependencies behind your own interface. Any company, OS, or library can disappear or make a decision that you can't control. If you call someone else's API directly in a thousand scattered places, someone else's fuck-up becomes everyone else's problem, including yours.

Code should last decades, not days. Don't half-ass solutions to your problems.

### Admire Simplicity

> "A fool admires complexity. A genius admires simplicity."
>
> - *Terry Davis*

Simple languages are great because they are trivial to understand. When you need more, they make it easy to build complex tools on top of them. Why do you think C has the most mature and powerful tooling? It's simple to understand, therefore it's simple to parse, debug, generate documentation for, generate code for, and more things I may not have thought of. This can all be done with tools you can build yourself in the same language.

### Build Houses on Mountains, Not Mountains on Houses

(Roughly paraphrased from Eskil Steenberg again.)

Mountains are made of layers. Build a huge technology library (a mountain) and then build small applications (houses) on top of it.

Consider a media player. By itself it's a daunting task. With a user interface library, audio library, and video library, it's trivial, and those same libraries can be reused for future applications that also need user interfaces, audio, and video.

This is also why you want to ensure your libraries are actually finished before marking them as v1. Build houses on dormant mountains, not active volcanos.

### Fix It Now

Code you dislike should be rewritten immediately. It will never be easier to change a piece of code than it is today, because further usage only makes it harder. This is perhaps the highest-leverage way to keep technical debt down and avoid expensive refactor tractors.

It's also not worth showing off flashy mostly-done hacks, because nobody will understand why the last 5% will take months. Show only finished, debugged work, so estimates stay honest and nobody attempts to build their house on your active volcano.

---

Like the real Ten Commandments (we only have seven, but I digress), there's room for nuance. C# is the most obvious one that breaks some of these rules by its nature - it has a garbage collector, for example - but we should all strive to adhere to these as closely as possible regardless.

## 2. Language

| Language | Standard Version | Runtime Version |
|----------|------------------|-----------------|
| C        | 17               | N/A             |
| C++      | 17               | N/A             |
| C#       | 14               | .NET 10         |

These versions will be updated as new language versions release and are stabilized.

C# is usually kept as up-to-date as possible to avoid heavy upheavals down the line. C/C++ code prioritizes stability much more strongly. We generally wait until compiler support for new language features is stable across the board before upgrading our language version.

For comparison engineers like Eskil Steenberg prefer to target C89 across the board (usually also their own subset, for example Steenberg's "Dependable C"). We target newer language versions due to the fact that we generally ship our software on a much more limited set of hardware. Most of our software is video games that target current-generation consoles and PC; we don't really deal with exotic hardware very often if at all.

## 3. Indentation, Spacing and Line Breaks

### Indentation

Indent with 4 spaces. It's easy enough to read and parse. 2 is too small and 8 is too wide for my taste. Tabs are also inconsistent across editors.

Switch statements should align the `switch` and subordinate `case` labels in the same column:

```cpp
switch ( value )
{
case 0:
case 1:
    DoSomething();
    break;
case 2:
case 3:
    DoSomethingElse();
    break;
default:
    DoDefaultThing();
}
```

### Line Breaks

The preferred limit of a single line is 120 columns. Most modern monitors are wide enough to deal with it, even when diffing/merging.

**NEVER** break user-visible strings, such as log messages, because that breaks the ability to search for them easily.

### Braces

Allman brace style is used across the board. Rationale is that with mixed-case names it greatly improves vertical readability. Other styles are nice (K&R especially) but they crush everything in which can make things like if statements hard to read at times.

```cpp
if ( boolean )
{
    // Stuff
}

switch ( value )
{
case x:
    // Stuff
case y:
    // Stuff
case z:
    // Stuff
default:
    // Stuff
}

while ( state )
{
    // Stuff
}

do
{
    // Stuff
}
while ( state )

int Function( int param )
{
    // Stuff
}
```

### Spaces

Use spaces after control flow statements:
```cpp
if switch case for do while
```

Use spaces around binary and ternary operators:
```cpp
= + - < > * / % | & ^ <= >= == != ? :
```

Do not use spaces after unary operators or functions:
```cpp
& * + - ~ ! sizeof() alignas() alignof() __attribute__() __declspec() defined()
```

Do not use spaces before or after increment or decrement unary operators:
```cpp
++ --
```

Do not use spaces around structure member operators:
```cpp
. ->
```

Use spaces inside parenthesis:
```cpp
while ( thing )
if ( Function( otherThing ) )
```

Do not leave trailing whitespace at the end of lines.

## 4. Naming Scheme

Write descriptive names with a module-like structure, going left-to-right from most general to most specific.

The descriptiveness of a name is inversely proportional to its scope. The smaller the scope, the shorter a name can be, and the larger the scope, the longer and more descriptive it should be.

For example, there is no need to call a temporary variable `TemporaryVariable`, just call it `tmp` and move on. Likewise calling a global-scope non-namespaced function `Init()` is asking for trouble, try something more specific like `RE_SomeSubsystem_Init()` (though, you can do just `Init()` if it's in namespace `RE::SomeSubsystem`).

Name using a directory-like structure, most general (left) to most specific (right).

### Code

All API-facing symbols should be prefixed with `RE` (functions, preprocessor definitions, macros) or `Re` (types, enum values, etc.). In C++ use `RE` as a top-level namespace for function symbols. In C# use `Refractor` as a top-level namespace. At one point we almost did `RS`, but

Examples (these are not real APIs thus far, just illustrative; for real APIs, once a program hits 1.0, refer to docs):

```cpp
// Examples of primitive data types
ReUint8
ReSint32
ReFloat64
ReBool

// Composite data types or enumerations
ReUint8Vec3
ReFloat64Mat4x4
ReRenderGraph
ReCamera3D
enum ReEntityType
{
    ReEntityType_Model,
    ReEntityType_Skeleton,
    ReEntityType_PointLight,
    ...
};

// C API and usage might look like this (.h)
// RE prefix -> Directory (module) -> Action (what are we doing)
RE_RenderGraph_Validate( &renderGraph );
RE_Audio_PushBuffer( &ctx->audioQueue, &buffer, bufferSize );
if ( RE_Entity_Exists( entity ) )
{
    return RE_True;
}
ReInputResult inputResult = RE_Input_Init( &inputParams );
if ( inputResult == RE_Failure )
{
    ReStringUTF8 failReason = RE_Input_GetError( inputResult );
#if RE_BUILD < RE_BUILD_SHIPPING
    RE_LOG( ReLogCategoryInput, ReLogType::Fatal, failReason );
#endif
    return RE_False;
}

// C++ API and usage might look like this (.hpp)
// By the way, this is super easy to generate once we have the C API, proving my point once again
RE::RenderGraph::Validate( &renderGraph );
RE::Audio::PushBuffer( &ctx->audioQueue, &buffer, bufferSize );
if ( RE::Entity::Exists( entity ) )
{
    return RE_True;
}

{
    using namespace RE::Input;
    ReInputResult inputResult = Init( &inputParams ); // ReInputResult is a C type and still in global namespace, but Init() is RE::Input::Init().
    if ( inputResult == RE_Failure )
    {
        ReStringUTF8 failReason = GetError( inputResult );
        if constexpr ( RE_BUILD < RE_BUILD_SHIPPING )
        {
            RE_LOG( ReLogCategoryInput, ReLogType::Fatal, failReason );
        }
        return RE_False;
    }
}
```

The naming scheme demonstrated above is readable and C/C++ compatible.

**Note:** In some code it's tempting to confuse `RE` with "Resident Evil" or "Reach for the Moon" (RE ENGINE) from Capcom. **These are not the same.** In fact Capcom's namespace is `Via`/`via` (for reasons that are between their engineers and God), and they call it Biohazard over there anyway, so it's a non-issue in practice.

I *almost* did `RS`/`Rs` instead but found `RE`/`Re` to be a bit more readable in our context. Lowercase e and capital E look rather different, while lowercase s and capital S look very similar, making the former easier to parse at a glance.

I just know that we have a lot of gaming nerds over here, including myself (a huge RE fan), so figured I'd write this down even though it'll only apply to some of us.

When the verb is the salient thing and the object is effectively the variant
being selected, put the verb first and fold the type into the tail:

```cpp
// C API
RE_Simd_AddVector4_Float32( a, b );
RE_Simd_AddVector4_Sint32( c, d );

// C++ API gets this added convenience, so the above is mostly relevant for C APIs
RE::Simd::AddVector4( a, b );
RE::Simd::AddVector4( c, d );

{
    using namespace RE::Simd;
    AddVector4( a, b );
    AddVector4( c, d );
}
```

### Offensive Names

We don't really care about "offensive" or "insensitive" names.

Rationale: We are engineers, not dogmatic thought or language police. New terms become offensive or insensitive all the time for the same reason that products get enshittified: someone's sad ass, somewhere, needs an excuse to invent a problem so that they can sell you a solution. One day a "safe" term you use will be deemed "offensive" by someone, so that they can sell you a reason to conform to their worldview. We would rather spend time engineering than trying to play a never-ending game of catch-up with them.

If reading or hearing `master` bothers you so much that you can't solve real engineering problems, then I pray for you and any "apprentice" that are unfortunate enough to meet you in the future.

Of course, you're free to use whatever terminology you want. I won't judge anyone for naming their repo's branch `main`. The important thing is that it communicates what you want. We're just not going to bend over backwards around someone else throwing a pissy fit over words.

### Directory Structure

Same rules apply, broadly speaking.

```cpp
// Bad include paths
#include <core.h>

// Better include paths:
#include <core/core.h>

// Best include paths:
#include <RE/Foundation/Foundation.h>
#include <RE/RenderGraph/RenderGraph.h>
#include <RE/Input/InputContext.h> // When we want more specific than <RE/Input/Input.h>
#include <RE/Input/InputTypes.h>
#include <RE/Audio/AudioMixerEffectBus.h> // When we want more specific than <RE/Audio/Audio.h>

#include <RE/Foundation/BeginAPI.h>

// code...

#include <RE/Foundation/EndAPI.h>
```

### Casing Conventions

If it wasn't obvious, let's solidify them:

| Symbol Type                                    | Convention                              |
|------------------------------------------------|-----------------------------------------|
| Preprocessor (Define/Macro)                    | `RE_UPPER_WITH_UNDERSCORES_AS_NEEDED`   |
| Types (structs, unions, enums, typedefs, etc.) | `RePascalCase`                          |
| Functions                                      | `RE_PascalCase_WithUnderscoresAsNeeded` |
| Variables                                      | `camelCase`                             |


## 5. Declarations and Definitions

When declaring an API function:

```c
/**
 * Brief description of what this does.
 *
 * Longer description that continues the brief description, if needed.
 *
 * That can be multiple blocks if you really have a lot to say.
 *
 * @param param1 Description
 * @param param2 Description
 * @param param3 Description
 * @param param4 Description
 *
 * @return What the return value means.
 *
 * @warning Any warnings on this function, if there are any.
 *
 * @threadsafe Whether or not this method is thread-safe, with an explanation of why or why not.
 *
 * @see Anything else that's relevant.
 */
ReReturnType RE_FunctionName( param1, param2, param3, param4, ... );

// Wrapping for functions with *lots* of parameters, or long enough parameters to hit the wrap limit (though if you have this many, consider a parameter struct instead)
ReReturnType RE_FunctionWithHellaParameters( param1, param2, param3, param4, param5, param6,
    param7, param8, param9, ... );
```

When defining a function:

```c
// Average definition
// If it's an internal (non-API) function, consider adding lightweight documentation
// Critical things to document would be return value meaning, warnings, thread-safety
qualifiers // static, inline, etc.
ReReturnType
RE_FunctionName( param1, param2, param3, param4 )
{
    // Function body
}

// Wrapping for functions with *lots* of parameters, or long enough parameters to hit the wrap limit
// If that list is big enough that someone comes by and asks "What the fuck is this?" then maybe it's time to group those parameters into a parameter struct
qualifiers
ReReturnType
RE_FunctionWithHellaParameters( param1, param2, param3, param4, param5, param6, param7,
    param8, param9, ... )
{
    // Function body
}
```

Note that implementation functions use line breaks between the return type and function name. This helps with searching (can `grep ^RE_FunctionName`).

## 6. Functions

### Prototypes

Pulling this from the [Linux kernel style](https://www.kernel.org/doc/html/latest/process/coding-style.html) for the sake of having an easy and complete example (albeit slightly modified):

---
(begin Linux Kernel Style)
```c
__init void * __must_check action(enum magic value, size_t size, u8 count,
                                  char *fmt, ...) __printf(4, 5) __malloc;
```

The order of elements for a function prototype (declaration):

- storage class (below, `static __always_inline`, noting that `__always_inline` is technically an attribute but is treated like inline)
- storage class attributes (here, `__init` -- i.e. section declarations, but also things like `__cold`)
- return type (here, `void *`)
- return type attributes (here, `__must_check`)
- function name (here, action)
- function parameters (here, `(enum magic value, size_t size, u8 count, char *fmt, ...)`, noting that parameter names should always be included)
- function parameter attributes (here, `__printf(4, 5)`)
- function behavior attributes (here, `__malloc`)

Note that for a function definition (i.e. the actual function body), the compiler does not allow function parameter attributes after the function parameters. In these cases, they should go after the storage class attributes (e.g. note the changed position of `__printf(4, 5)` below, compared to the declaration example above):

```c
static __always_inline __init __printf(4, 5) void * __must_check action(enum magic value,
               size_t size, u8 count, char *fmt, ...) __malloc
{
       ...
}
```
(end Linux Kernel Style)
---

For our own code, in a lot of cases many of these are irrelevant, but if for any reason they were to become relevant, this is what we follow. We also typically don't use these exact names. For example instead of `__always_inline` we have `RE_ALWAYS_INLINE_HINT`, and instead of `static` we have separate `internal`/`global`/`local_persist` that are each defined to `static` but in context make code much easier to read/understand.

That is not exhaustive, there are many other deviations within our own code and naming if that wasn't obvious enough, but you get the point.

## 7. `goto` and Centralized Exits

While `goto` isn't the worst thing in the world, we do often find ourselves using RAII (constructors/destructors) which can get fucked up by it. Generally it's fine for centralizing common function exit logic (for example multiple exit points needing to clean up something), just be careful with it.

Notably don't have a single jump label for things that could fail separately:

```cpp
SomeError:
    RE_Memory_Free( allocator, thing->other );
    RE_Memory_Free( allocator, thing );
    return result;
```

Since `thing` might be null here, prefer splitting:

```cpp
ThingOtherFailed:
    RE_Memory_Free( allocator, thing->other );
ThingFailed:
    RE_Memory_Free( allocator, thing );
    return result;
```

Obviously make sure you test for this. It's a good case for unit tests because it's pretty straightforward control flow management.

For C code this is useful. As I said before though, in C++ code we use a lot of RAII which can make this redundant, for example:

```cpp
{
    ReScopedLock lock( &mutex );
    // Now it's definitely a non-issue
}
```

## 8. Comments

### Style

Most of the examples thus far have used `//` line comments (and we will probably continue to do so for example code), but in real code we prefer to universally use `/* */` block comments, simply for consistency.

### What, Why, How

Explain the *what* and, if needed, the *why*. Usually the *what* is most relevant in the API, and the *why* is most relevant in implementation.

Very occasionally, a super simple *how* can be useful, but in 99% of cases the code probably just needs to be rewritten so that it's more self-explanatory. Generally you only ever comment *how* if the code has already been written to be as self-explanatory as possible, and it's just a genuinely hard-to-follow solution (because that's just the nature of some problems).

## 9. Macros and Enums

### Basic Rules

Names of macros are capitalized:
```cpp
#define RE_MATH_PI 3.14159...
```

Names of enum values should take the form `EnumName_Value`:
```cpp
enum ReEntityType : ReUint8
{
    ReEntityType_Model,
    ReEntityType_Skeleton,
    ReEntityType_Volume,
    ...
};
```

There are occasionally some macro names that make sense being named identically to functions and sometimes even identically to normal language symbols.

Macros with multiple statements should live in a do-while block:
```cpp
#define SOME_MACRO( x, y, z )       \
    do                              \
    {                               \
        if ( x > y )                \
        {                           \
            DoSomething( z );       \
        }                           \
    }                               \
    while ( 0 )
```

Do not have a macro itself directly affect control flow:
```cpp
// Acceptable: user does control flow themselves (in C++ you do ranged-for, in C this is the closest you'll get)
#define for_each( type, item, array, count )  for (type *item = (array); item , (array) + (count); item++ )

// BAD: Breaks the reader's internal parsing
#define SOME_MACRO( x )             \
    do                              \
    {                               \
        if ( x )                    \
        {                           \
            return someValue;       \
        }                           \
    }                               \
    while ( 0 )
```

Do not depend on having a local variable with some magic name:
```cpp
#define SOME_MACRO( x ) bar( x, y )
```

Do not implement macros with arguments that are used as l-values:
```cpp
// This will be a problem if someone changes the macro to be an inline function
FOO( x ) = y;
```

Do not forget about precedence:
```cpp
// Macros defining constants using expressions must enclose the expression in parenthesis.
// Of course in C++ you can use constexpr and a lot of these problems go away, but this is still relevant for C code and APIs.
#define SOME_VALUE 0x12345
#define SOME_EXPRESSION ( SOME_VALUE | 3 )
```

Be wary of namespace collisions when defining local variables in macros resembling functions. When in doubt, name macro local variables using the macro name as a prefix:
```cpp
// BAD: ret is prone to collision
#define SOME_MACRO( type, x )           \
    ({                                  \
        type ret;                       \
        ret = CalculateValue( x );      \
        (ret);                          \
    })

// BETTER: make it a lot harder to collide
#define SOME_MACRO( type, x )                   \
    ({                                          \
        type SOME_MACRO_ret;                    \
        SOME_MACRO_ret = CalculateValue( x );   \
        (SOME_MACRO_ret);                       \
    })
```

### `__FILE__` / `__LINE__` instrumentation

These can be extremely useful for debugging and logging purposes, so they're usually fine.

### Usability of Complicated Call Sites

Because of limitations, C APIs can be a... little rough to use at times. There can be cases where macros genuinely make a C API easier to call. The `flecs` library is full of examples like this, such as:
```c
/* Kind of cheating with a macro, but this is way more ergonomic and causes fewer mistakes
 * than typing out what it expands to manually. */
ECS_COMPONENT(world, component_type);
```

In the example above, `flecs` defines this such that it expands to a single function call, just making other things *within* the process of calling that function a hell of a lot easier to deal with.

Use your best judgement. Generally speaking, any function that would probably be templated in C++ *might* have a good reason to do something like this, but even then that's not a catch-all rule. Below is something that's closer to what we prefer to do.

### Cases involving just a type, and not a bunch of data

A common pattern for us is that where a C++ template would look like this:
```cpp
do_something<some_type>();
```

We might instead do:
```c
do_something(DO_SOMETHING_PARAM(some_type));
```

We do this a lot because we try to not generate a bunch of inline code using macros, which is what this would probably do if it were a real macro:
```c
DO_SOMETHING(some_type);
```

When using a macro just to do something with a type (e.g., fill out some kind of generic struct that wants info about a type, or that needs to do repetitive and easy-to-fuck-up work with something), this is typically a suitable pattern. It's certainly plenty readable and your debugger probably isn't going to break on it as long as you're not doing anything incredibly stupid inside one.

**Cases where we use this:** Basically any time we need info about a type at runtime and we would use a template in C++ to solve that. We almost would have used it in an all-purpose ECS framework:
```cpp
RE_Entity_RegisterComponent( RE_ENTITY_COMPONENT_TYPE( some_type ) );
```

Of course we didn't end up writing that (we found a far simpler solution to that instead), but the idea is roughly the same.

## 10. Data Types and Primitives

Use explicit-width types by default. There should be really no reason to use built-in C types.

| Category          | Types                                         |
|-------------------|-----------------------------------------------|
| Unsigned Integers | `ReUint8`, `ReUint16`, `ReUint32`, `ReUint64` |
| Signed Integers   | `ReSint8`, `ReSint16`, `ReSint32`, `ReSint64` |
| Floating-Point    | `ReFloat32`, `ReFloat64`                      |
| Boolean           | `ReBool`                                      |

Default to unsigned types. Signed types should be reserved for only when a value can genuinely be negative.

Vectors append `VecN` to the base type, where `N` is the number of axes. Example: `ReFloat32Vec3`.

Matrices append `MatNxM` to the base type, where `N` and `M` are the dimensions. Example: `ReFloat32Mat4x4`. As can be observed from that example, this is true even when `N == M`.

### Booleans and State

Booleans are fine as stack-local temporaries and return values, but storing them in structures is discouraged. Prefer, in order:

1. **Default to splitting data by state.** Keep things in separate arrays/structures by which state they're in, rather than carrying a flag on each element and branching.
2. **If logical state must be stored, use bitfields.** Use the boolean type for this (which is just a `typedef` over `ReUint8`):

    ```cpp
    ReBool isVisible : 1;
    ReBool isDirty   : 1;
    ```

This keeps structures small and avoids scattering branch-driving flags through hot data.

This also connects to a broader rule: **don't store derived data twice.** Storing `width`, `length`, *and* `area` invites the three to drifting out of sync with no canonical truth. Instead just recompute `area` as needed; it's always consistent, and math is cheaper than the extra memory access anyway.

### `sizeof` is an operator, not a function

Think of `sizeof` like a cast, not a call.

Prefer sizing from the *variable* rather than spelling out the type:

```c
/* Fragile: silently wrong if a's type changes. */
a = malloc( sizeof( float ) * 10 );

/* Robust: follows a's type automatically. */
a = malloc( sizeof( *a ) * 10 );
```

If you later change `a`'s type, the second form's allocation tracks it; the
literal-type form silently under- or over-allocates.

## 11. Memory Management

Even to this day, memory is slow as fuck, and probably will be for a long time. By comparison computation is free.

| Action          | Cost              |
|-----------------|-------------------|
| Register        | ~0 cycles         |
| L1 cache        | ~2–3 cycles       |
| L2 cache        | ~10–15 cycles     |
| Main memory     | up to ~200 cycles |
| SSE4.2 Multiply | ~1 cycle          |

(These numbers are CPU-dependent, but in that ballpark is where even a lot of modern CPUs fall.)

A SIMD core can do four or even eight multiplies per cycle, so a single 200-cycle memory fetch can cost the equivalent of ~800-1600 multiplies. That's a hell of a lot of performance to piss down the toilet.

It's unavoidable sometimes (can't fit everything in registers or cache), but generally write cache-coherent code when possible to minimize waiting on memory.

### Arrays beat linked lists

Linked-list nodes scatter across memory (cache misses) and waste space on `next` pointers. A growable array packs elements adjacently, so they can be prefetched together, thus making them faster to traverse.

Worth remembering these basic tricks:

- Grow in chunks (e.g. +16) or by doubling so reallocation is rare. Even doubling never uses more than ~2× memory, no worse than a linked list's pointer overhead.
- **Fast unordered removal:** swap the last element into the removed slot, decrement the count.
- **Ordered removal:** walk backward from the end; you're already scanning in-cache to find the element, so shift as you go. Still beats a linked list, since you usually have to *find* the element first anyway.

### `realloc` is good, not bad

Virtual memory hands out ~4 KB blocks and remaps them, so a large `realloc` often only needs to fix the tail rather than copy everything. Combined with chunked growth, `realloc` runs rarely and gives you contiguous, cache-friendly storage.

That said, we don't typically use the build-in libc allocator functions, but we do have our own allocators, some of which may include `realloc`-like functionality. If you find yourself needing it, use it.

### Advanced allocation tricks

For cases where you'd use a flexible array member, in C++ you can do some fucked-up stuff like this:

```cpp
struct Buffer {
    size_t len;
    char data[1];
};

char *raw = new char[sizeof(Buffer) + extraBytes];
Buffer *b = new (raw) Buffer(); // placement new to construct properly
```

We have a macro that works in C and C++ which does something like this called `RE_Memory_Allocate_Flexible()`. If needed, you can use that.

### Stride

Don't assume tight packing. Pass pointer, count, and **stride** (bytes to advance per element) instead of just pointer + count. The same routine then works on tightly packed data, on interleaved data (e.g. a color field inside a larger struct), or on padded buffers - no copying into a temporary. Stride makes functions dramatically more versatile, so it's definitely worth using.
