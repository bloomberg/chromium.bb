/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/bitdepth_conversion_avx2.h"
#include "aom_ports/mem.h"

static void hadamard_col8x2_avx2(__m256i *in, int iter) {
  __m256i a0 = in[0];
  __m256i a1 = in[1];
  __m256i a2 = in[2];
  __m256i a3 = in[3];
  __m256i a4 = in[4];
  __m256i a5 = in[5];
  __m256i a6 = in[6];
  __m256i a7 = in[7];

  __m256i b0 = _mm256_add_epi16(a0, a1);
  __m256i b1 = _mm256_sub_epi16(a0, a1);
  __m256i b2 = _mm256_add_epi16(a2, a3);
  __m256i b3 = _mm256_sub_epi16(a2, a3);
  __m256i b4 = _mm256_add_epi16(a4, a5);
  __m256i b5 = _mm256_sub_epi16(a4, a5);
  __m256i b6 = _mm256_add_epi16(a6, a7);
  __m256i b7 = _mm256_sub_epi16(a6, a7);

  a0 = _mm256_add_epi16(b0, b2);
  a1 = _mm256_add_epi16(b1, b3);
  a2 = _mm256_sub_epi16(b0, b2);
  a3 = _mm256_sub_epi16(b1, b3);
  a4 = _mm256_add_epi16(b4, b6);
  a5 = _mm256_add_epi16(b5, b7);
  a6 = _mm256_sub_epi16(b4, b6);
  a7 = _mm256_sub_epi16(b5, b7);

  if (iter == 0) {
    b0 = _mm256_add_epi16(a0, a4);
    b7 = _mm256_add_epi16(a1, a5);
    b3 = _mm256_add_epi16(a2, a6);
    b4 = _mm256_add_epi16(a3, a7);
    b2 = _mm256_sub_epi16(a0, a4);
    b6 = _mm256_sub_epi16(a1, a5);
    b1 = _mm256_sub_epi16(a2, a6);
    b5 = _mm256_sub_epi16(a3, a7);

    a0 = _mm256_unpacklo_epi16(b0, b1);
    a1 = _mm256_unpacklo_epi16(b2, b3);
    a2 = _mm256_unpackhi_epi16(b0, b1);
    a3 = _mm256_unpackhi_epi16(b2, b3);
    a4 = _mm256_unpacklo_epi16(b4, b5);
    a5 = _mm256_unpacklo_epi16(b6, b7);
    a6 = _mm256_unpackhi_epi16(b4, b5);
    a7 = _mm256_unpackhi_epi16(b6, b7);

    b0 = _mm256_unpacklo_epi32(a0, a1);
    b1 = _mm256_unpacklo_epi32(a4, a5);
    b2 = _mm256_unpackhi_epi32(a0, a1);
    b3 = _mm256_unpackhi_epi32(a4, a5);
    b4 = _mm256_unpacklo_epi32(a2, a3);
    b5 = _mm256_unpacklo_epi32(a6, a7);
    b6 = _mm256_unpackhi_epi32(a2, a3);
    b7 = _mm256_unpackhi_epi32(a6, a7);

    in[0] = _mm256_unpacklo_epi64(b0, b1);
    in[1] = _mm256_unpackhi_epi64(b0, b1);
    in[2] = _mm256_unpacklo_epi64(b2, b3);
    in[3] = _mm256_unpackhi_epi64(b2, b3);
    in[4] = _mm256_unpacklo_epi64(b4, b5);
    in[5] = _mm256_unpackhi_epi64(b4, b5);
    in[6] = _mm256_unpacklo_epi64(b6, b7);
    in[7] = _mm256_unpackhi_epi64(b6, b7);
  } else {
    in[0] = _mm256_add_epi16(a0, a4);
    in[7] = _mm256_add_epi16(a1, a5);
    in[3] = _mm256_add_epi16(a2, a6);
    in[4] = _mm256_add_epi16(a3, a7);
    in[2] = _mm256_sub_epi16(a0, a4);
    in[6] = _mm256_sub_epi16(a1, a5);
    in[1] = _mm256_sub_epi16(a2, a6);
    in[5] = _mm256_sub_epi16(a3, a7);
  }
}

static void hadamard_8x8x2_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                                int16_t *coeff) {
  __m256i src[8];
  src[0] = _mm256_loadu_si256((const __m256i *)src_diff);
  src[1] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[2] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[3] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[4] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[5] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[6] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[7] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));

  hadamard_col8x2_avx2(src, 0);
  hadamard_col8x2_avx2(src, 1);

  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[0], src[1], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[2], src[3], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[4], src[5], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[6], src[7], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[0], src[1], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[2], src[3], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[4], src[5], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[6], src[7], 0x31));
}

