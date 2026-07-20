/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/* TODO(will) Per-directory includes - foundation/primitive/foundation_primitive.h for example */
#include "foundation/primitive/foundation_primitive_predef.h"
#include "foundation/primitive/foundation_primitive_types.h"

#include "foundation/memory/foundation_memory_allocator.h"

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

typedef struct type_registry type_registry;

/* TODO(will) Should figure out what parameters these take. Descriptor struct (type_registry_desc), something else? */
type_registry * type_registry_create ();
void            type_registry_destroy (type_registry *reg);

/* TODO(will) Same thing as above. Should figure out what parameters these take. Descriptor struct, something else?
 *            Decided to split these out for granularity in case someone wants to allocate earlier and startup later.
 */
b8   type_registry_startup (type_registry *reg);
void type_registry_shutdown (type_registry *reg);

/* TODO(will) Exhaustively, what do these different structs need? And, how can we achieve the granularity we want and not make our API suck to use?
 *            Consider for example that editor might want more data than runtime, but with this library being foundational (hence the name 'foundation')
 *            and reusable across projects other than this engine, and also just for the sake of good code hygiene, isn't really supposed to know about
 *            any sort of "editor" or whatnot, so the user has to be able to composition of type info themselves as needed.
 */

typedef struct type_descriptor type_descriptor;

typedef struct type_field_descriptor type_field_descriptor;

enum type_field_kind
{
    TYPE_FIELD_U8,
    TYPE_FIELD_U16,
    TYPE_FIELD_U32,
    TYPE_FIELD_U64,

    TYPE_FIELD_S8,
    TYPE_FIELD_S16,
    TYPE_FIELD_S32,
    TYPE_FIELD_S64,

    TYPE_FIELD_F32,
    TYPE_FIELD_F64,

    TYPE_FIELD_B8,

    TYPE_FIELD_PTR,

    TYPE_FIELD_STRUCT,

    TYPE_FIELD_DEFAULT_COUNT, /* Would be a cap for default type field kinds. Not sure about this though. Maybe look to SDL API for how they do similar stuff with input events? */

    /* TODO(will) Do we need any more kinds? User-defined kinds? (Maybe not the latter with STRUCT as a catch-all, but throwing that out there.) */

    TYPE_FIELD_MAX,
};

/* TODO(will) Any other structs and/or further decomposition out of the ones above should be done as well. */
