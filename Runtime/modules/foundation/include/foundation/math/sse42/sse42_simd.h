// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <foundation/definitions.h>
#include <foundation/math/simd_register_base.h>
#include <foundation/primitive_types.h>

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <nmmintrin.h>
#include <type_traits>

namespace hydra::math::sse42
{
    class SIMD
    {
    public:
        struct float32x4 : SimdRegisterBase<__m128>
        {
            using Super::Super;
        };
        struct float64x2 : SimdRegisterBase<__m128d>
        {
            using Super::Super;
        };
        struct int32x4 : SimdRegisterBase<__m128i>
        {
            using Super::Super;
        };
        struct int64x2 : SimdRegisterBase<__m128i>
        {
            using Super::Super;
        };

        // ==========================================================
        //  LOAD / STORE
        // ==========================================================

        // --- float32x4 ---

        HYDRA_FORCE_INLINE static float32x4
        load(const float32 *p)
        {
            return _mm_load_ps(p); // 16-byte aligned
        }

        HYDRA_FORCE_INLINE static float32x4
        loadu(const float32 *p)
        {
            return _mm_loadu_ps(p); // unaligned
        }

        HYDRA_FORCE_INLINE static void
        store(float32 *p, const float32x4 a)
        {
            _mm_store_ps(p, a); // 16-byte aligned
        }

        HYDRA_FORCE_INLINE static void
        storeu(float32 *p, const float32x4 a)
        {
            _mm_storeu_ps(p, a);
        }

        HYDRA_FORCE_INLINE static void
        stream_store(float32 *p, const float32x4 a)
        {
            _mm_stream_ps(p, a); // non-temporal, bypasses cache
        }

        // --- float64x2 ---

        HYDRA_FORCE_INLINE static float64x2
        load(const float64 *p)
        {
            return _mm_load_pd(p);
        }

        HYDRA_FORCE_INLINE static float64x2
        loadu(const float64 *p)
        {
            return _mm_loadu_pd(p);
        }

        HYDRA_FORCE_INLINE static void
        store(float64 *p, const float64x2 a)
        {
            _mm_store_pd(p, a);
        }

        HYDRA_FORCE_INLINE static void
        storeu(float64 *p, const float64x2 a)
        {
            _mm_storeu_pd(p, a);
        }

        HYDRA_FORCE_INLINE static void
        stream_store(float64 *p, const float64x2 a)
        {
            _mm_stream_pd(p, a);
        }

        // --- int32x4 ---

        HYDRA_FORCE_INLINE static int32x4
        load(const int32_t *p)
        {
            return _mm_load_si128(reinterpret_cast<const __m128i *>(p));
        }

        HYDRA_FORCE_INLINE static int32x4
        loadu(const int32_t *p)
        {
            return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        }