static INLINE void hadamard_16x16_avx2(const int16_t *src_diff,
                                       ptrdiff_t src_stride, tran_low_t *coeff,
                                       int is_final) {
  DECLARE_ALIGNED(32, int16_t, temp_coeff[16 * 16]);
  int16_t *t_coeff = temp_coeff;
  int16_t *coeff16 = (int16_t *)coeff;
  int idx;
  for (idx = 0; idx < 2; ++idx) {
    const int16_t *src_ptr = src_diff + idx * 8 * src_stride;
    hadamard_8x8x2_avx2(src_ptr, src_stride, t_coeff + (idx * 64 * 2));
  }

  for (idx = 0; idx < 64; idx += 16) {
    const __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    const __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 64));
    const __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 128));
    const __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 192));

    __m256i b0 = _mm256_add_epi16(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi16(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi16(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi16(coeff2, coeff3);

    b0 = _mm256_srai_epi16(b0, 1);
    b1 = _mm256_srai_epi16(b1, 1);
    b2 = _mm256_srai_epi16(b2, 1);
    b3 = _mm256_srai_epi16(b3, 1);
    if (is_final) {
      store_tran_low(_mm256_add_epi16(b0, b2), coeff);
      store_tran_low(_mm256_add_epi16(b1, b3), coeff + 64);
      store_tran_low(_mm256_sub_epi16(b0, b2), coeff + 128);
      store_tran_low(_mm256_sub_epi16(b1, b3), coeff + 192);
      coeff += 16;
    } else {
      _mm256_storeu_si256((__m256i *)coeff16, _mm256_add_epi16(b0, b2));
      _mm256_storeu_si256((__m256i *)(coeff16 + 64), _mm256_add_epi16(b1, b3));
      _mm256_storeu_si256((__m256i *)(coeff16 + 128), _mm256_sub_epi16(b0, b2));
      _mm256_storeu_si256((__m256i *)(coeff16 + 192), _mm256_sub_epi16(b1, b3));
      coeff16 += 16;
    }
    t_coeff += 16;
  }
}

void aom_hadamard_16x16_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  hadamard_16x16_avx2(src_diff, src_stride, coeff, 1);
}

void aom_hadamard_32x32_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  // For high bitdepths, it is unnecessary to store_tran_low
  // (mult/unpack/store), then load_tran_low (load/pack) the same memory in the
  // next stage.  Output to an intermediate buffer first, then store_tran_low()
  // in the final stage.
  DECLARE_ALIGNED(32, int16_t, temp_coeff[32 * 32]);
  int16_t *t_coeff = temp_coeff;
  int idx;
  for (idx = 0; idx < 4; ++idx) {
    // src_diff: 9 bit, dynamic range [-255, 255]
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 16 * src_stride + (idx & 0x01) * 16;
    hadamard_16x16_avx2(src_ptr, src_stride,
                        (tran_low_t *)(t_coeff + idx * 256), 0);
  }

  for (idx = 0; idx < 256; idx += 16) {
    const __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    const __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 256));
    const __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 512));
    const __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 768));

    __m256i b0 = _mm256_add_epi16(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi16(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi16(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi16(coeff2, coeff3);

    b0 = _mm256_srai_epi16(b0, 2);
    b1 = _mm256_srai_epi16(b1, 2);
    b2 = _mm256_srai_epi16(b2, 2);
    b3 = _mm256_srai_epi16(b3, 2);

    store_tran_low(_mm256_add_epi16(b0, b2), coeff);
    store_tran_low(_mm256_add_epi16(b1, b3), coeff + 256);
    store_tran_low(_mm256_sub_epi16(b0, b2), coeff + 512);
    store_tran_low(_mm256_sub_epi16(b1, b3), coeff + 768);

    coeff += 16;
    t_coeff += 16;
  }
}

int aom_satd_avx2(const tran_low_t *coeff, int length) {
  const __m256i one = _mm256_set1_epi16(1);
  __m256i accum = _mm256_setzero_si256();
  int i;

  for (i = 0; i < length; i += 16) {
    const __m256i src_line = load_tran_low(coeff);
    const __m256i abs = _mm256_abs_epi16(src_line);
    const __m256i sum = _mm256_madd_epi16(abs, one);
    accum = _mm256_add_epi32(accum, sum);
    coeff += 16;
  }

  {  // 32 bit horizontal add
    const __m256i a = _mm256_srli_si256(accum, 8);
    const __m256i b = _mm256_add_epi32(accum, a);
    const __m256i c = _mm256_srli_epi64(b, 32);
    const __m256i d = _mm256_add_epi32(b, c);
    const __m128i accum_128 = _mm_add_epi32(_mm256_castsi256_si128(d),
                                            _mm256_extractf128_si256(d, 1));
    return _mm_cvtsi128_si32(accum_128);
  }
}
