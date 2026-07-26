/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationCompiler.h>
#include <RE/Foundation/FoundationPrimitivePredef.h>

typedef __platform_primitive_uint8   ReUint8;
typedef __platform_primitive_uint16  ReUint16;
typedef __platform_primitive_uint32  ReUint32;
typedef __platform_primitive_uint64  ReUint64;

typedef __platform_primitive_int8    ReSint8;
typedef __platform_primitive_int16   ReSint16;
typedef __platform_primitive_int32   ReSint32;
typedef __platform_primitive_int64   ReSint64;

typedef __platform_primitive_float32 ReFloat32;
typedef __platform_primitive_float64 ReFloat64;

typedef __platform_primitive_uint8   ReBool;

#define RE_False 0
#define RE_True  1
