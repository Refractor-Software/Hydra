// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <foundation/definitions.h>
#include <foundation/primitive_types.h>

#include <foundation/math/simd_register_base.h>

#if HYDRA_SIMD_AVX2
#include <immintrin.h>
#endif

namespace hydra::math::avx2
{
    class SIMD
    {
#if HYDRA_SIMD_AVX2
        using int32x8 = __m256i;

        using float32x8 = __m256;
        using float64x4 = __m256d;
#endif
    };
} // namespace hydra::math::avx2
