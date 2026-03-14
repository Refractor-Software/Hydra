// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#if defined(_MSC_VER)
#define HYDRA_FORCE_INLINE __forceinline
#else
#define HYDRA_FORCE_INLINE __attribute__((always_inline))
#endif

#define HYDRA_SIMD_AVX2 1
