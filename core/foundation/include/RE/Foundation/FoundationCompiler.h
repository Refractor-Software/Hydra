/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    FoundationCompiler.h

    Compiler and language abstractions - everything that would otherwise force a raw __declspec,
    __attribute__, or language-version test out into ordinary code. Wrapping these here is the
    same "small footprint" reasoning applied to the compiler itself: one place to fix when a
    toolchain changes its mind, instead of a thousand scattered call sites.
*/

#if defined( __cplusplus )
#define RE_LANGUAGE_CPP 1
#else
#define RE_LANGUAGE_CPP 0
#endif

#if RE_LANGUAGE_CPP
#define RE_STATIC_CAST( To )      static_cast<To>
#define RE_REINTERPRET_CAST( To ) reinterpret_cast<To>
#else
#define RE_STATIC_CAST( To )      (To)
#define RE_REINTERPRET_CAST( To ) (To)
#endif

/* C++ never adopted restrict, so every compiler that matters to us spells it __restrict there
 * instead. Same guarantee, different keyword.
 */
#if RE_LANGUAGE_CPP
#define RE_RESTRICT __restrict
#else
#define RE_RESTRICT restrict
#endif

/* A hint, not a command - the compiler is still free to ignore it, and usually knows better. */
#if defined( _MSC_VER )
#define RE_ALWAYS_INLINE_HINT __forceinline
#elif defined( __GNUC__ ) || defined( __clang__ )
#define RE_ALWAYS_INLINE_HINT inline __attribute__( ( always_inline ) )
#else
#define RE_ALWAYS_INLINE_HINT inline
#endif

/* Thread-local storage duration. Compiler-level, not an OS call - the thread caches and the
 * ambient context both need it, and neither wants a platform boundary for something every
 * compiler has spelled natively for a decade.
 */
#if RE_LANGUAGE_CPP
#define RE_THREAD_LOCAL thread_local
#elif defined( _MSC_VER )
#define RE_THREAD_LOCAL __declspec( thread )
#else
#define RE_THREAD_LOCAL _Thread_local
#endif

/* Aligns a declaration. Used to keep per-thread state off shared cache lines, where false sharing
 * costs scaling in a way that shows up in no profile except as "it just doesn't speed up".
 */
#if RE_LANGUAGE_CPP
#define RE_ALIGN_AS( bytes ) alignas( bytes )
#elif defined( _MSC_VER )
#define RE_ALIGN_AS( bytes ) __declspec( align( bytes ) )
#else
#define RE_ALIGN_AS( bytes ) _Alignas( bytes )
#endif

/* Alignment of a type, as an integer constant expression.
 *
 * C11 spells this _Alignof and C++11 spells it alignof; <stdalign.h> would give C the alignof
 * spelling too, but pulling in a standard header for one keyword is exactly the dependency we
 * would rather not have. MSVC has understood __alignof since long before either standard.
 */
#if RE_LANGUAGE_CPP
#define RE_ALIGNOF( T ) alignof( T )
#elif defined( _MSC_VER )
#define RE_ALIGNOF( T ) __alignof( T )
#else
#define RE_ALIGNOF( T ) _Alignof( T )
#endif

/* Three names for what C spells `static` in three unrelated ways. Which one is meant is
 * otherwise only recoverable from context, and getting it wrong while reading is easy:
 *
 *   internal      - file-local linkage; invisible outside this translation unit.
 *   global        - a variable at file scope; lives for the whole program.
 *   local_persist - a variable inside a function that survives between calls.
 */
#define internal      static
#define global        static
#define local_persist static
