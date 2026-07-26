/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/* TODO(will) pull in a lot of cpredef stuff here, including applying their recommended best practices... */

#define RE_PLATFORM_USE_DEFAULT_PRIMITIVE_INT 1
#define RE_PLATFORM_USE_DEFAULT_PRIMITIVE_FLT 1

/* NOTE(will) Default platform primitives. Violates C reserved naming because fuck you, this is internal and not to be used elsewhere.
 *            This technically isn't necessary with stdint.h and cstdint available, but done regardless to avoid dragging in unneeded
 *            header imports from the standard library if we already know the types (also good for C89 for which preserving API
 *            compatibility is a nice-to-have).
 */

/* TODO(will) Separate out into FoundationPrimitiveTypesPlatformBegin.h and FoundationPrimitiveTypesPlatformEnd.h so we can define in a controlled scope. */
#if RE_PLATFORM_USE_DEFAULT_PRIMITIVE_INT

#define __platform_primitive_uint8  unsigned char
#define __platform_primitive_uint16 unsigned short
#define __platform_primitive_uint32 unsigned int
#define __platform_primitive_uint64 unsigned long long

#define __platform_primitive_int8   signed char
#define __platform_primitive_int16  signed short
#define __platform_primitive_int32  signed int
#define __platform_primitive_int64  signed long long

/* TODO(will) Gotta define this too, for below C++11 and below C11. */
static_assert( sizeof( __platform_primitive_uint8 )  == 1 );
static_assert( sizeof( __platform_primitive_uint16 ) == 2 );
static_assert( sizeof( __platform_primitive_uint32 ) == 4 );
static_assert( sizeof( __platform_primitive_uint64 ) == 8 );

static_assert( sizeof( __platform_primitive_int8 )   == 1 );
static_assert( sizeof( __platform_primitive_int16 )  == 2 );
static_assert( sizeof( __platform_primitive_int32 )  == 4 );
static_assert( sizeof( __platform_primitive_int64 )  == 8 );

#endif

#if RE_PLATFORM_USE_DEFAULT_PRIMITIVE_FLT

#define __platform_primitive_float32 float
#define __platform_primitive_float64 double

static_assert( sizeof( __platform_primitive_float32 ) == 4 );
static_assert( sizeof( __platform_primitive_float64 ) == 8 );

#endif