        HYDRA_FORCE_INLINE static void
        store(int32_t *p, const int32x4 a)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(p), a);
        }

        HYDRA_FORCE_INLINE static void
        storeu(int32_t *p, const int32x4 a)
        {
            _mm_storeu_si128(reinterpret_cast<__m128i *>(p), a);
        }

        HYDRA_FORCE_INLINE static void
        stream_store(int32_t *p, const int32x4 a)
        {
            _mm_stream_si128(reinterpret_cast<__m128i *>(p), a);
        }

        // ==========================================================
        //  BROADCAST / SET / ZERO
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        broadcast(const float32 s)
        {
            return _mm_set1_ps(s);
        }

        HYDRA_FORCE_INLINE static float32x4
        set(const float32 a, const float32 b, const float32 c, const float32 d)
        {
            /* TODO(will) Apparently supposed to be (d, c, b, a) but xmmintrin.h shows
             *            as _mm_set_ps(float _A, float _B, float _C, float _D) - ???
             */
            return _mm_set_ps(a, b, c, d);
        }

        HYDRA_FORCE_INLINE static float32x4
        zero_f32x4()
        {
            return _mm_setzero_ps();
        }

        HYDRA_FORCE_INLINE static float64x2
        broadcast(const float64 s)
        {
            return _mm_set1_pd(s);
        }

        HYDRA_FORCE_INLINE static float64x2
        set(const float64 a, const float64 b)
        {
            return _mm_set_pd(b, a);
        }

        HYDRA_FORCE_INLINE static float64x2
        zero_f64x2()
        {
            return _mm_setzero_pd();
        }

        HYDRA_FORCE_INLINE static int32x4
        broadcast(const int32_t s)
        {
            return _mm_set1_epi32(s);
        }

        HYDRA_FORCE_INLINE static int32x4
        set(const int32_t a, const int32_t b, const int32_t c, const int32_t d)
        {
            return _mm_set_epi32(d, c, b, a);
        }

        HYDRA_FORCE_INLINE static int32x4
        zero_i32x4()
        {
            return _mm_setzero_si128();
        }

        // ==========================================================
        //  ARITHMETIC — float32x4
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        add(const float32x4 a, const float32x4 b)
        {
            return _mm_add_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        sub(const float32x4 a, const float32x4 b)
        {
            return _mm_sub_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        mul(const float32x4 a, const float32x4 b)
        {
            return _mm_mul_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        div(const float32x4 a, const float32x4 b)
        {
            return _mm_div_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        abs(const float32x4 a)
        {
            // Clear the sign bit via AND with 0x7FFFFFFF in each lane
            const __m128i mask = _mm_set1_epi32(0x7FFFFFFF);
            return _mm_and_ps(a, _mm_castsi128_ps(mask));
        }
        HYDRA_FORCE_INLINE static float32x4
        neg(const float32x4 a)
        {
            return _mm_xor_ps(a, _mm_set1_ps(-0.0f));
        }
        HYDRA_FORCE_INLINE static float32x4
        sqrt(const float32x4 a)
        {
            return _mm_sqrt_ps(a);
        }
        HYDRA_FORCE_INLINE static float32x4
        rcp(const float32x4 a)
        {
            return _mm_rcp_ps(a);
        } // ~12-bit precision
        HYDRA_FORCE_INLINE static float32x4
        rsqrt(const float32x4 a)
        {
            return _mm_rsqrt_ps(a);
        } // ~12-bit precision

        // FMA — requires FMA extension (available on all AVX2 CPUs, guarded here)
#if defined(__FMA__) || defined(__AVX2__)
        HYDRA_FORCE_INLINE static float32x4
        fma(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return _mm_fmadd_ps(a, b, c); // a*b + c
        }
        HYDRA_FORCE_INLINE static float32x4
        fms(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return _mm_fmsub_ps(a, b, c); // a*b - c
        }
        HYDRA_FORCE_INLINE static float32x4
        fnma(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return _mm_fnmadd_ps(a, b, c); // -(a*b) + c
        }
#else
        HYDRA_FORCE_INLINE static float32x4
        fma(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return add(mul(a, b), c);
        }
        HYDRA_FORCE_INLINE static float32x4
        fms(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return sub(mul(a, b), c);
        }
        HYDRA_FORCE_INLINE static float32x4
        fnma(const float32x4 a, const float32x4 b, const float32x4 c)
        {
            return add(neg(mul(a, b)), c);
        }
#endif

        // ==========================================================
        //  ARITHMETIC — float64x2
        // ==========================================================

        HYDRA_FORCE_INLINE static float64x2
        add(const float64x2 a, const float64x2 b)
        {
            return _mm_add_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        sub(const float64x2 a, const float64x2 b)
        {
            return _mm_sub_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        mul(const float64x2 a, const float64x2 b)
        {
            return _mm_mul_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        div(const float64x2 a, const float64x2 b)
        {
            return _mm_div_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        abs(const float64x2 a)
        {
            const __m128i mask = _mm_set1_epi64x(0x7FFFFFFFFFFFFFFFLL);
            return _mm_and_pd(a, _mm_castsi128_pd(mask));
        }
        HYDRA_FORCE_INLINE static float64x2
        neg(const float64x2 a)
        {
            return _mm_xor_pd(a, _mm_set1_pd(-0.0));
        }
        HYDRA_FORCE_INLINE static float64x2
        sqrt(const float64x2 a)
        {
            return _mm_sqrt_pd(a);
        }

#if defined(__FMA__)
        HYDRA_FORCE_INLINE static float64x2
        fma(float64x2 a, float64x2 b, float64x2 c)
        {
            return _mm_fmadd_pd(a, b, c);
        }
        HYDRA_FORCE_INLINE static float64x2
        fms(float64x2 a, float64x2 b, float64x2 c)
        {
            return _mm_fmsub_pd(a, b, c);
        }
        HYDRA_FORCE_INLINE static float64x2
        fnma(float64x2 a, float64x2 b, float64x2 c)
        {
            return _mm_fnmadd_pd(a, b, c);
        }
#else
        HYDRA_FORCE_INLINE static float64x2
        fma(const float64x2 a, const float64x2 b, const float64x2 c)
        {
            return add(mul(a, b), c);
        }
        HYDRA_FORCE_INLINE static float64x2
        fms(const float64x2 a, const float64x2 b, const float64x2 c)
        {
            return sub(mul(a, b), c);
        }
        HYDRA_FORCE_INLINE static float64x2
        fnma(const float64x2 a, const float64x2 b, const float64x2 c)
        {
            return add(neg(mul(a, b)), c);
        }
#endif

        // ==========================================================
        //  ARITHMETIC — int32x4
        // ==========================================================

        HYDRA_FORCE_INLINE static int32x4
        add(const int32x4 a, const int32x4 b)
        {
            return _mm_add_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        sub(const int32x4 a, const int32x4 b)
        {
            return _mm_sub_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        mul(const int32x4 a, const int32x4 b)
        {
            return _mm_mullo_epi32(a, b);
        } // SSE4.1
        HYDRA_FORCE_INLINE static int32x4
        abs(const int32x4 a)
        {
            return _mm_abs_epi32(a);
        } // SSSE3
        HYDRA_FORCE_INLINE static int32x4
        neg(const int32x4 a)
        {
            return _mm_sub_epi32(_mm_setzero_si128(), a);
        }

        // ==========================================================
        //  MIN / MAX / CLAMP
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        min(const float32x4 a, const float32x4 b)
        {
            return _mm_min_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        max(const float32x4 a, const float32x4 b)
        {
            return _mm_max_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        clamp(const float32x4 v, const float32x4 lo, const float32x4 hi)
        {
            return min(max(v, lo), hi);
        }

        HYDRA_FORCE_INLINE static float64x2
        min(const float64x2 a, const float64x2 b)
        {
            return _mm_min_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        max(const float64x2 a, const float64x2 b)
        {
            return _mm_max_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        clamp(const float64x2 v, const float64x2 lo, const float64x2 hi)
        {
            return min(max(v, lo), hi);
        }

        HYDRA_FORCE_INLINE static int32x4
        min(const int32x4 a, const int32x4 b)
        {
            return _mm_min_epi32(a, b);
        } // SSE4.1
        HYDRA_FORCE_INLINE static int32x4
        max(const int32x4 a, const int32x4 b)
        {
            return _mm_max_epi32(a, b);
        } // SSE4.1
        HYDRA_FORCE_INLINE static int32x4
        clamp(const int32x4 v, const int32x4 lo, const int32x4 hi)
        {
            return min(max(v, lo), hi);
        }

        // ==========================================================
        //  COMPARISON — return same-type mask (all bits set = true)
        // ==========================================================

        // --- float32x4 ---
        HYDRA_FORCE_INLINE static float32x4
        cmpeq(const float32x4 a, const float32x4 b)
        {
            return _mm_cmpeq_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        cmpneq(const float32x4 a, const float32x4 b)
        {
            return _mm_cmpneq_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        cmplt(const float32x4 a, const float32x4 b)
        {
            return _mm_cmplt_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        cmple(const float32x4 a, const float32x4 b)
        {
            return _mm_cmple_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        cmpgt(const float32x4 a, const float32x4 b)
        {
            return _mm_cmpgt_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        cmpge(const float32x4 a, const float32x4 b)
        {
            return _mm_cmpge_ps(a, b);
        }

        // --- float64x2 ---
        HYDRA_FORCE_INLINE static float64x2
        cmpeq(const float64x2 a, const float64x2 b)
        {
            return _mm_cmpeq_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        cmpneq(const float64x2 a, const float64x2 b)
        {
            return _mm_cmpneq_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        cmplt(const float64x2 a, const float64x2 b)
        {
            return _mm_cmplt_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        cmple(const float64x2 a, const float64x2 b)
        {
            return _mm_cmple_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        cmpgt(const float64x2 a, const float64x2 b)
        {
            return _mm_cmpgt_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        cmpge(const float64x2 a, const float64x2 b)
        {
            return _mm_cmpge_pd(a, b);
        }

        // --- int32x4 ---
        // Note: SSE4.2 has no unsigned integer comparisons — use bias trick (XOR sign bit) for unsigned
        HYDRA_FORCE_INLINE static int32x4
        cmpeq(const int32x4 a, const int32x4 b)
        {
            return _mm_cmpeq_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        cmplt(const int32x4 a, const int32x4 b)
        {
            return _mm_cmplt_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        cmpgt(const int32x4 a, const int32x4 b)
        {
            return _mm_cmpgt_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        cmple(const int32x4 a, const int32x4 b)
        {
            // a <= b  ===  NOT (a > b)
            return bitwise_not(cmpgt(a, b));
        }
        HYDRA_FORCE_INLINE static int32x4
        cmpneq(const int32x4 a, const int32x4 b)
        {
            return bitwise_not(cmpeq(a, b));
        }
        HYDRA_FORCE_INLINE static int32x4
        cmpge(const int32x4 a, const int32x4 b)
        {
            return bitwise_not(cmplt(a, b));
        }

        // Unsigned int32x4 comparisons via sign-bit bias
        HYDRA_FORCE_INLINE static int32x4
        cmplt_u32(const int32x4 a, const int32x4 b)
        {
            const __m128i bias = _mm_set1_epi32(static_cast<int32_t>(0x80000000));
            return _mm_cmplt_epi32(_mm_xor_si128(a, bias), _mm_xor_si128(b, bias));
        }
        HYDRA_FORCE_INLINE static int32x4
        cmpgt_u32(const int32x4 a, const int32x4 b)
        {
            const __m128i bias = _mm_set1_epi32(static_cast<int32_t>(0x80000000));
            return _mm_cmpgt_epi32(_mm_xor_si128(a, bias), _mm_xor_si128(b, bias));
        }

        // ==========================================================
        //  MASKING / SELECTION
        // ==========================================================

        // blend — select per lane based on mask (mask all-1 = take b, all-0 = take a)
        HYDRA_FORCE_INLINE static float32x4
        blend(const float32x4 a, const float32x4 b, const float32x4 mask)
        {
            return _mm_blendv_ps(a, b, mask);
        }

        // blend with compile-time immediate (faster — single instruction, no mask register)
        // imm8: bit N set = lane N takes from b, else a
        template <int Imm8>
        HYDRA_FORCE_INLINE static float32x4
        blend_imm(const float32x4 a, const float32x4 b)
        {
            return _mm_blend_ps(a, b, Imm8);
        }

        HYDRA_FORCE_INLINE static float64x2
        blend(const float64x2 a, const float64x2 b, const float64x2 mask)
        {
            return _mm_blendv_pd(a, b, mask);
        }

        template <int Imm8>
        HYDRA_FORCE_INLINE static float64x2
        blend_imm(const float64x2 a, const float64x2 b)
        {
            return _mm_blend_pd(a, b, Imm8);
        }

        HYDRA_FORCE_INLINE static int32x4
        blend(const int32x4 a, const int32x4 b, const int32x4 mask)
        {
            return _mm_blendv_epi8(a, b, mask); // SSE4.1 - byte-granularity mask
        }

        template <int Imm8>
        HYDRA_FORCE_INLINE static int32x4
        blend_imm(const int32x4 a, const int32x4 b)
        {
            /* TODO(will) Uhh... AVX2? Apparently so. Definitely not SSE4.1; comes from immintin.h - ??? */
            return _mm_blend_epi32(a, b, Imm8);
        }

        // movemask — collapse to scalar bitmask (1 bit per lane)
        HYDRA_FORCE_INLINE static int
        movemask(const float32x4 a)
        {
            return _mm_movemask_ps(a);
        }
        HYDRA_FORCE_INLINE static int
        movemask(const float64x2 a)
        {
            return _mm_movemask_pd(a);
        }
        HYDRA_FORCE_INLINE static int
        movemask(const int32x4 a)
        {
            return _mm_movemask_epi8(a);
        } // 1 bit per byte (16 bits)

        HYDRA_FORCE_INLINE static bool
        any_true(const float32x4 mask)
        {
            return movemask(mask) != 0;
        }
        HYDRA_FORCE_INLINE static bool
        all_true(const float32x4 mask)
        {
            return movemask(mask) == 0xF;
        }
        HYDRA_FORCE_INLINE static bool
        none_true(const float32x4 mask)
        {
            return movemask(mask) == 0;
        }

        HYDRA_FORCE_INLINE static bool
        any_true(const float64x2 mask)
        {
            return movemask(mask) != 0;
        }
        HYDRA_FORCE_INLINE static bool
        all_true(const float64x2 mask)
        {
            return movemask(mask) == 0x3;
        }
        HYDRA_FORCE_INLINE static bool
        none_true(const float64x2 mask)
        {
            return movemask(mask) == 0;
        }

        HYDRA_FORCE_INLINE static bool
        any_true(const int32x4 mask)
        {
            return movemask(mask) != 0;
        }
        HYDRA_FORCE_INLINE static bool
        all_true(const int32x4 mask)
        {
            return movemask(mask) == 0xFFFF;
        }
        HYDRA_FORCE_INLINE static bool
        none_true(const int32x4 mask)
        {
            return movemask(mask) == 0;
        }

        // ==========================================================
        //  BITWISE
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        bitwise_and(const float32x4 a, const float32x4 b)
        {
            return _mm_and_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        bitwise_or(const float32x4 a, const float32x4 b)
        {
            return _mm_or_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        bitwise_xor(const float32x4 a, const float32x4 b)
        {
            return _mm_xor_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        bitwise_not(const float32x4 a)
        {
            return _mm_xor_ps(a, _mm_castsi128_ps(_mm_set1_epi32(-1)));
        }
        HYDRA_FORCE_INLINE static float32x4
        bitwise_andnot(const float32x4 a, const float32x4 b)
        {
            return _mm_andnot_ps(a, b);
        } // ~a & b

        HYDRA_FORCE_INLINE static float64x2
        bitwise_and(const float64x2 a, const float64x2 b)
        {
            return _mm_and_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        bitwise_or(const float64x2 a, const float64x2 b)
        {
            return _mm_or_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        bitwise_xor(const float64x2 a, const float64x2 b)
        {
            return _mm_xor_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        bitwise_not(const float64x2 a)
        {
            return _mm_xor_pd(a, _mm_castsi128_pd(_mm_set1_epi32(-1)));
        }
        HYDRA_FORCE_INLINE static float64x2
        bitwise_andnot(const float64x2 a, const float64x2 b)
        {
            return _mm_andnot_pd(a, b);
        }

        HYDRA_FORCE_INLINE static int32x4
        bitwise_and(const int32x4 a, const int32x4 b)
        {
            return _mm_and_si128(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        bitwise_or(const int32x4 a, const int32x4 b)
        {
            return _mm_or_si128(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        bitwise_xor(const int32x4 a, const int32x4 b)
        {
            return _mm_xor_si128(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        bitwise_not(const int32x4 a)
        {
            return _mm_xor_si128(a, _mm_set1_epi32(-1));
        }
        HYDRA_FORCE_INLINE static int32x4
        bitwise_andnot(const int32x4 a, const int32x4 b)
        {
            return _mm_andnot_si128(a, b);
        } // ~a & b

        // Shifts — compile-time immediate
        template <int Imm>
        HYDRA_FORCE_INLINE static int32x4
        shift_left_imm(const int32x4 a)
        {
            return _mm_slli_epi32(a, Imm);
        }

        template <int Imm>
        HYDRA_FORCE_INLINE static int32x4
        shift_right_imm(const int32x4 a)
        {
            return _mm_srli_epi32(a, Imm);
        } // logical

        template <int Imm>
        HYDRA_FORCE_INLINE static int32x4
        shift_right_arith_imm(const int32x4 a)
        {
            return _mm_srai_epi32(a, Imm);
        } // arithmetic (sign-extending)

        // Shifts — runtime variable (SSE4.2 has no per-lane variable shift for i32 — use scalar emulation)
        // Note: AVX2 introduces _mm256_sllv_epi32 for per-lane variable shifts.
        // Uniform variable shift (same count applied to all lanes) is supported:
        HYDRA_FORCE_INLINE static int32x4
        shift_left(const int32x4 a, const int count)
        {
            return _mm_sll_epi32(a, _mm_cvtsi32_si128(count));
        }
        HYDRA_FORCE_INLINE static int32x4
        shift_right(const int32x4 a, const int count)
        {
            return _mm_srl_epi32(a, _mm_cvtsi32_si128(count));
        }
        HYDRA_FORCE_INLINE static int32x4
        shift_right_arith(const int32x4 a, const int count)
        {
            return _mm_sra_epi32(a, _mm_cvtsi32_si128(count));
        }

        // ==========================================================
        //  ROUNDING — float32x4 / float64x2
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        floor(const float32x4 a)
        {
            return _mm_floor_ps(a);
        } // SSE4.1
        HYDRA_FORCE_INLINE static float32x4
        ceil(const float32x4 a)
        {
            return _mm_ceil_ps(a);
        } // SSE4.1
        HYDRA_FORCE_INLINE static float32x4
        round(const float32x4 a)
        {
            return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        }
        HYDRA_FORCE_INLINE static float32x4
        truncate(const float32x4 a)
        {
            return _mm_round_ps(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
        }

        HYDRA_FORCE_INLINE static float64x2
        floor(const float64x2 a)
        {
            return _mm_floor_pd(a);
        }
        HYDRA_FORCE_INLINE static float64x2
        ceil(const float64x2 a)
        {
            return _mm_ceil_pd(a);
        }
        HYDRA_FORCE_INLINE static float64x2
        round(const float64x2 a)
        {
            return _mm_round_pd(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        }
        HYDRA_FORCE_INLINE static float64x2
        truncate(const float64x2 a)
        {
            return _mm_round_pd(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
        }

        // ==========================================================
        //  CONVERSION
        // ==========================================================

        HYDRA_FORCE_INLINE static int32x4
        f32x4_to_i32x4(const float32x4 a)
        {
            return _mm_cvtps_epi32(a);
        } // round-to-nearest
        HYDRA_FORCE_INLINE static int32x4
        f32x4_to_i32x4_trunc(const float32x4 a)
        {
            return _mm_cvttps_epi32(a);
        } // truncate
        HYDRA_FORCE_INLINE static float32x4
        i32x4_to_f32x4(const int32x4 a)
        {
            return _mm_cvtepi32_ps(a);
        }

        HYDRA_FORCE_INLINE static float32x4
        f64x2_to_f32x4(const float64x2 a)
        {
            return _mm_cvtpd_ps(a);
        } // 2 doubles -> 2 floats in low lanes
        HYDRA_FORCE_INLINE static float64x2
        f32x4_to_f64x2(const float32x4 a)
        {
            return _mm_cvtps_pd(a);
        } // low 2 floats -> 2 doubles

        HYDRA_FORCE_INLINE static int32x4
        f64x2_to_i32x4(const float64x2 a)
        {
            return _mm_cvtpd_epi32(a);
        } // 2 doubles -> 2 ints in low lanes
        HYDRA_FORCE_INLINE static float64x2
        i32x4_to_f64x2(const int32x4 a)
        {
            return _mm_cvtepi32_pd(a);
        } // low 2 ints -> 2 doubles

        // ==========================================================
        //  REDUCTION (horizontal)
        //  Note: these are intentionally slow — use sparingly
        // ==========================================================

        HYDRA_FORCE_INLINE static float32
        hadd(const float32x4 a)
        {
            __m128 t = _mm_hadd_ps(a, a);
            t = _mm_hadd_ps(t, t);
            return _mm_cvtss_f32(t);
        }

        HYDRA_FORCE_INLINE static float32
        hmin(const float32x4 a)
        {
            __m128 t = _mm_min_ps(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1)));
            t = _mm_min_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(1, 0, 3, 2)));
            return _mm_cvtss_f32(t);
        }

        HYDRA_FORCE_INLINE static float32
        hmax(const float32x4 a)
        {
            __m128 t = _mm_max_ps(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1)));
            t = _mm_max_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(1, 0, 3, 2)));
            return _mm_cvtss_f32(t);
        }

        HYDRA_FORCE_INLINE static float32
        dot(const float32x4 a, const float32x4 b)
        {
            return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF)); // SSE4.1 dot product, all lanes
        }

        HYDRA_FORCE_INLINE static float64
        hadd(const float64x2 a)
        {
            const __m128d t = _mm_hadd_pd(a, a);
            return _mm_cvtsd_f64(t);
        }

        HYDRA_FORCE_INLINE static int32_t
        hadd(const int32x4 a)
        {
            // No integer hadd in SSE4.2 — shuffle and add
            __m128i t = _mm_hadd_epi32(a, a);
            t = _mm_hadd_epi32(t, t);
            return _mm_cvtsi128_si32(t);
        }

        // ==========================================================
        //  SHUFFLE / PERMUTE
        // ==========================================================

        // Shuffle with compile-time immediate
        // imm8 encodes which source lanes map to dst lanes 0..3
        template <int Imm8>
        HYDRA_FORCE_INLINE static float32x4
        shuffle(const float32x4 a, const float32x4 b)
        {
            return _mm_shuffle_ps(a, b, Imm8);
        }

        template <int Imm8>
        HYDRA_FORCE_INLINE static float32x4
        permute(const float32x4 a)
        {
            return _mm_permute_ps(a, Imm8); // SSE4.1 — permute within single register
        }

        template <int Imm8>
        HYDRA_FORCE_INLINE static float64x2
        shuffle(const float64x2 a, const float64x2 b)
        {
            return _mm_shuffle_pd(a, b, Imm8);
        }

        template <int Imm8>
        HYDRA_FORCE_INLINE static int32x4
        shuffle_i32(const int32x4 a)
        {
            return _mm_shuffle_epi32(a, Imm8);
        }

        HYDRA_FORCE_INLINE static float32x4
        unpack_lo(const float32x4 a, const float32x4 b)
        {
            return _mm_unpacklo_ps(a, b);
        }
        HYDRA_FORCE_INLINE static float32x4
        unpack_hi(const float32x4 a, const float32x4 b)
        {
            return _mm_unpackhi_ps(a, b);
        }

        HYDRA_FORCE_INLINE static float64x2
        unpack_lo(const float64x2 a, const float64x2 b)
        {
            return _mm_unpacklo_pd(a, b);
        }
        HYDRA_FORCE_INLINE static float64x2
        unpack_hi(const float64x2 a, const float64x2 b)
        {
            return _mm_unpackhi_pd(a, b);
        }

        HYDRA_FORCE_INLINE static int32x4
        unpack_lo(const int32x4 a, const int32x4 b)
        {
            return _mm_unpacklo_epi32(a, b);
        }
        HYDRA_FORCE_INLINE static int32x4
        unpack_hi(const int32x4 a, const int32x4 b)
        {
            return _mm_unpackhi_epi32(a, b);
        }

        // ==========================================================
        //  LANE EXTRACTION (scalar — debug / tail use only)
        //  Mark call sites clearly — these are slow
        // ==========================================================

        template <int Lane>
        HYDRA_FORCE_INLINE static float32
        lane(const float32x4 a)
        {
            static_assert(Lane >= 0 && Lane < 4);
            float32 result;
            // Type-safe extraction via _mm_extract_ps returns int bits
            const int bits = _mm_extract_ps(a, Lane);
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }

        template <int Lane>
        HYDRA_FORCE_INLINE static float64
        lane(const float64x2 a)
        {
            static_assert(Lane >= 0 && Lane < 2);
            if constexpr (Lane == 0)
                return _mm_cvtsd_f64(a);
            else
                return _mm_cvtsd_f64(_mm_unpackhi_pd(a, a));
        }

        template <int Lane>
        HYDRA_FORCE_INLINE static int32_t
        lane(const int32x4 a)
        {
            static_assert(Lane >= 0 && Lane < 4);
            return _mm_extract_epi32(a, Lane); // SSE4.1
        }

        // ==========================================================
        //  PARTIAL LOAD / STORE (loop tail handling)
        //  Scalar fallback — intentionally not SIMD
        // ==========================================================

        HYDRA_FORCE_INLINE static float32x4
        load_partial(const float32 *p, const int count)
        {
            alignas(16) float32 tmp[4] = {};
            for (int i = 0; i < count; ++i)
                tmp[i] = p[i];
            return load(tmp);
        }

        HYDRA_FORCE_INLINE static void
        store_partial(float32 *p, const float32x4 a, const int count)
        {
            alignas(16) float32 tmp[4];
            store(tmp, a);
            for (int i = 0; i < count; ++i)
                p[i] = tmp[i];
        }

        HYDRA_FORCE_INLINE static int32x4
        load_partial(const int32_t *p, const int count)
        {
            alignas(16) int32_t tmp[4] = {};
            for (int i = 0; i < count; ++i)
                tmp[i] = p[i];
            return load(tmp);
        }

        HYDRA_FORCE_INLINE static void
        store_partial(int32_t *p, const int32x4 a, const int count)
        {
            alignas(16) int32_t tmp[4];
            store(tmp, a);
            for (int i = 0; i < count; ++i)
                p[i] = tmp[i];
        }

    }; // class SIMD
} // namespace hydra::math::sse42

// ============================================================
//  Operator overloads
//  Delegate to SIMD::op — template covers all six register types
//  Constrained to SimdRegisterBase<T> and its derived structs
// ============================================================

namespace hydra
{
    // Concept to constrain operators to our register wrapper types
    template <typename T>
    concept simd_type =
        std::is_same_v<T, math::sse42::SIMD::float32x4> || std::is_same_v<T, math::sse42::SIMD::float64x2> ||
        std::is_same_v<T, math::sse42::SIMD::int32x4>;

    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator+(T a, T b)
    {
        return math::sse42::SIMD::add(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator-(T a, T b)
    {
        return math::sse42::SIMD::sub(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator*(T a, T b)
    {
        return math::sse42::SIMD::mul(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator/(T a, T b)
    {
        return math::sse42::SIMD::div(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator-(T a)
    {
        return math::sse42::SIMD::neg(a);
    }

    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator&(T a, T b)
    {
        return math::sse42::SIMD::bitwise_and(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator|(T a, T b)
    {
        return math::sse42::SIMD::bitwise_or(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator^(T a, T b)
    {
        return math::sse42::SIMD::bitwise_xor(a, b);
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T
    operator~(T a)
    {
        return math::sse42::SIMD::bitwise_not(a);
    }

    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator+=(T &a, T b)
    {
        a = a + b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator-=(T &a, T b)
    {
        a = a - b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator*=(T &a, T b)
    {
        a = a * b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator/=(T &a, T b)
    {
        a = a / b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator&=(T &a, T b)
    {
        a = a & b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator|=(T &a, T b)
    {
        a = a | b;
        return a;
    }
    template <simd_type T>
    HYDRA_FORCE_INLINE T &
    operator^=(T &a, T b)
    {
        a = a ^ b;
        return a;
    }
} // namespace hydra
