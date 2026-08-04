/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#define UNICODE
#include <windows.h>

/* The kernel's common prelude - every Win64 translation unit reaches windows.h through this
 * header, so this is where the storage-class spellings (internal/global/local_persist) come
 * from for kernel code that has no other reason to pull in foundation.
 */
#include <RE/Foundation/FoundationCompiler.h>
