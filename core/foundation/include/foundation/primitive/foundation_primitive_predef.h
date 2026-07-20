/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/* TODO(will) pull in a lot of cpredef stuff here, including applying their recommended best practices... */

#if defined(__cplusplus)
#define LANGUAGE_CPP 1
#else
#define LANGUAGE_CPP 0
#endif

#if LANGUAGE_CPP
#define StaticCast(To)      static_cast<To>
#define ReinterpretCast(To) reinterpret_cast<To>
#else
#define StaticCast(To)      (To)
#define ReinterpretCast(To) (To)
#endif

#define PLATFORM_USE_DEFAULT_PRIMITIVE_INT 1
#define PLATFORM_USE_DEFAULT_PRIMITIVE_FLT 1
#define PLATFORM_USE_DEFAULT_PRIMITIVE_POINTER 1

/* NOTE(will) Default platform primitives. Violates C reserved naming because fuck you, this is internal and not to be used elsewhere.
 *            This technically isn't necessary with stdint.h and cstdint available, but done regardless to avoid dragging in unneeded
 *            header imports from the standard library if we already know the types (also good for C89 for which preserving API
 *            compatibility is a nice-to-have).
 */

/* TODO(will) Separate out into foundation_primitive_types_platform_begin.h and foundation_primitive_types_platform_end.h so we can define in a controlled scope. */
#if PLATFORM_USE_DEFAULT_PRIMITIVE_INT

#define __platform_primitive_uint8  unsigned char
#define __platform_primitive_uint16 unsigned short
#define __platform_primitive_uint32 unsigned int
#define __platform_primitive_uint64 unsigned long long

#define __platform_primitive_int8   signed char
#define __platform_primitive_int16  signed short
#define __platform_primitive_int32  signed int
#define __platform_primitive_int64  signed long long

/* TODO(will) Gotta define this too, for below C++11 and below C11. */
static_assert (sizeof (__platform_primitive_uint8)  == 1);
static_assert (sizeof (__platform_primitive_uint16) == 2);
static_assert (sizeof (__platform_primitive_uint32) == 4);
static_assert (sizeof (__platform_primitive_uint64) == 8);

static_assert (sizeof (__platform_primitive_int8)   == 1);
static_assert (sizeof (__platform_primitive_int16)  == 2);
static_assert (sizeof (__platform_primitive_int32)  == 4);
static_assert (sizeof (__platform_primitive_int64)  == 8);

#endif

#if PLATFORM_USE_DEFAULT_PRIMITIVE_FLT

#define __platform_primitive_float32 float
#define __platform_primitive_float64 double

static_assert (sizeof (__platform_primitive_float32) == 4);
static_assert (sizeof (__platform_primitive_float64) == 8);

#endif

/* NOTE(will) When moving the rest out, keep this *under* any custom changes to primitive int, in case we can reuse definitions from there. */
#if PLATFORM_USE_DEFAULT_PRIMITIVE_POINTER

#define __platform_primitive_uintptr __platform_primitive_uint64
#define __platform_primitive_intptr  __platform_primitive_int64

#define __platform_primitive_usize   __platform_primitive_uint64
#define __platform_primitive_isize   __platform_primitive_int64

#endif
