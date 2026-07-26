/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/* TODO(will) Per-directory includes - RE/Foundation/FoundationPrimitive.h for example */
#include <RE/Foundation/FoundationPrimitivePredef.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>

/* TODO(will) Build this. There is a lot. */

/* --- REFLECTION SYSTEM ---
 *
 * For a complete reflection system it can be asserted there are roughly seven categories of data to track:
 *
 * 1) Mechanical  - How do I copy/move/destroy this blob of bytes? - Example use case: generic containers
 * 2) Identity    - Is this the same type as that?                 - Example use case: any lookup-by-type (object properties, etc.)
 * 3) Descriptive - How should a human perceive this value?        - Example use case: inspector widget choice
 * 4) Structural  - What is this type made of?                     - Example use case: recursive inspector, serializer
 * 5) Behavioral  - What can this type do?                         - Example use case: scripting, signals, slots
 * 6) Access      - How do I read/write this correctly?            - Example use case: getter/setter (edit or display inspector value, etc.)
 * 7) Versioning  - Is this still the same shape it used to be?    - Example use case: schema migration (save game, etc.)
 *
 * Nearly everything else we do relies on this reflection data existing. This even includes things like our container types, all of which
 * use this reflection system to support out-of-line implementation and variant-like behavior without bloating anything (in fact, doing
 * them this way results in *more compact* binaries and possibly better performance with good API usage, and of course non-template variant
 * functionality is required for tools to have a single consistent API to work with at all in the first place).
 *
 * This API is all you need to work with our high-performance runtime reflection system.
 */

/* TODO(will) So regarding the stuff above - as stated, needs to be high performance and fairly general-purpose.
 *
 * To that end, this results in generally a few important considerations for the reflection system:
 *
 * 1) Split by consumer  - Don't store a giant descriptor internally, split it out (likely something resembling struct-of-arrays) so that it's
 *                         as easy as possible to access only small bits of reflection data as needed.
 *
 * 2) Accept user memory - Don't allocate memory internally with anything other than a user-provided allocator. This is because this library is
 *                         platform-independent and also the user should have full control over where reflection data gets placed. Of course this
 *                         typically means the user has to supply an allocator that can dynamically grow as reflection data is registered, but
 *                         that's not the worst thing in the world.
 *
 * That said, there's a number of practical things to keep in mind, split by responsibility:
 *
 * 1) Reflection GENERATION   - Compile-time, unordered process, using macros and other tricks as needed to generate static data. Lives in .rodata with zero runtime cost to construct. Doesn't need allocators.
 * 2) Reflection REGISTRATION - Run-time, ordered process, using lifecycles and requiring user memory management. Does need allocators.
 */

typedef struct ReTypeRegistry ReTypeRegistry;

/* TODO(will) Should figure out what parameters these take. Descriptor struct (ReTypeRegistryDesc), something else? */
ReTypeRegistry * RE_TypeRegistry_Create ();
void            RE_TypeRegistry_Destroy (ReTypeRegistry *reg);

/* TODO(will) Same thing as above. Should figure out what parameters these take. Descriptor struct, something else?
 *            Decided to split these out for granularity in case someone wants to allocate earlier and startup later.
 */
ReBool   RE_TypeRegistry_Startup (ReTypeRegistry *reg);
void RE_TypeRegistry_Shutdown (ReTypeRegistry *reg);

/* TODO(will) Exhaustively, what do these different structs need? And, how can we achieve the granularity we want and not make our API suck to use?
 *            Consider for example that editor might want more data than runtime, but with this library being foundational (hence the name 'foundation')
 *            and reusable across projects other than this engine, and also just for the sake of good code hygiene, isn't really supposed to know about
 *            any sort of "editor" or whatnot, so the user has to be able to composition of type info themselves as needed.
 */

typedef struct ReTypeDescriptor ReTypeDescriptor;

typedef struct ReTypeFieldDescriptor ReTypeFieldDescriptor;

enum ReTypeFieldKind
{
    ReTypeFieldKind_Uint8,
    ReTypeFieldKind_Uint16,
    ReTypeFieldKind_Uint32,
    ReTypeFieldKind_Uint64,

    ReTypeFieldKind_Sint8,
    ReTypeFieldKind_Sint16,
    ReTypeFieldKind_Sint32,
    ReTypeFieldKind_Sint64,

    ReTypeFieldKind_Float32,
    ReTypeFieldKind_Float64,

    ReTypeFieldKind_Bool,

    ReTypeFieldKind_Pointer,

    ReTypeFieldKind_Struct,

    ReTypeFieldKind_DefaultCount, /* Would be a cap for default type field kinds. Not sure about this though. Maybe look to SDL API for how they do similar stuff with input events? */

    /* TODO(will) Do we need any more kinds? User-defined kinds? (Maybe not the latter with STRUCT as a catch-all, but throwing that out there.) */

    ReTypeFieldKind_Max,
};

/* TODO(will) Any other structs and/or further decomposition out of the ones above should be done as well. */
