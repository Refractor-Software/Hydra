/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

struct ReAllocator
{
    void *_context;
    void *(*_alloc) (void *ctx, ReUint64 size, ReUint64 align);
    void( *_free )  (void *ctx, void *ptr);
};
