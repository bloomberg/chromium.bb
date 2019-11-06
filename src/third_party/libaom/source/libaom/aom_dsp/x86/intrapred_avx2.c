/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
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
#include "aom_dsp/x86/lpf_common_sse2.h"

static INLINE __m256i dc_sum_64(const uint8_t *ref) {
  const __m256i x0 = _mm256_loadu_si256((const __m256i *)ref);
  const __m256i x1 = _mm256_loadu_si256((const __m256i *)(ref + 32));
  const __m256i zero = _mm256_setzero_si256();
  __m256i y0 = _mm256_sad_epu8(x0, zero);
  __m256i y1 = _mm256_sad_epu8(x1, zero);
  y0 = _mm256_add_epi64(y0, y1);
  __m256i u0 = _mm256_permute2x128_si256(y0, y0, 1);
  y0 = _mm256_add_epi64(u0, y0);
  u0 = _mm256_unpackhi_epi64(y0, y0);
  return _mm256_add_epi16(y0, u0);
}

static INLINE __m256i dc_sum_32(const uint8_t *ref) {
  const __m256i x = _mm256_loadu_si256((const __m256i *)ref);
  const __m256i zero = _mm256_setzero_si256();
  __m256i y = _mm256_sad_epu8(x, zero);
  __m256i u = _mm256_permute2x128_si256(y, y, 1);
  y = _mm256_add_epi64(u, y);
  u = _mm256_unpackhi_epi64(y, y);
  return _mm256_add_epi16(y, u);
}

static INLINE void row_store_32xh(const __m256i *r, int height, uint8_t *dst,
                                  ptrdiff_t stride) {
  for (int i = 0; i < height; ++i) {
    _mm256_storeu_si256((__m256i *)dst, *r);
    dst += stride;
  }
}

static INLINE void row_store_32x2xh(const __m256i *r0, const __m256i *r1,
                                    int height, uint8_t *dst,
                                    ptrdiff_t stride) {
  for (int i = 0; i < height; ++i) {
    _mm256_storeu_si256((__m256i *)dst, *r0);
    _mm256_storeu_si256((__m256i *)(dst + 32), *r1);
    dst += stride;
  }
}

static INLINE void row_store_64xh(const __m256i *r, int height, uint8_t *dst,
                                  ptrdiff_t stride) {
  for (int i = 0; i < height; ++i) {
    _mm256_storeu_si256((__m256i *)dst, *r);
    _mm256_storeu_si256((__m256i *)(dst + 32), *r);
    dst += stride;
  }
}

static INLINE void highbd_transpose16x4_8x8_sse2(__m128i *x, __m128i *d) {
  __m128i r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;

  r0 = _mm_unpacklo_epi16(x[0], x[1]);
  r1 = _mm_unpacklo_epi16(x[2], x[3]);
  r2 = _mm_unpacklo_epi16(x[4], x[5]);
  r3 = _mm_unpacklo_epi16(x[6], x[7]);

  r4 = _mm_unpacklo_epi16(x[8], x[9]);
  r5 = _mm_unpacklo_epi16(x[10], x[11]);
  r6 = _mm_unpacklo_epi16(x[12], x[13]);
  r7 = _mm_unpacklo_epi16(x[14], x[15]);

  r8 = _mm_unpacklo_epi32(r0, r1);
  r9 = _mm_unpackhi_epi32(r0, r1);
  r10 = _mm_unpacklo_epi32(r2, r3);
  r11 = _mm_unpackhi_epi32(r2, r3);

  r12 = _mm_unpacklo_epi32(r4, r5);
  r13 = _mm_unpackhi_epi32(r4, r5);
  r14 = _mm_unpacklo_epi32(r6, r7);
  r15 = _mm_unpackhi_epi32(r6, r7);

  r0 = _mm_unpacklo_epi64(r8, r9);
  r1 = _mm_unpackhi_epi64(r8, r9);
  r2 = _mm_unpacklo_epi64(r10, r11);
  r3 = _mm_unpackhi_epi64(r10, r11);

  r4 = _mm_unpacklo_epi64(r12, r13);
  r5 = _mm_unpackhi_epi64(r12, r13);
  r6 = _mm_unpacklo_epi64(r14, r15);
  r7 = _mm_unpackhi_epi64(r14, r15);

  d[0] = _mm_unpacklo_epi64(r0, r2);
  d[1] = _mm_unpacklo_epi64(r4, r6);
  d[2] = _mm_unpacklo_epi64(r1, r3);
  d[3] = _mm_unpacklo_epi64(r5, r7);

  d[4] = _mm_unpackhi_epi64(r0, r2);
  d[5] = _mm_unpackhi_epi64(r4, r6);
  d[6] = _mm_unpackhi_epi64(r1, r3);
  d[7] = _mm_unpackhi_epi64(r5, r7);
}

static INLINE void highbd_transpose4x16_avx2(__m256i *x, __m256i *d) {
  __m256i w0, w1, w2, w3, ww0, ww1;

  w0 = _mm256_unpacklo_epi16(x[0], x[1]);  // 00 10 01 11 02 12 03 13
  w1 = _mm256_unpacklo_epi16(x[2], x[3]);  // 20 30 21 31 22 32 23 33
  w2 = _mm256_unpackhi_epi16(x[0], x[1]);  // 40 50 41 51 42 52 43 53
  w3 = _mm256_unpackhi_epi16(x[2], x[3]);  // 60 70 61 71 62 72 63 73

  ww0 = _mm256_unpacklo_epi32(w0, w1);  // 00 10 20 30 01 11 21 31
  ww1 = _mm256_unpacklo_epi32(w2, w3);  // 40 50 60 70 41 51 61 71

  d[0] = _mm256_unpacklo_epi64(ww0, ww1);  // 00 10 20 30 40 50 60 70
  d[1] = _mm256_unpackhi_epi64(ww0, ww1);  // 01 11 21 31 41 51 61 71

  ww0 = _mm256_unpackhi_epi32(w0, w1);  // 02 12 22 32 03 13 23 33
  ww1 = _mm256_unpackhi_epi32(w2, w3);  // 42 52 62 72 43 53 63 73

  d[2] = _mm256_unpacklo_epi64(ww0, ww1);  // 02 12 22 32 42 52 62 72
  d[3] = _mm256_unpackhi_epi64(ww0, ww1);  // 03 13 23 33 43 53 63 73
}

static INLINE void highbd_transpose8x16_16x8_avx2(__m256i *x, __m256i *d) {
  __m256i w0, w1, w2, w3, ww0, ww1;

  w0 = _mm256_unpacklo_epi16(x[0], x[1]);  // 00 10 01 11 02 12 03 13
  w1 = _mm256_unpacklo_epi16(x[2], x[3]);  // 20 30 21 31 22 32 23 33
  w2 = _mm256_unpacklo_epi16(x[4], x[5]);  // 40 50 41 51 42 52 43 53
  w3 = _mm256_unpacklo_epi16(x[6], x[7]);  // 60 70 61 71 62 72 63 73

  ww0 = _mm256_unpacklo_epi32(w0, w1);  // 00 10 20 30 01 11 21 31
  ww1 = _mm256_unpacklo_epi32(w2, w3);  // 40 50 60 70 41 51 61 71

  d[0] = _mm256_unpacklo_epi64(ww0, ww1);  // 00 10 20 30 40 50 60 70
  d[1] = _mm256_unpackhi_epi64(ww0, ww1);  // 01 11 21 31 41 51 61 71

  ww0 = _mm256_unpackhi_epi32(w0, w1);  // 02 12 22 32 03 13 23 33
  ww1 = _mm256_unpackhi_epi32(w2, w3);  // 42 52 62 72 43 53 63 73

  d[2] = _mm256_unpacklo_epi64(ww0, ww1);  // 02 12 22 32 42 52 62 72
  d[3] = _mm256_unpackhi_epi64(ww0, ww1);  // 03 13 23 33 43 53 63 73

  w0 = _mm256_unpackhi_epi16(x[0], x[1]);  // 04 14 05 15 06 16 07 17
  w1 = _mm256_unpackhi_epi16(x[2], x[3]);  // 24 34 25 35 26 36 27 37
  w2 = _mm256_unpackhi_epi16(x[4], x[5]);  // 44 54 45 55 46 56 47 57
  w3 = _mm256_unpackhi_epi16(x[6], x[7]);  // 64 74 65 75 66 76 67 77

  ww0 = _mm256_unpacklo_epi32(w0, w1);  // 04 14 24 34 05 15 25 35
  ww1 = _mm256_unpacklo_epi32(w2, w3);  // 44 54 64 74 45 55 65 75

  d[4] = _mm256_unpacklo_epi64(ww0, ww1);  // 04 14 24 34 44 54 64 74
  d[5] = _mm256_unpackhi_epi64(ww0, ww1);  // 05 15 25 35 45 55 65 75

  ww0 = _mm256_unpackhi_epi32(w0, w1);  // 06 16 26 36 07 17 27 37
  ww1 = _mm256_unpackhi_epi32(w2, w3);  // 46 56 66 76 47 57 67 77

  d[6] = _mm256_unpacklo_epi64(ww0, ww1);  // 06 16 26 36 46 56 66 76
  d[7] = _mm256_unpackhi_epi64(ww0, ww1);  // 07 17 27 37 47 57 67 77
}

static INLINE void highbd_transpose16x16_avx2(__m256i *x, __m256i *d) {
  __m256i w0, w1, w2, w3, ww0, ww1;
  __m256i dd[16];
  w0 = _mm256_unpacklo_epi16(x[0], x[1]);
  w1 = _mm256_unpacklo_epi16(x[2], x[3]);
  w2 = _mm256_unpacklo_epi16(x[4], x[5]);
  w3 = _mm256_unpacklo_epi16(x[6], x[7]);

  ww0 = _mm256_unpacklo_epi32(w0, w1);  //
  ww1 = _mm256_unpacklo_epi32(w2, w3);  //

  dd[0] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[1] = _mm256_unpackhi_epi64(ww0, ww1);

  ww0 = _mm256_unpackhi_epi32(w0, w1);  //
  ww1 = _mm256_unpackhi_epi32(w2, w3);  //

  dd[2] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[3] = _mm256_unpackhi_epi64(ww0, ww1);

  w0 = _mm256_unpackhi_epi16(x[0], x[1]);
  w1 = _mm256_unpackhi_epi16(x[2], x[3]);
  w2 = _mm256_unpackhi_epi16(x[4], x[5]);
  w3 = _mm256_unpackhi_epi16(x[6], x[7]);

  ww0 = _mm256_unpacklo_epi32(w0, w1);  //
  ww1 = _mm256_unpacklo_epi32(w2, w3);  //

  dd[4] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[5] = _mm256_unpackhi_epi64(ww0, ww1);

  ww0 = _mm256_unpackhi_epi32(w0, w1);  //
  ww1 = _mm256_unpackhi_epi32(w2, w3);  //

  dd[6] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[7] = _mm256_unpackhi_epi64(ww0, ww1);

  w0 = _mm256_unpacklo_epi16(x[8], x[9]);
  w1 = _mm256_unpacklo_epi16(x[10], x[11]);
  w2 = _mm256_unpacklo_epi16(x[12], x[13]);
  w3 = _mm256_unpacklo_epi16(x[14], x[15]);

  ww0 = _mm256_unpacklo_epi32(w0, w1);
  ww1 = _mm256_unpacklo_epi32(w2, w3);

  dd[8] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[9] = _mm256_unpackhi_epi64(ww0, ww1);

  ww0 = _mm256_unpackhi_epi32(w0, w1);
  ww1 = _mm256_unpackhi_epi32(w2, w3);

  dd[10] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[11] = _mm256_unpackhi_epi64(ww0, ww1);

  w0 = _mm256_unpackhi_epi16(x[8], x[9]);
  w1 = _mm256_unpackhi_epi16(x[10], x[11]);
  w2 = _mm256_unpackhi_epi16(x[12], x[13]);
  w3 = _mm256_unpackhi_epi16(x[14], x[15]);

  ww0 = _mm256_unpacklo_epi32(w0, w1);
  ww1 = _mm256_unpacklo_epi32(w2, w3);

  dd[12] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[13] = _mm256_unpackhi_epi64(ww0, ww1);

  ww0 = _mm256_unpackhi_epi32(w0, w1);
  ww1 = _mm256_unpackhi_epi32(w2, w3);

  dd[14] = _mm256_unpacklo_epi64(ww0, ww1);
  dd[15] = _mm256_unpackhi_epi64(ww0, ww1);

  for (int i = 0; i < 8; i++) {
    d[i] = _mm256_insertf128_si256(dd[i], _mm256_castsi256_si128(dd[i + 8]), 1);
    d[i + 8] = _mm256_insertf128_si256(dd[i + 8],
                                       _mm256_extracti128_si256(dd[i], 1), 0);
  }
}

void aom_dc_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_32(above);
  __m256i sum_left = dc_sum_32(left);
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum_left = _mm256_add_epi16(sum_left, thirtytwo);
  sum_left = _mm256_srai_epi16(sum_left, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum_left, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_32(above);
  (void)left;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m256i sum = dc_sum_32(left);
  (void)above;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_v_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row = _mm256_loadu_si256((const __m256i *)above);
  (void)left;
  row_store_32xh(&row, 32, dst, stride);
}

// There are 32 rows togeter. This function does line:
// 0,1,2,3, and 16,17,18,19. The next call would do
// 4,5,6,7, and 20,21,22,23. So 4 times of calling
// would finish 32 rows.
static INLINE void h_predictor_32x8line(const __m256i *row, uint8_t *dst,
                                        ptrdiff_t stride) {
  __m256i t[4];
  __m256i m = _mm256_setzero_si256();
  const __m256i inc = _mm256_set1_epi8(4);
  int i;

  for (i = 0; i < 4; i++) {
    t[i] = _mm256_shuffle_epi8(*row, m);
    __m256i r0 = _mm256_permute2x128_si256(t[i], t[i], 0);
    __m256i r1 = _mm256_permute2x128_si256(t[i], t[i], 0x11);
    _mm256_storeu_si256((__m256i *)dst, r0);
    _mm256_storeu_si256((__m256i *)(dst + (stride << 4)), r1);
    dst += stride;
    m = _mm256_add_epi8(m, inc);
  }
}

void aom_h_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m256i left_col = _mm256_loadu_si256((__m256i const *)left);

  __m256i u = _mm256_unpacklo_epi8(left_col, left_col);

  __m256i v = _mm256_unpacklo_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  v = _mm256_unpackhi_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  u = _mm256_unpackhi_epi8(left_col, left_col);

  v = _mm256_unpacklo_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  v = _mm256_unpackhi_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
}

// -----------------------------------------------------------------------------
// Rectangle

// TODO(luoyi) The following two functions are shared with intrapred_sse2.c.
// Use a header file, intrapred_common_x86.h
static INLINE __m128i dc_sum_16_sse2(const uint8_t *ref) {
  __m128i x = _mm_load_si128((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_sad_epu8(x, zero);
  const __m128i high = _mm_unpackhi_epi64(x, x);
  return _mm_add_epi16(x, high);
}

static INLINE __m128i dc_sum_32_sse2(const uint8_t *ref) {
  __m128i x0 = _mm_load_si128((__m128i const *)ref);
  __m128i x1 = _mm_load_si128((__m128i const *)(ref + 16));
  const __m128i zero = _mm_setzero_si128();
  x0 = _mm_sad_epu8(x0, zero);
  x1 = _mm_sad_epu8(x1, zero);
  x0 = _mm_add_epi16(x0, x1);
  const __m128i high = _mm_unpackhi_epi64(x0, x0);
  return _mm_add_epi16(x0, high);
}

void aom_dc_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i top_sum = dc_sum_32_sse2(above);
  __m128i left_sum = dc_sum_16_sse2(left);
  left_sum = _mm_add_epi16(top_sum, left_sum);
  uint16_t sum = _mm_cvtsi128_si32(left_sum);
  sum += 24;
  sum /= 48;
  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_32(above);
  __m256i sum_left = dc_sum_64(left);
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  uint16_t sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_left));
  sum += 48;
  sum /= 96;
  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_32xh(&row, 64, dst, stride);
}

void aom_dc_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_64(above);
  __m256i sum_left = dc_sum_64(left);
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  uint16_t sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_left));
  sum += 64;
  sum /= 128;
  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_64xh(&row, 64, dst, stride);
}

void aom_dc_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_64(above);
  __m256i sum_left = dc_sum_32(left);
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  uint16_t sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_left));
  sum += 48;
  sum /= 96;
  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_64xh(&row, 32, dst, stride);
}

void aom_dc_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_64(above);
  __m256i sum_left = _mm256_castsi128_si256(dc_sum_16_sse2(left));
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  uint16_t sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_left));
  sum += 40;
  sum /= 80;
  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_64xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_32(above);
  (void)left;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_32(above);
  (void)left;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 64, dst, stride);
}

void aom_dc_top_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_64(above);
  (void)left;

  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum = _mm256_add_epi16(sum, thirtytwo);
  sum = _mm256_srai_epi16(sum, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_64xh(&row, 64, dst, stride);
}

void aom_dc_top_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_64(above);
  (void)left;

  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum = _mm256_add_epi16(sum, thirtytwo);
  sum = _mm256_srai_epi16(sum, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_64xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_64(above);
  (void)left;

  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum = _mm256_add_epi16(sum, thirtytwo);
  sum = _mm256_srai_epi16(sum, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_64xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i sum = dc_sum_16_sse2(left);
  (void)above;

  const __m128i eight = _mm_set1_epi16(8);
  sum = _mm_add_epi16(sum, eight);
  sum = _mm_srai_epi16(sum, 4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i r = _mm_shuffle_epi8(sum, zero);
  const __m256i row = _mm256_inserti128_si256(_mm256_castsi128_si256(r), r, 1);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m256i sum = dc_sum_64(left);
  (void)above;

  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum = _mm256_add_epi16(sum, thirtytwo);
  sum = _mm256_srai_epi16(sum, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 64, dst, stride);
}

void aom_dc_left_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m256i sum = dc_sum_64(left);
  (void)above;

  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum = _mm256_add_epi16(sum, thirtytwo);
  sum = _mm256_srai_epi16(sum, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_64xh(&row, 64, dst, stride);
}

void aom_dc_left_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m256i sum = dc_sum_32(left);
  (void)above;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_64xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i sum = dc_sum_16_sse2(left);
  (void)above;

  const __m128i eight = _mm_set1_epi16(8);
  sum = _mm_add_epi16(sum, eight);
  sum = _mm_srai_epi16(sum, 4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i r = _mm_shuffle_epi8(sum, zero);
  const __m256i row = _mm256_inserti128_si256(_mm256_castsi128_si256(r), r, 1);
  row_store_64xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_32xh(&row, 64, dst, stride);
}

void aom_dc_128_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_64xh(&row, 64, dst, stride);
}

void aom_dc_128_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_64xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_64xh(&row, 16, dst, stride);
}

void aom_v_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row = _mm256_loadu_si256((const __m256i *)above);
  (void)left;
  row_store_32xh(&row, 16, dst, stride);
}

void aom_v_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row = _mm256_loadu_si256((const __m256i *)above);
  (void)left;
  row_store_32xh(&row, 64, dst, stride);
}

void aom_v_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row0 = _mm256_loadu_si256((const __m256i *)above);
  const __m256i row1 = _mm256_loadu_si256((const __m256i *)(above + 32));
  (void)left;
  row_store_32x2xh(&row0, &row1, 64, dst, stride);
}

void aom_v_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row0 = _mm256_loadu_si256((const __m256i *)above);
  const __m256i row1 = _mm256_loadu_si256((const __m256i *)(above + 32));
  (void)left;
  row_store_32x2xh(&row0, &row1, 32, dst, stride);
}

void aom_v_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row0 = _mm256_loadu_si256((const __m256i *)above);
  const __m256i row1 = _mm256_loadu_si256((const __m256i *)(above + 32));
  (void)left;
  row_store_32x2xh(&row0, &row1, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// PAETH_PRED

// Return 16 16-bit pixels in one row (__m256i)
static INLINE __m256i paeth_pred(const __m256i *left, const __m256i *top,
                                 const __m256i *topleft) {
  const __m256i base =
      _mm256_sub_epi16(_mm256_add_epi16(*top, *left), *topleft);

  __m256i pl = _mm256_abs_epi16(_mm256_sub_epi16(base, *left));
  __m256i pt = _mm256_abs_epi16(_mm256_sub_epi16(base, *top));
  __m256i ptl = _mm256_abs_epi16(_mm256_sub_epi16(base, *topleft));

  __m256i mask1 = _mm256_cmpgt_epi16(pl, pt);
  mask1 = _mm256_or_si256(mask1, _mm256_cmpgt_epi16(pl, ptl));
  __m256i mask2 = _mm256_cmpgt_epi16(pt, ptl);

  pl = _mm256_andnot_si256(mask1, *left);

  ptl = _mm256_and_si256(mask2, *topleft);
  pt = _mm256_andnot_si256(mask2, *top);
  pt = _mm256_or_si256(pt, ptl);
  pt = _mm256_and_si256(mask1, pt);

  return _mm256_or_si256(pt, pl);
}

// Return 16 8-bit pixels in one row (__m128i)
static INLINE __m128i paeth_16x1_pred(const __m256i *left, const __m256i *top,
                                      const __m256i *topleft) {
  const __m256i p0 = paeth_pred(left, top, topleft);
  const __m256i p1 = _mm256_permute4x64_epi64(p0, 0xe);
  const __m256i p = _mm256_packus_epi16(p0, p1);
  return _mm256_castsi256_si128(p);
}

static INLINE __m256i get_top_vector(const uint8_t *above) {
  const __m128i x = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t0 = _mm_unpacklo_epi8(x, zero);
  const __m128i t1 = _mm_unpackhi_epi8(x, zero);
  return _mm256_inserti128_si256(_mm256_castsi128_si256(t0), t1, 1);
}

void aom_paeth_predictor_16x8_avx2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i x = _mm_loadl_epi64((const __m128i *)left);
  const __m256i l = _mm256_inserti128_si256(_mm256_castsi128_si256(x), x, 1);
  const __m256i tl16 = _mm256_set1_epi16((uint16_t)above[-1]);
  __m256i rep = _mm256_set1_epi16(0x8000);
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i top = get_top_vector(above);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

static INLINE __m256i get_left_vector(const uint8_t *left) {
  const __m128i x = _mm_load_si128((const __m128i *)left);
  return _mm256_inserti128_si256(_mm256_castsi128_si256(x), x, 1);
}

void aom_paeth_predictor_16x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i l = get_left_vector(left);
  const __m256i tl16 = _mm256_set1_epi16((uint16_t)above[-1]);
  __m256i rep = _mm256_set1_epi16(0x8000);
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i top = get_top_vector(above);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m256i l = get_left_vector(left);
  const __m256i tl16 = _mm256_set1_epi16((uint16_t)above[-1]);
  __m256i rep = _mm256_set1_epi16(0x8000);
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i top = get_top_vector(above);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }

  l = get_left_vector(left + 16);
  rep = _mm256_set1_epi16(0x8000);
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i tl16 = _mm256_set1_epi16((uint16_t)above[-1]);
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i top = get_top_vector(above);

  for (int j = 0; j < 4; ++j) {
    const __m256i l = get_left_vector(left + j * 16);
    __m256i rep = _mm256_set1_epi16(0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m256i l16 = _mm256_shuffle_epi8(l, rep);
      const __m128i row = paeth_16x1_pred(&l16, &top, &tl16);

      _mm_store_si128((__m128i *)dst, row);
      dst += stride;
      rep = _mm256_add_epi16(rep, one);
    }
  }
}

// Return 32 8-bit pixels in one row (__m256i)
static INLINE __m256i paeth_32x1_pred(const __m256i *left, const __m256i *top0,
                                      const __m256i *top1,
                                      const __m256i *topleft) {
  __m256i p0 = paeth_pred(left, top0, topleft);
  __m256i p1 = _mm256_permute4x64_epi64(p0, 0xe);
  const __m256i x0 = _mm256_packus_epi16(p0, p1);

  p0 = paeth_pred(left, top1, topleft);
  p1 = _mm256_permute4x64_epi64(p0, 0xe);
  const __m256i x1 = _mm256_packus_epi16(p0, p1);

  return _mm256_permute2x128_si256(x0, x1, 0x20);
}

void aom_paeth_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i l = get_left_vector(left);
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  __m256i rep = _mm256_set1_epi16(0x8000);
  const __m256i one = _mm256_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);

    const __m256i r = paeth_32x1_pred(&l16, &t0, &t1, &tl);

    _mm256_storeu_si256((__m256i *)dst, r);

    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m256i l = get_left_vector(left);
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  __m256i rep = _mm256_set1_epi16(0x8000);
  const __m256i one = _mm256_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);

    const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
    const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);

    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }

  l = get_left_vector(left + 16);
  rep = _mm256_set1_epi16(0x8000);
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);

    const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
    const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);

    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  const __m256i one = _mm256_set1_epi16(1);

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m256i l = get_left_vector(left + j * 16);
    __m256i rep = _mm256_set1_epi16(0x8000);
    for (i = 0; i < 16; ++i) {
      const __m256i l16 = _mm256_shuffle_epi8(l, rep);

      const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
      const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);

      dst += stride;
      rep = _mm256_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i t2 = get_top_vector(above + 32);
  const __m256i t3 = get_top_vector(above + 48);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  const __m256i one = _mm256_set1_epi16(1);

  int i, j;
  for (j = 0; j < 2; ++j) {
    const __m256i l = get_left_vector(left + j * 16);
    __m256i rep = _mm256_set1_epi16(0x8000);
    for (i = 0; i < 16; ++i) {
      const __m256i l16 = _mm256_shuffle_epi8(l, rep);

      const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
      const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);
      const __m128i r2 = paeth_16x1_pred(&l16, &t2, &tl);
      const __m128i r3 = paeth_16x1_pred(&l16, &t3, &tl);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);

      dst += stride;
      rep = _mm256_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i t2 = get_top_vector(above + 32);
  const __m256i t3 = get_top_vector(above + 48);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  const __m256i one = _mm256_set1_epi16(1);

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m256i l = get_left_vector(left + j * 16);
    __m256i rep = _mm256_set1_epi16(0x8000);
    for (i = 0; i < 16; ++i) {
      const __m256i l16 = _mm256_shuffle_epi8(l, rep);

      const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
      const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);
      const __m128i r2 = paeth_16x1_pred(&l16, &t2, &tl);
      const __m128i r3 = paeth_16x1_pred(&l16, &t3, &tl);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);

      dst += stride;
      rep = _mm256_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m256i t0 = get_top_vector(above);
  const __m256i t1 = get_top_vector(above + 16);
  const __m256i t2 = get_top_vector(above + 32);
  const __m256i t3 = get_top_vector(above + 48);
  const __m256i tl = _mm256_set1_epi16((uint16_t)above[-1]);
  const __m256i one = _mm256_set1_epi16(1);

  int i;
  const __m256i l = get_left_vector(left);
  __m256i rep = _mm256_set1_epi16(0x8000);
  for (i = 0; i < 16; ++i) {
    const __m256i l16 = _mm256_shuffle_epi8(l, rep);

    const __m128i r0 = paeth_16x1_pred(&l16, &t0, &tl);
    const __m128i r1 = paeth_16x1_pred(&l16, &t1, &tl);
    const __m128i r2 = paeth_16x1_pred(&l16, &t2, &tl);
    const __m128i r3 = paeth_16x1_pred(&l16, &t3, &tl);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);
    _mm_store_si128((__m128i *)(dst + 32), r2);
    _mm_store_si128((__m128i *)(dst + 48), r3);

    dst += stride;
    rep = _mm256_add_epi16(rep, one);
  }
}

#define PERM4x64(c0, c1, c2, c3) c0 + (c1 << 2) + (c2 << 4) + (c3 << 6)
#define PERM2x128(c0, c1) c0 + (c1 << 4)

static AOM_FORCE_INLINE void highbd_dr_prediction_z1_4xN_internal_avx2(
    int N, __m128i *dst, const uint16_t *above, int upsample_above, int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((N + 4) - 1) << upsample_above;
  int x;
  // a assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a1, a32, a16;
  __m256i diff;
  __m128i a_mbase_x, max_base_x128, base_inc128, mask128;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm_set1_epi16(above[max_base_x]);
  max_base_x128 = _mm_set1_epi32(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i res1;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        dst[i] = a_mbase_x;  // save 4 values
      }
      return;
    }

    a0 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base)));
    a1 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 1)));

    if (upsample_above) {
      a0 = _mm256_permutevar8x32_epi32(
          a0, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0, 1));
      base_inc128 = _mm_setr_epi32(base, base + 2, base + 4, base + 6);
      shift = _mm256_srli_epi32(
          _mm256_and_si256(
              _mm256_slli_epi32(_mm256_set1_epi32(x), upsample_above),
              _mm256_set1_epi32(0x3f)),
          1);
    } else {
      base_inc128 = _mm_setr_epi32(base, base + 1, base + 2, base + 3);
      shift = _mm256_srli_epi32(
          _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);
    }

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16

    b = _mm256_mullo_epi32(diff, shift);
    res = _mm256_add_epi32(a32, b);
    res = _mm256_srli_epi32(res, 5);

    res1 = _mm256_castsi256_si128(res);
    res1 = _mm_packus_epi32(res1, res1);

    mask128 = _mm_cmpgt_epi32(max_base_x128, base_inc128);
    mask128 = _mm_packs_epi32(mask128, mask128);  // goto 16 bit
    dst[r] = _mm_blendv_epi8(a_mbase_x, res1, mask128);
    x += dx;
  }
}

static void highbd_dr_prediction_z1_4xN_avx2(int N, uint16_t *dst,
                                             ptrdiff_t stride,
                                             const uint16_t *above,
                                             int upsample_above, int dx) {
  __m128i dstvec[16];

  highbd_dr_prediction_z1_4xN_internal_avx2(N, dstvec, above, upsample_above,
                                            dx);
  for (int i = 0; i < N; i++) {
    _mm_storel_epi64((__m128i *)(dst + stride * i), dstvec[i]);
  }
}

static AOM_FORCE_INLINE void highbd_dr_prediction_z1_8xN_internal_avx2(
    int N, __m128i *dst, const uint16_t *above, int upsample_above, int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((8 + N) - 1) << upsample_above;

  int x;
  // a assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a1, a0_1, a1_1, a32, a16;
  __m256i a_mbase_x, diff, max_base_x256, base_inc256, mask256;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi16(above[max_base_x]);
  max_base_x256 = _mm256_set1_epi32(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res, res1, shift;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        dst[i] = _mm256_castsi256_si128(a_mbase_x);  // save 8 values
      }
      return;
    }

    a0 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base)));
    a1 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 1)));

    if (upsample_above) {
      a0 = _mm256_permutevar8x32_epi32(
          a0, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0, 1));

      a0_1 =
          _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 8)));
      a0_1 = _mm256_permutevar8x32_epi32(
          a0_1, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1_1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0_1, 1));

      a0 = _mm256_inserti128_si256(a0, _mm256_castsi256_si128(a0_1), 1);
      a1 = _mm256_inserti128_si256(a1, _mm256_castsi256_si128(a1_1), 1);
      base_inc256 =
          _mm256_setr_epi32(base, base + 2, base + 4, base + 6, base + 8,
                            base + 10, base + 12, base + 14);
      shift = _mm256_srli_epi32(
          _mm256_and_si256(
              _mm256_slli_epi32(_mm256_set1_epi32(x), upsample_above),
              _mm256_set1_epi32(0x3f)),
          1);
    } else {
      base_inc256 = _mm256_setr_epi32(base, base + 1, base + 2, base + 3,
                                      base + 4, base + 5, base + 6, base + 7);
      shift = _mm256_srli_epi32(
          _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);
    }

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16

    b = _mm256_mullo_epi32(diff, shift);
    res = _mm256_add_epi32(a32, b);
    res = _mm256_srli_epi32(res, 5);

    res1 = _mm256_packus_epi32(
        res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));

    mask256 = _mm256_cmpgt_epi32(max_base_x256, base_inc256);
    mask256 = _mm256_packs_epi32(
        mask256, _mm256_castsi128_si256(
                     _mm256_extracti128_si256(mask256, 1)));  // goto 16 bit
    res1 = _mm256_blendv_epi8(a_mbase_x, res1, mask256);
    dst[r] = _mm256_castsi256_si128(res1);
    x += dx;
  }
}

static void highbd_dr_prediction_z1_8xN_avx2(int N, uint16_t *dst,
                                             ptrdiff_t stride,
                                             const uint16_t *above,
                                             int upsample_above, int dx) {
  __m128i dstvec[32];

  highbd_dr_prediction_z1_8xN_internal_avx2(N, dstvec, above, upsample_above,
                                            dx);
  for (int i = 0; i < N; i++) {
    _mm_storeu_si128((__m128i *)(dst + stride * i), dstvec[i]);
  }
}

static AOM_FORCE_INLINE void highbd_dr_prediction_z1_16xN_internal_avx2(
    int N, __m256i *dstvec, const uint16_t *above, int upsample_above, int dx) {
  int x;
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((16 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, a32, a16;
  __m256i a_mbase_x, diff, max_base_x256, base_inc256, mask256;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi16(above[max_base_x]);
  max_base_x256 = _mm256_set1_epi16(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res[2], res1;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        dstvec[i] = a_mbase_x;  // save 16 values
      }
      return;
    }
    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    a0 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base)));
    a1 = _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 1)));

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
    b = _mm256_mullo_epi32(diff, shift);

    res[0] = _mm256_add_epi32(a32, b);
    res[0] = _mm256_srli_epi32(res[0], 5);
    res[0] = _mm256_packus_epi32(
        res[0], _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));

    int mdif = max_base_x - base;
    if (mdif > 8) {
      a0_1 =
          _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 8)));
      a1_1 =
          _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *)(above + base + 9)));

      diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
      b = _mm256_mullo_epi32(diff, shift);

      res[1] = _mm256_add_epi32(a32, b);
      res[1] = _mm256_srli_epi32(res[1], 5);
      res[1] = _mm256_packus_epi32(
          res[1], _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
    } else {
      res[1] = a_mbase_x;
    }
    res1 = _mm256_inserti128_si256(res[0], _mm256_castsi256_si128(res[1]),
                                   1);  // 16 16bit values

    base_inc256 = _mm256_setr_epi16(base, base + 1, base + 2, base + 3,
                                    base + 4, base + 5, base + 6, base + 7,
                                    base + 8, base + 9, base + 10, base + 11,
                                    base + 12, base + 13, base + 14, base + 15);
    mask256 = _mm256_cmpgt_epi16(max_base_x256, base_inc256);
    dstvec[r] = _mm256_blendv_epi8(a_mbase_x, res1, mask256);
    x += dx;
  }
}

static void highbd_dr_prediction_z1_16xN_avx2(int N, uint16_t *dst,
                                              ptrdiff_t stride,
                                              const uint16_t *above,
                                              int upsample_above, int dx) {
  __m256i dstvec[64];
  highbd_dr_prediction_z1_16xN_internal_avx2(N, dstvec, above, upsample_above,
                                             dx);
  for (int i = 0; i < N; i++) {
    _mm256_storeu_si256((__m256i *)(dst + stride * i), dstvec[i]);
  }
}

static AOM_FORCE_INLINE void highbd_dr_prediction_z1_32xN_internal_avx2(
    int N, __m256i *dstvec, const uint16_t *above, int upsample_above, int dx) {
  int x;
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((32 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, a32, a16;
  __m256i a_mbase_x, diff, max_base_x256, base_inc256, mask256;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi16(above[max_base_x]);
  max_base_x256 = _mm256_set1_epi16(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res[2], res1;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        dstvec[i] = a_mbase_x;  // save 32 values
        dstvec[i + N] = a_mbase_x;
      }
      return;
    }

    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    for (int j = 0; j < 32; j += 16) {
      int mdif = max_base_x - (base + j);
      if (mdif <= 0) {
        res1 = a_mbase_x;
      } else {
        a0 = _mm256_cvtepu16_epi32(
            _mm_loadu_si128((__m128i *)(above + base + j)));
        a1 = _mm256_cvtepu16_epi32(
            _mm_loadu_si128((__m128i *)(above + base + 1 + j)));

        diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        res[0] = _mm256_add_epi32(a32, b);
        res[0] = _mm256_srli_epi32(res[0], 5);
        res[0] = _mm256_packus_epi32(
            res[0],
            _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));
        if (mdif > 8) {
          a0_1 = _mm256_cvtepu16_epi32(
              _mm_loadu_si128((__m128i *)(above + base + 8 + j)));
          a1_1 = _mm256_cvtepu16_epi32(
              _mm_loadu_si128((__m128i *)(above + base + 9 + j)));

          diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
          a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
          a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
          b = _mm256_mullo_epi32(diff, shift);

          res[1] = _mm256_add_epi32(a32, b);
          res[1] = _mm256_srli_epi32(res[1], 5);
          res[1] = _mm256_packus_epi32(
              res[1],
              _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
        } else {
          res[1] = a_mbase_x;
        }
        res1 = _mm256_inserti128_si256(res[0], _mm256_castsi256_si128(res[1]),
                                       1);  // 16 16bit values
        base_inc256 = _mm256_setr_epi16(
            base + j, base + j + 1, base + j + 2, base + j + 3, base + j + 4,
            base + j + 5, base + j + 6, base + j + 7, base + j + 8,
            base + j + 9, base + j + 10, base + j + 11, base + j + 12,
            base + j + 13, base + j + 14, base + j + 15);

        mask256 = _mm256_cmpgt_epi16(max_base_x256, base_inc256);
        res1 = _mm256_blendv_epi8(a_mbase_x, res1, mask256);
      }
      if (!j)
        dstvec[r] = res1;
      else
        dstvec[r + N] = res1;
    }
    x += dx;
  }
}

static void highbd_dr_prediction_z1_32xN_avx2(int N, uint16_t *dst,
                                              ptrdiff_t stride,
                                              const uint16_t *above,
                                              int upsample_above, int dx) {
  __m256i dstvec[128];

  highbd_dr_prediction_z1_32xN_internal_avx2(N, dstvec, above, upsample_above,
                                             dx);
  for (int i = 0; i < N; i++) {
    _mm256_storeu_si256((__m256i *)(dst + stride * i), dstvec[i]);
    _mm256_storeu_si256((__m256i *)(dst + stride * i + 16), dstvec[i + N]);
  }
}

static void highbd_dr_prediction_z1_64xN_avx2(int N, uint16_t *dst,
                                              ptrdiff_t stride,
                                              const uint16_t *above,
                                              int upsample_above, int dx) {
  int x;

  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((64 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, a32, a16;
  __m256i a_mbase_x, diff, max_base_x256, base_inc256, mask256;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi16(above[max_base_x]);
  max_base_x256 = _mm256_set1_epi16(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++, dst += stride) {
    __m256i b, res[2], res1;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        _mm256_storeu_si256((__m256i *)dst, a_mbase_x);  // save 32 values
        _mm256_storeu_si256((__m256i *)(dst + 16), a_mbase_x);
        _mm256_storeu_si256((__m256i *)(dst + 32), a_mbase_x);
        _mm256_storeu_si256((__m256i *)(dst + 48), a_mbase_x);
        dst += stride;
      }
      return;
    }

    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    __m128i a0_128, a0_1_128, a1_128, a1_1_128;
    for (int j = 0; j < 64; j += 16) {
      int mdif = max_base_x - (base + j);
      if (mdif <= 0) {
        _mm256_storeu_si256((__m256i *)(dst + j), a_mbase_x);
      } else {
        a0_128 = _mm_loadu_si128((__m128i *)(above + base + j));
        a1_128 = _mm_loadu_si128((__m128i *)(above + base + 1 + j));
        a0 = _mm256_cvtepu16_epi32(a0_128);
        a1 = _mm256_cvtepu16_epi32(a1_128);

        diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        res[0] = _mm256_add_epi32(a32, b);
        res[0] = _mm256_srli_epi32(res[0], 5);
        res[0] = _mm256_packus_epi32(
            res[0],
            _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));
        if (mdif > 8) {
          a0_1_128 = _mm_loadu_si128((__m128i *)(above + base + 8 + j));
          a1_1_128 = _mm_loadu_si128((__m128i *)(above + base + 9 + j));
          a0_1 = _mm256_cvtepu16_epi32(a0_1_128);
          a1_1 = _mm256_cvtepu16_epi32(a1_1_128);

          diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
          a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
          a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
          b = _mm256_mullo_epi32(diff, shift);

          res[1] = _mm256_add_epi32(a32, b);
          res[1] = _mm256_srli_epi32(res[1], 5);
          res[1] = _mm256_packus_epi32(
              res[1],
              _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
        } else {
          res[1] = a_mbase_x;
        }
        res1 = _mm256_inserti128_si256(res[0], _mm256_castsi256_si128(res[1]),
                                       1);  // 16 16bit values
        base_inc256 = _mm256_setr_epi16(
            base + j, base + j + 1, base + j + 2, base + j + 3, base + j + 4,
            base + j + 5, base + j + 6, base + j + 7, base + j + 8,
            base + j + 9, base + j + 10, base + j + 11, base + j + 12,
            base + j + 13, base + j + 14, base + j + 15);

        mask256 = _mm256_cmpgt_epi16(max_base_x256, base_inc256);
        res1 = _mm256_blendv_epi8(a_mbase_x, res1, mask256);
        _mm256_storeu_si256((__m256i *)(dst + j), res1);
      }
    }
    x += dx;
  }
}

// Directional prediction, zone 1: 0 < angle < 90
void av1_highbd_dr_prediction_z1_avx2(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_above,
                                      int dx, int dy, int bd) {
  (void)left;
  (void)dy;
  (void)bd;

  switch (bw) {
    case 4:
      highbd_dr_prediction_z1_4xN_avx2(bh, dst, stride, above, upsample_above,
                                       dx);
      break;
    case 8:
      highbd_dr_prediction_z1_8xN_avx2(bh, dst, stride, above, upsample_above,
                                       dx);
      break;
    case 16:
      highbd_dr_prediction_z1_16xN_avx2(bh, dst, stride, above, upsample_above,
                                        dx);
      break;
    case 32:
      highbd_dr_prediction_z1_32xN_avx2(bh, dst, stride, above, upsample_above,
                                        dx);
      break;
    case 64:
      highbd_dr_prediction_z1_64xN_avx2(bh, dst, stride, above, upsample_above,
                                        dx);
      break;
    default: break;
  }
  return;
}

static void highbd_transpose_TX_8X8(const uint16_t *src, ptrdiff_t pitchSrc,
                                    uint16_t *dst, ptrdiff_t pitchDst) {
  __m128i r0, r1, r2, r3, r4, r5, r6, r7, r0_Lo, r1_Lo, r2_Lo, r3_Lo, r4_Lo,
      r5_Lo, r6_Lo;
  r0 = _mm_load_si128(
      (__m128i *)(src + 0 * pitchSrc));  // 07,06,05,04,03,02,01,00
  r1 = _mm_load_si128(
      (__m128i *)(src + 1 * pitchSrc));  // 17,16,15,14,13,12,11,10
  r2 = _mm_load_si128(
      (__m128i *)(src + 2 * pitchSrc));  // 27,26,25,24,23,22,21,20
  r3 = _mm_load_si128(
      (__m128i *)(src + 3 * pitchSrc));  // 37,36,35,34,33,32,31,30
  r4 = _mm_load_si128(
      (__m128i *)(src + 4 * pitchSrc));  // 47,46,45,44,43,42,41,40
  r5 = _mm_load_si128(
      (__m128i *)(src + 5 * pitchSrc));  // 57,56,55,54,53,52,51,50
  r6 = _mm_load_si128(
      (__m128i *)(src + 6 * pitchSrc));  // 67,66,65,64,63,62,61,60
  r7 = _mm_load_si128(
      (__m128i *)(src + 7 * pitchSrc));  // 77,76,75,74,73,72,71,70

  r0_Lo = _mm_unpacklo_epi16(r0, r1);
  r2_Lo = _mm_unpacklo_epi16(r2, r3);
  r4_Lo = _mm_unpacklo_epi16(r4, r5);
  r6_Lo = _mm_unpacklo_epi16(r6, r7);

  r1_Lo = r0_Lo;
  r0_Lo = _mm_unpacklo_epi32(r0_Lo, r2_Lo);
  r1_Lo = _mm_unpackhi_epi32(r1_Lo, r2_Lo);
  r5_Lo = r4_Lo;
  r4_Lo = _mm_unpacklo_epi32(r4_Lo, r6_Lo);
  r5_Lo = _mm_unpackhi_epi32(r5_Lo, r6_Lo);
  r2_Lo = r0_Lo;
  r0_Lo = _mm_unpacklo_epi64(r0_Lo, r4_Lo);  // 64
  r2_Lo = _mm_unpackhi_epi64(r2_Lo, r4_Lo);
  r3_Lo = r1_Lo;
  r1_Lo = _mm_unpacklo_epi64(r1_Lo, r5_Lo);
  r3_Lo = _mm_unpackhi_epi64(r3_Lo, r5_Lo);

  _mm_storeu_si128((__m128i *)(dst + 0 * pitchDst), r0_Lo);
  _mm_storeu_si128((__m128i *)(dst + 1 * pitchDst), r2_Lo);
  _mm_storeu_si128((__m128i *)(dst + 2 * pitchDst), r1_Lo);
  _mm_storeu_si128((__m128i *)(dst + 3 * pitchDst), r3_Lo);

  r0 = _mm_unpackhi_epi16(r0, r1);
  r2 = _mm_unpackhi_epi16(r2, r3);
  r4 = _mm_unpackhi_epi16(r4, r5);
  r6 = _mm_unpackhi_epi16(r6, r7);

  r1 = r0;
  r0 = _mm_unpacklo_epi32(r0, r2);
  r1 = _mm_unpackhi_epi32(r1, r2);
  r5 = r4;
  r4 = _mm_unpacklo_epi32(r4, r6);
  r5 = _mm_unpackhi_epi32(r5, r6);
  r2 = r0;
  r0 = _mm_unpacklo_epi64(r0, r4);
  r2 = _mm_unpackhi_epi64(r2, r4);
  r3 = r1;
  r1 = _mm_unpacklo_epi64(r1, r5);
  r3 = _mm_unpackhi_epi64(r3, r5);

  _mm_storeu_si128((__m128i *)(dst + 4 * pitchDst), r0);
  _mm_storeu_si128((__m128i *)(dst + 5 * pitchDst), r2);
  _mm_storeu_si128((__m128i *)(dst + 6 * pitchDst), r1);
  _mm_storeu_si128((__m128i *)(dst + 7 * pitchDst), r3);
}

static uint8_t HighbdLoadMaskx[8][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 },
  { 0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 4, 5 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
};

static uint8_t HighbdEvenOddMaskx4[8][16] = {
  { 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14,
    15 },  // 0=0,1, 1=2,3, 2=4,5, 3=6,7, 4=8,9, 5=10,11, 6=12,13, 7=14,15,
           // >7=0,1
  { 0, 1, 2, 3, 6, 7, 10, 11, 14, 15, 4, 5, 8, 9, 12, 13 },
  { 0, 1, 0, 1, 4, 5, 8, 9, 12, 13, 0, 1, 6, 7, 10, 11 },
  { 0, 1, 0, 1, 0, 1, 6, 7, 10, 11, 14, 15, 0, 1, 8, 9 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 8, 9, 12, 13, 0, 1, 0, 1 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 10, 11, 14, 15, 0, 1 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 12, 13, 0, 1 },
  { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 14, 15 }
};

static uint16_t HighbdEvenOddMaskx8_2[8][16] = {
  { 0, 2, 4, 6, 8, 10, 12, 14 },      { 2, 2, 4, 6, 8, 10, 12, 14 },
  { 4, 4, 4, 6, 8, 10, 12, 14 },      { 6, 6, 6, 6, 8, 10, 12, 14 },
  { 8, 8, 8, 8, 8, 10, 12, 14 },      { 10, 10, 10, 10, 10, 10, 12, 14 },
  { 12, 12, 12, 12, 12, 12, 12, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
};

static uint16_t HighbdBaseMask[17][16] = {
  {
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  },
  { 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0,
    0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0,
    0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0,
    0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0 },
  { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff }
};

static void highbd_dr_prediction_z2_Nx4_avx2(
    int N, uint16_t *dst, ptrdiff_t stride, const uint16_t *above,
    const uint16_t *left, int upsample_above, int upsample_left, int dx,
    int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  // a assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a16;
  __m256i diff;
  __m128i c3f, min_base_y128;

  a16 = _mm256_set1_epi32(16);
  c3f = _mm_set1_epi32(0x3f);
  min_base_y128 = _mm_set1_epi32(min_base_y);

  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i resx, resy, resxy;
    __m128i a0_x128, a1_x128;
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 4) {
      base_min_diff = 4;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    if (base_shift > 3) {
      resx = _mm_setzero_si128();
    } else {
      a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift));
      a1_x128 = _mm_loadu_si128((__m128i *)(above + base_x + 1 + base_shift));

      if (upsample_above) {
        a0_x128 = _mm_shuffle_epi8(a0_x128,
                                   *(__m128i *)HighbdEvenOddMaskx4[base_shift]);
        a1_x128 = _mm_shuffle_epi8(a1_x128,
                                   *(__m128i *)HighbdEvenOddMaskx4[base_shift]);
        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(
                _mm_slli_epi32(
                    _mm_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                   (2 << 6) - y * dx, (3 << 6) - y * dx),
                    upsample_above),
                c3f),
            1));
      } else {
        a0_x128 =
            _mm_shuffle_epi8(a0_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);
        a1_x128 =
            _mm_shuffle_epi8(a1_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);
        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(_mm_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                         (2 << 6) - y * dx, (3 << 6) - y * dx),
                          c3f),
            1));
      }
      a0_x = _mm256_cvtepu16_epi32(a0_x128);
      a1_x = _mm256_cvtepu16_epi32(a1_x128);

      diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resx = _mm256_castsi256_si128(res);
      resx = _mm_packus_epi32(resx, resx);
    }
    // y calc
    if (base_x < min_base_x) {
      DECLARE_ALIGNED(32, int, base_y_c[4]);
      __m128i r6, c1234, dy128, y_c128, base_y_c128, mask128;
      r6 = _mm_set1_epi32(r << 6);
      dy128 = _mm_set1_epi32(dy);
      c1234 = _mm_setr_epi32(1, 2, 3, 4);
      y_c128 = _mm_sub_epi32(r6, _mm_mullo_epi32(c1234, dy128));
      base_y_c128 = _mm_srai_epi32(y_c128, frac_bits_y);
      mask128 = _mm_cmpgt_epi32(min_base_y128, base_y_c128);
      base_y_c128 = _mm_andnot_si128(mask128, base_y_c128);
      _mm_store_si128((__m128i *)base_y_c, base_y_c128);

      a0_y = _mm256_castsi128_si256(
          _mm_setr_epi32(left[base_y_c[0]], left[base_y_c[1]],
                         left[base_y_c[2]], left[base_y_c[3]]));
      a1_y = _mm256_castsi128_si256(
          _mm_setr_epi32(left[base_y_c[0] + 1], left[base_y_c[1] + 1],
                         left[base_y_c[2] + 1], left[base_y_c[3] + 1]));

      if (upsample_left) {
        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(_mm_slli_epi32(y_c128, upsample_left), c3f), 1));
      } else {
        shift = _mm256_castsi128_si256(
            _mm_srli_epi32(_mm_and_si128(y_c128, c3f), 1));
      }
      diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resy = _mm256_castsi256_si128(res);
      resy = _mm_packus_epi32(resy, resy);
    } else {
      resy = resx;
    }
    resxy =
        _mm_blendv_epi8(resx, resy, *(__m128i *)HighbdBaseMask[base_min_diff]);
    _mm_storel_epi64((__m128i *)(dst), resxy);
    dst += stride;
  }
}

static void highbd_dr_prediction_z2_Nx8_avx2(
    int N, uint16_t *dst, ptrdiff_t stride, const uint16_t *above,
    const uint16_t *left, int upsample_above, int upsample_left, int dx,
    int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a16, c3f, min_base_y256;
  __m256i diff;
  __m128i a0_x128, a1_x128;

  a16 = _mm256_set1_epi32(16);
  c3f = _mm256_set1_epi32(0x3f);
  min_base_y256 = _mm256_set1_epi32(min_base_y);

  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i resx, resy, resxy;
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 8) {
      base_min_diff = 8;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    if (base_shift > 7) {
      resx = _mm_setzero_si128();
    } else {
      if (upsample_above) {
        a0_x128 = _mm_setr_epi16(
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][0]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][1]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][2]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][3]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][4]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][5]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][6]],
            above[base_x + HighbdEvenOddMaskx8_2[base_shift][7]]);
        a1_x128 = _mm_setr_epi16(
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][0]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][1]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][2]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][3]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][4]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][5]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][6]],
            above[base_x + 1 + HighbdEvenOddMaskx8_2[base_shift][7]]);
        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_slli_epi32(
                    _mm256_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                      (2 << 6) - y * dx, (3 << 6) - y * dx,
                                      (4 << 6) - y * dx, (5 << 6) - y * dx,
                                      (6 << 6) - y * dx, (7 << 6) - y * dx),
                    upsample_above),
                c3f),
            1);
      } else {
        a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift));
        a1_x128 = _mm_loadu_si128((__m128i *)(above + base_x + 1 + base_shift));
        a0_x128 =
            _mm_shuffle_epi8(a0_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);
        a1_x128 =
            _mm_shuffle_epi8(a1_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(-y * dx, (1 << 6) - y * dx, (2 << 6) - y * dx,
                                  (3 << 6) - y * dx, (4 << 6) - y * dx,
                                  (5 << 6) - y * dx, (6 << 6) - y * dx,
                                  (7 << 6) - y * dx),
                c3f),
            1);
      }

      a0_x = _mm256_cvtepu16_epi32(a0_x128);
      a1_x = _mm256_cvtepu16_epi32(a1_x128);

      diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resx = _mm256_castsi256_si128(_mm256_packus_epi32(
          res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1))));
    }
    // y calc
    if (base_x < min_base_x) {
      DECLARE_ALIGNED(32, int, base_y_c[8]);
      __m256i r6, c256, dy256, y_c256, base_y_c256, mask256;
      r6 = _mm256_set1_epi32(r << 6);
      dy256 = _mm256_set1_epi32(dy);
      c256 = _mm256_setr_epi32(1, 2, 3, 4, 5, 6, 7, 8);
      y_c256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
      base_y_c256 = _mm256_srai_epi32(y_c256, frac_bits_y);
      mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
      base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
      _mm256_store_si256((__m256i *)base_y_c, base_y_c256);

      a0_y = _mm256_cvtepu16_epi32(_mm_setr_epi16(
          left[base_y_c[0]], left[base_y_c[1]], left[base_y_c[2]],
          left[base_y_c[3]], left[base_y_c[4]], left[base_y_c[5]],
          left[base_y_c[6]], left[base_y_c[7]]));
      a1_y = _mm256_cvtepu16_epi32(_mm_setr_epi16(
          left[base_y_c[0] + 1], left[base_y_c[1] + 1], left[base_y_c[2] + 1],
          left[base_y_c[3] + 1], left[base_y_c[4] + 1], left[base_y_c[5] + 1],
          left[base_y_c[6] + 1], left[base_y_c[7] + 1]));

      if (upsample_left) {
        shift = _mm256_srli_epi32(
            _mm256_and_si256(_mm256_slli_epi32((y_c256), upsample_left), c3f),
            1);
      } else {
        shift = _mm256_srli_epi32(_mm256_and_si256(y_c256, c3f), 1);
      }
      diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resy = _mm256_castsi256_si128(_mm256_packus_epi32(
          res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1))));
    } else {
      resy = resx;
    }
    resxy =
        _mm_blendv_epi8(resx, resy, *(__m128i *)HighbdBaseMask[base_min_diff]);
    _mm_storeu_si128((__m128i *)(dst), resxy);
    dst += stride;
  }
}

static void highbd_dr_prediction_z2_HxW_avx2(
    int H, int W, uint16_t *dst, ptrdiff_t stride, const uint16_t *above,
    const uint16_t *left, int upsample_above, int upsample_left, int dx,
    int dy) {
  // here upsample_above and upsample_left are 0 by design of
  // av1_use_intra_edge_upsample
  const int min_base_x = -1;
  const int min_base_y = -1;
  (void)upsample_above;
  (void)upsample_left;
  const int frac_bits_x = 6;
  const int frac_bits_y = 6;

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a0_1_x, a1_1_x, a16;
  __m256i diff, min_base_y256, c3f;
  __m128i a0_x128, a1_x128, a0_1_x128, a1_1_x128;

  a16 = _mm256_set1_epi32(16);
  min_base_y256 = _mm256_set1_epi16(min_base_y);
  c3f = _mm256_set1_epi32(0x3f);
  for (int r = 0; r < H; r++) {
    __m256i b, res, shift;
    __m256i resx[2], resy[2];
    __m256i resxy;
    for (int j = 0; j < W; j += 16) {
      int y = r + 1;
      int base_x = (-y * dx) >> frac_bits_x;
      int base_shift = 0;
      if ((base_x + j) < (min_base_x - 1)) {
        base_shift = (min_base_x - (base_x + j) - 1);
      }
      int base_min_diff = (min_base_x - base_x - j);
      if (base_min_diff > 16) {
        base_min_diff = 16;
      } else {
        if (base_min_diff < 0) base_min_diff = 0;
      }

      if (base_shift > 7) {
        resx[0] = _mm256_setzero_si256();
      } else {
        a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift + j));
        a1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 1 + j));
        a0_x128 =
            _mm_shuffle_epi8(a0_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);
        a1_x128 =
            _mm_shuffle_epi8(a1_x128, *(__m128i *)HighbdLoadMaskx[base_shift]);

        a0_x = _mm256_cvtepu16_epi32(a0_x128);
        a1_x = _mm256_cvtepu16_epi32(a1_x128);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(
                    ((0 + j) << 6) - y * dx, ((1 + j) << 6) - y * dx,
                    ((2 + j) << 6) - y * dx, ((3 + j) << 6) - y * dx,
                    ((4 + j) << 6) - y * dx, ((5 + j) << 6) - y * dx,
                    ((6 + j) << 6) - y * dx, ((7 + j) << 6) - y * dx),
                _mm256_set1_epi32(0x3f)),
            1);

        diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        resx[0] = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
      }
      base_shift = 0;
      if ((base_x + j + 8) < (min_base_x - 1)) {
        base_shift = (min_base_x - (base_x + j + 8) - 1);
      }
      if (base_shift > 7) {
        resx[1] = _mm256_setzero_si256();
      } else {
        a0_1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 8 + j));
        a1_1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 9 + j));
        a0_1_x128 = _mm_shuffle_epi8(a0_1_x128,
                                     *(__m128i *)HighbdLoadMaskx[base_shift]);
        a1_1_x128 = _mm_shuffle_epi8(a1_1_x128,
                                     *(__m128i *)HighbdLoadMaskx[base_shift]);

        a0_1_x = _mm256_cvtepu16_epi32(a0_1_x128);
        a1_1_x = _mm256_cvtepu16_epi32(a1_1_x128);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(
                    ((8 + j) << 6) - y * dx, ((9 + j) << 6) - y * dx,
                    ((10 + j) << 6) - y * dx, ((11 + j) << 6) - y * dx,
                    ((12 + j) << 6) - y * dx, ((13 + j) << 6) - y * dx,
                    ((14 + j) << 6) - y * dx, ((15 + j) << 6) - y * dx),
                _mm256_set1_epi32(0x3f)),
            1);

        diff = _mm256_sub_epi32(a1_1_x, a0_1_x);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_1_x, 5);       // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);         // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        resx[1] = _mm256_add_epi32(a32, b);
        resx[1] = _mm256_srli_epi32(resx[1], 5);
        resx[1] = _mm256_packus_epi32(
            resx[1],
            _mm256_castsi128_si256(_mm256_extracti128_si256(resx[1], 1)));
      }
      resx[0] =
          _mm256_inserti128_si256(resx[0], _mm256_castsi256_si128(resx[1]),
                                  1);  // 16 16bit values

      // y calc
      if ((base_x < min_base_x)) {
        DECLARE_ALIGNED(32, int, base_y_c[16]);
        __m256i r6, c256, dy256, y_c256, y_c_1_256, base_y_c256, mask256;
        r6 = _mm256_set1_epi32(r << 6);
        dy256 = _mm256_set1_epi32(dy);
        c256 = _mm256_setr_epi32(1 + j, 2 + j, 3 + j, 4 + j, 5 + j, 6 + j,
                                 7 + j, 8 + j);
        y_c256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
        base_y_c256 = _mm256_srai_epi32(y_c256, frac_bits_y);
        mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
        base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
        _mm256_store_si256((__m256i *)base_y_c, base_y_c256);
        c256 = _mm256_setr_epi32(9 + j, 10 + j, 11 + j, 12 + j, 13 + j, 14 + j,
                                 15 + j, 16 + j);
        y_c_1_256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
        base_y_c256 = _mm256_srai_epi32(y_c_1_256, frac_bits_y);
        mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
        base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
        _mm256_store_si256((__m256i *)(base_y_c + 8), base_y_c256);

        a0_y = _mm256_cvtepu16_epi32(_mm_setr_epi16(
            left[base_y_c[0]], left[base_y_c[1]], left[base_y_c[2]],
            left[base_y_c[3]], left[base_y_c[4]], left[base_y_c[5]],
            left[base_y_c[6]], left[base_y_c[7]]));
        a1_y = _mm256_cvtepu16_epi32(_mm_setr_epi16(
            left[base_y_c[0] + 1], left[base_y_c[1] + 1], left[base_y_c[2] + 1],
            left[base_y_c[3] + 1], left[base_y_c[4] + 1], left[base_y_c[5] + 1],
            left[base_y_c[6] + 1], left[base_y_c[7] + 1]));

        shift = _mm256_srli_epi32(_mm256_and_si256(y_c256, c3f), 1);

        diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        resy[0] = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));

        a0_y = _mm256_cvtepu16_epi32(_mm_setr_epi16(
            left[base_y_c[8]], left[base_y_c[9]], left[base_y_c[10]],
            left[base_y_c[11]], left[base_y_c[12]], left[base_y_c[13]],
            left[base_y_c[14]], left[base_y_c[15]]));
        a1_y = _mm256_cvtepu16_epi32(
            _mm_setr_epi16(left[base_y_c[8] + 1], left[base_y_c[9] + 1],
                           left[base_y_c[10] + 1], left[base_y_c[11] + 1],
                           left[base_y_c[12] + 1], left[base_y_c[13] + 1],
                           left[base_y_c[14] + 1], left[base_y_c[15] + 1]));
        shift = _mm256_srli_epi32(_mm256_and_si256(y_c_1_256, c3f), 1);

        diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        resy[1] = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));

        resy[0] =
            _mm256_inserti128_si256(resy[0], _mm256_castsi256_si128(resy[1]),
                                    1);  // 16 16bit values
      } else {
        resy[0] = resx[0];
      }
      resxy = _mm256_blendv_epi8(resx[0], resy[0],
                                 *(__m256i *)HighbdBaseMask[base_min_diff]);
      _mm256_storeu_si256((__m256i *)(dst + j), resxy);
    }  // for j
    dst += stride;
  }
}

// Directional prediction, zone 2: 90 < angle < 180
void av1_highbd_dr_prediction_z2_avx2(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_above,
                                      int upsample_left, int dx, int dy,
                                      int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);
  switch (bw) {
    case 4:
      highbd_dr_prediction_z2_Nx4_avx2(bh, dst, stride, above, left,
                                       upsample_above, upsample_left, dx, dy);
      break;
    case 8:
      highbd_dr_prediction_z2_Nx8_avx2(bh, dst, stride, above, left,
                                       upsample_above, upsample_left, dx, dy);
      break;
    default:
      highbd_dr_prediction_z2_HxW_avx2(bh, bw, dst, stride, above, left,
                                       upsample_above, upsample_left, dx, dy);
      break;
  }
  return;
}

static void highbd_transpose(const uint16_t *src, ptrdiff_t pitchSrc,
                             uint16_t *dst, ptrdiff_t pitchDst, int width,
                             int height) {
  for (int j = 0; j < height; j += 8)
    for (int i = 0; i < width; i += 8)
      highbd_transpose_TX_8X8(src + i * pitchSrc + j, pitchSrc,
                              dst + j * pitchDst + i, pitchDst);
}

static void highbd_dr_prediction_z3_4x4_avx2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *left,
                                             int upsample_left, int dy) {
  __m128i dstvec[4], d[4];

  highbd_dr_prediction_z1_4xN_internal_avx2(4, dstvec, left, upsample_left, dy);
  highbd_transpose4x8_8x4_low_sse2(&dstvec[0], &dstvec[1], &dstvec[2],
                                   &dstvec[3], &d[0], &d[1], &d[2], &d[3]);
  _mm_storel_epi64((__m128i *)(dst + 0 * stride), d[0]);
  _mm_storel_epi64((__m128i *)(dst + 1 * stride), d[1]);
  _mm_storel_epi64((__m128i *)(dst + 2 * stride), d[2]);
  _mm_storel_epi64((__m128i *)(dst + 3 * stride), d[3]);
  return;
}

static void highbd_dr_prediction_z3_8x8_avx2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *left,
                                             int upsample_left, int dy) {
  __m128i dstvec[8], d[8];

  highbd_dr_prediction_z1_8xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  highbd_transpose8x8_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3],
                           &dstvec[4], &dstvec[5], &dstvec[6], &dstvec[7],
                           &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                           &d[7]);
  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
  }
}

static void highbd_dr_prediction_z3_4x8_avx2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *left,
                                             int upsample_left, int dy) {
  __m128i dstvec[4], d[8];

  highbd_dr_prediction_z1_8xN_internal_avx2(4, dstvec, left, upsample_left, dy);
  highbd_transpose4x8_8x4_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3],
                               &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                               &d[7]);
  for (int i = 0; i < 8; i++) {
    _mm_storel_epi64((__m128i *)(dst + i * stride), d[i]);
  }
}

static void highbd_dr_prediction_z3_8x4_avx2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *left,
                                             int upsample_left, int dy) {
  __m128i dstvec[8], d[4];

  highbd_dr_prediction_z1_4xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  highbd_transpose8x8_low_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3],
                               &dstvec[4], &dstvec[5], &dstvec[6], &dstvec[7],
                               &d[0], &d[1], &d[2], &d[3]);
  _mm_storeu_si128((__m128i *)(dst + 0 * stride), d[0]);
  _mm_storeu_si128((__m128i *)(dst + 1 * stride), d[1]);
  _mm_storeu_si128((__m128i *)(dst + 2 * stride), d[2]);
  _mm_storeu_si128((__m128i *)(dst + 3 * stride), d[3]);
}

static void highbd_dr_prediction_z3_8x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m256i dstvec[8], d[16];

  highbd_dr_prediction_z1_16xN_internal_avx2(8, dstvec, left, upsample_left,
                                             dy);
  highbd_transpose8x16_16x8_avx2(dstvec, d);
  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride),
                     _mm256_castsi256_si128(d[i]));
  }
  for (int i = 8; i < 16; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride),
                     _mm256_extracti128_si256(d[i - 8], 1));
  }
}

static void highbd_dr_prediction_z3_16x8_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m128i dstvec[16], d[16];

  highbd_dr_prediction_z1_8xN_internal_avx2(16, dstvec, left, upsample_left,
                                            dy);
  for (int i = 0; i < 16; i += 8) {
    highbd_transpose8x8_sse2(&dstvec[0 + i], &dstvec[1 + i], &dstvec[2 + i],
                             &dstvec[3 + i], &dstvec[4 + i], &dstvec[5 + i],
                             &dstvec[6 + i], &dstvec[7 + i], &d[0 + i],
                             &d[1 + i], &d[2 + i], &d[3 + i], &d[4 + i],
                             &d[5 + i], &d[6 + i], &d[7 + i]);
  }
  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
    _mm_storeu_si128((__m128i *)(dst + i * stride + 8), d[i + 8]);
  }
}

static void highbd_dr_prediction_z3_4x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m256i dstvec[4], d[4], d1;

  highbd_dr_prediction_z1_16xN_internal_avx2(4, dstvec, left, upsample_left,
                                             dy);
  highbd_transpose4x16_avx2(dstvec, d);
  for (int i = 0; i < 4; i++) {
    _mm_storel_epi64((__m128i *)(dst + i * stride),
                     _mm256_castsi256_si128(d[i]));
    d1 = _mm256_bsrli_epi128(d[i], 8);
    _mm_storel_epi64((__m128i *)(dst + (i + 4) * stride),
                     _mm256_castsi256_si128(d1));
    _mm_storel_epi64((__m128i *)(dst + (i + 8) * stride),
                     _mm256_extracti128_si256(d[i], 1));
    _mm_storel_epi64((__m128i *)(dst + (i + 12) * stride),
                     _mm256_extracti128_si256(d1, 1));
  }
}

static void highbd_dr_prediction_z3_16x4_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m128i dstvec[16], d[8];

  highbd_dr_prediction_z1_4xN_internal_avx2(16, dstvec, left, upsample_left,
                                            dy);
  highbd_transpose16x4_8x8_sse2(dstvec, d);

  _mm_storeu_si128((__m128i *)(dst + 0 * stride), d[0]);
  _mm_storeu_si128((__m128i *)(dst + 0 * stride + 8), d[1]);
  _mm_storeu_si128((__m128i *)(dst + 1 * stride), d[2]);
  _mm_storeu_si128((__m128i *)(dst + 1 * stride + 8), d[3]);
  _mm_storeu_si128((__m128i *)(dst + 2 * stride), d[4]);
  _mm_storeu_si128((__m128i *)(dst + 2 * stride + 8), d[5]);
  _mm_storeu_si128((__m128i *)(dst + 3 * stride), d[6]);
  _mm_storeu_si128((__m128i *)(dst + 3 * stride + 8), d[7]);
}

static void highbd_dr_prediction_z3_8x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m256i dstvec[16], d[16];

  highbd_dr_prediction_z1_32xN_internal_avx2(8, dstvec, left, upsample_left,
                                             dy);
  for (int i = 0; i < 16; i += 8) {
    highbd_transpose8x16_16x8_avx2(dstvec + i, d + i);
  }

  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride),
                     _mm256_castsi256_si128(d[i]));
  }
  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + (i + 8) * stride),
                     _mm256_extracti128_si256(d[i], 1));
  }
  for (int i = 8; i < 16; i++) {
    _mm_storeu_si128((__m128i *)(dst + (i + 8) * stride),
                     _mm256_castsi256_si128(d[i]));
  }
  for (int i = 8; i < 16; i++) {
    _mm_storeu_si128((__m128i *)(dst + (i + 16) * stride),
                     _mm256_extracti128_si256(d[i], 1));
  }
}

static void highbd_dr_prediction_z3_32x8_avx2(uint16_t *dst, ptrdiff_t stride,
                                              const uint16_t *left,
                                              int upsample_left, int dy) {
  __m128i dstvec[32], d[32];

  highbd_dr_prediction_z1_8xN_internal_avx2(32, dstvec, left, upsample_left,
                                            dy);
  for (int i = 0; i < 32; i += 8) {
    highbd_transpose8x8_sse2(&dstvec[0 + i], &dstvec[1 + i], &dstvec[2 + i],
                             &dstvec[3 + i], &dstvec[4 + i], &dstvec[5 + i],
                             &dstvec[6 + i], &dstvec[7 + i], &d[0 + i],
                             &d[1 + i], &d[2 + i], &d[3 + i], &d[4 + i],
                             &d[5 + i], &d[6 + i], &d[7 + i]);
  }
  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
    _mm_storeu_si128((__m128i *)(dst + i * stride + 8), d[i + 8]);
    _mm_storeu_si128((__m128i *)(dst + i * stride + 16), d[i + 16]);
    _mm_storeu_si128((__m128i *)(dst + i * stride + 24), d[i + 24]);
  }
}

static void highbd_dr_prediction_z3_16x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  __m256i dstvec[16], d[16];

  highbd_dr_prediction_z1_16xN_internal_avx2(16, dstvec, left, upsample_left,
                                             dy);
  highbd_transpose16x16_avx2(dstvec, d);

  for (int i = 0; i < 16; i++) {
    _mm256_storeu_si256((__m256i *)(dst + i * stride), d[i]);
  }
}

static void highbd_dr_prediction_z3_32x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  __m256i dstvec[64], d[16];

  highbd_dr_prediction_z1_32xN_internal_avx2(32, dstvec, left, upsample_left,
                                             dy);

  highbd_transpose16x16_avx2(dstvec, d);
  for (int j = 0; j < 16; j++) {
    _mm256_storeu_si256((__m256i *)(dst + j * stride), d[j]);
  }
  highbd_transpose16x16_avx2(dstvec + 16, d);
  for (int j = 0; j < 16; j++) {
    _mm256_storeu_si256((__m256i *)(dst + j * stride + 16), d[j]);
  }
  highbd_transpose16x16_avx2(dstvec + 32, d);
  for (int j = 0; j < 16; j++) {
    _mm256_storeu_si256((__m256i *)(dst + (j + 16) * stride), d[j]);
  }
  highbd_transpose16x16_avx2(dstvec + 48, d);
  for (int j = 0; j < 16; j++) {
    _mm256_storeu_si256((__m256i *)(dst + (j + 16) * stride + 16), d[j]);
  }
}

static void highbd_dr_prediction_z3_64x64_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  DECLARE_ALIGNED(16, uint16_t, dstT[64 * 64]);
  highbd_dr_prediction_z1_64xN_avx2(64, dstT, 64, left, upsample_left, dy);
  highbd_transpose(dstT, 64, dst, stride, 64, 64);
}

static void highbd_dr_prediction_z3_16x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  __m256i dstvec[32], d[32];

  highbd_dr_prediction_z1_32xN_internal_avx2(16, dstvec, left, upsample_left,
                                             dy);
  for (int i = 0; i < 32; i += 8) {
    highbd_transpose8x16_16x8_avx2(dstvec + i, d + i);
  }
  // store
  for (int j = 0; j < 32; j += 16) {
    for (int i = 0; i < 8; i++) {
      _mm_storeu_si128((__m128i *)(dst + (i + j) * stride),
                       _mm256_castsi256_si128(d[(i + j)]));
    }
    for (int i = 0; i < 8; i++) {
      _mm_storeu_si128((__m128i *)(dst + (i + j) * stride + 8),
                       _mm256_castsi256_si128(d[(i + j) + 8]));
    }
    for (int i = 8; i < 16; i++) {
      _mm256_storeu_si256(
          (__m256i *)(dst + (i + j) * stride),
          _mm256_inserti128_si256(
              d[(i + j)], _mm256_extracti128_si256(d[(i + j) - 8], 1), 0));
    }
  }
}

static void highbd_dr_prediction_z3_32x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  __m256i dstvec[32], d[16];

  highbd_dr_prediction_z1_16xN_internal_avx2(32, dstvec, left, upsample_left,
                                             dy);
  for (int i = 0; i < 32; i += 16) {
    highbd_transpose16x16_avx2((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      _mm256_storeu_si256((__m256i *)(dst + j * stride + i), d[j]);
    }
  }
}

static void highbd_dr_prediction_z3_32x64_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  uint16_t dstT[64 * 32];
  highbd_dr_prediction_z1_64xN_avx2(32, dstT, 64, left, upsample_left, dy);
  highbd_transpose(dstT, 64, dst, stride, 32, 64);
}

static void highbd_dr_prediction_z3_64x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  DECLARE_ALIGNED(16, uint16_t, dstT[32 * 64]);
  highbd_dr_prediction_z1_32xN_avx2(64, dstT, 32, left, upsample_left, dy);
  highbd_transpose(dstT, 32, dst, stride, 64, 32);
  return;
}

static void highbd_dr_prediction_z3_16x64_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  DECLARE_ALIGNED(16, uint16_t, dstT[64 * 16]);
  highbd_dr_prediction_z1_64xN_avx2(16, dstT, 64, left, upsample_left, dy);
  highbd_transpose(dstT, 64, dst, stride, 16, 64);
}

static void highbd_dr_prediction_z3_64x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                               const uint16_t *left,
                                               int upsample_left, int dy) {
  __m256i dstvec[64], d[16];

  highbd_dr_prediction_z1_16xN_internal_avx2(64, dstvec, left, upsample_left,
                                             dy);
  for (int i = 0; i < 64; i += 16) {
    highbd_transpose16x16_avx2((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      _mm256_storeu_si256((__m256i *)(dst + j * stride + i), d[j]);
    }
  }
}

void av1_highbd_dr_prediction_z3_avx2(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_left,
                                      int dx, int dy, int bd) {
  (void)above;
  (void)dx;
  (void)bd;
  assert(dx == 1);
  assert(dy > 0);
  if (bw == bh) {
    switch (bw) {
      case 4:
        highbd_dr_prediction_z3_4x4_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 8:
        highbd_dr_prediction_z3_8x8_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 16:
        highbd_dr_prediction_z3_16x16_avx2(dst, stride, left, upsample_left,
                                           dy);
        break;
      case 32:
        highbd_dr_prediction_z3_32x32_avx2(dst, stride, left, upsample_left,
                                           dy);
        break;
      case 64:
        highbd_dr_prediction_z3_64x64_avx2(dst, stride, left, upsample_left,
                                           dy);
        break;
    }
  } else {
    if (bw < bh) {
      if (bw + bw == bh) {
        switch (bw) {
          case 4:
            highbd_dr_prediction_z3_4x8_avx2(dst, stride, left, upsample_left,
                                             dy);
            break;
          case 8:
            highbd_dr_prediction_z3_8x16_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 16:
            highbd_dr_prediction_z3_16x32_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
          case 32:
            highbd_dr_prediction_z3_32x64_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
        }
      } else {
        switch (bw) {
          case 4:
            highbd_dr_prediction_z3_4x16_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 8:
            highbd_dr_prediction_z3_8x32_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 16:
            highbd_dr_prediction_z3_16x64_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
        }
      }
    } else {
      if (bh + bh == bw) {
        switch (bh) {
          case 4:
            highbd_dr_prediction_z3_8x4_avx2(dst, stride, left, upsample_left,
                                             dy);
            break;
          case 8:
            highbd_dr_prediction_z3_16x8_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 16:
            highbd_dr_prediction_z3_32x16_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
          case 32:
            highbd_dr_prediction_z3_64x32_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
        }
      } else {
        switch (bh) {
          case 4:
            highbd_dr_prediction_z3_16x4_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 8:
            highbd_dr_prediction_z3_32x8_avx2(dst, stride, left, upsample_left,
                                              dy);
            break;
          case 16:
            highbd_dr_prediction_z3_64x16_avx2(dst, stride, left, upsample_left,
                                               dy);
            break;
        }
      }
    }
  }
  return;
}

// Low bit depth functions
static uint8_t BaseMask[33][32] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};

static AOM_FORCE_INLINE void dr_prediction_z1_4xN_internal_avx2(
    int N, __m128i *dst, const uint8_t *above, int upsample_above, int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((N + 4) - 1) << upsample_above;
  int x;
  // a assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a1, a32, a16;
  __m256i diff;
  __m128i a_mbase_x;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm_set1_epi8(above[max_base_x]);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i res1, a0_128, a1_128;

    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base) >> upsample_above;
    if (base_max_diff <= 0) {
      for (int i = r; i < N; ++i) {
        dst[i] = a_mbase_x;  // save 4 values
      }
      return;
    }
    if (base_max_diff > 4) base_max_diff = 4;
    a0_128 = _mm_loadu_si128((__m128i *)(above + base));
    a1_128 = _mm_srli_si128(a0_128, 1);
    a0 = _mm256_cvtepu8_epi32(a0_128);
    a1 = _mm256_cvtepu8_epi32(a1_128);

    if (upsample_above) {
      a0 = _mm256_permutevar8x32_epi32(
          a0, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0, 1));
      shift = _mm256_srli_epi32(
          _mm256_and_si256(
              _mm256_slli_epi32(_mm256_set1_epi32(x), upsample_above),
              _mm256_set1_epi32(0x3f)),
          1);
    } else {
      shift = _mm256_srli_epi32(
          _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);
    }

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16

    b = _mm256_mullo_epi32(diff, shift);
    res = _mm256_add_epi32(a32, b);
    res = _mm256_srli_epi32(res, 5);

    res1 = _mm256_castsi256_si128(res);
    res1 = _mm_packus_epi32(res1, res1);
    res1 = _mm_packus_epi16(res1, res1);

    dst[r] =
        _mm_blendv_epi8(a_mbase_x, res1, *(__m128i *)BaseMask[base_max_diff]);
    x += dx;
  }
}

static void dr_prediction_z1_4xN_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, int upsample_above,
                                      int dx) {
  __m128i dstvec[16];

  dr_prediction_z1_4xN_internal_avx2(N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    *(uint32_t *)(dst + stride * i) = _mm_cvtsi128_si32(dstvec[i]);
  }
}

static AOM_FORCE_INLINE void dr_prediction_z1_8xN_internal_avx2(
    int N, __m128i *dst, const uint8_t *above, int upsample_above, int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((8 + N) - 1) << upsample_above;

  int x;
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a1, a0_1, a1_1, a32, a16, diff;
  __m128i a_mbase_x;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm_set1_epi8(above[max_base_x]);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res, res1, shift;
    __m128i res128;

    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base) >> upsample_above;
    if (base_max_diff <= 0) {
      for (int i = r; i < N; ++i) {
        dst[i] = a_mbase_x;  // save 16 values, 8 to be used furter
      }
      return;
    }
    if (base_max_diff > 8) base_max_diff = 8;

    a0 = _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base)));
    a1 = _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base + 1)));

    if (upsample_above) {
      a0 = _mm256_permutevar8x32_epi32(
          a0, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0, 1));

      a0_1 =
          _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base + 8)));
      a0_1 = _mm256_permutevar8x32_epi32(
          a0_1, _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
      a1_1 = _mm256_castsi128_si256(_mm256_extracti128_si256(a0_1, 1));

      a0 = _mm256_inserti128_si256(a0, _mm256_castsi256_si128(a0_1), 1);
      a1 = _mm256_inserti128_si256(a1, _mm256_castsi256_si128(a1_1), 1);

      shift = _mm256_srli_epi32(
          _mm256_and_si256(
              _mm256_slli_epi32(_mm256_set1_epi32(x), upsample_above),
              _mm256_set1_epi32(0x3f)),
          1);
    } else {
      shift = _mm256_srli_epi32(
          _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);
    }

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16

    b = _mm256_mullo_epi32(diff, shift);
    res = _mm256_add_epi32(a32, b);
    res = _mm256_srli_epi32(res, 5);

    res1 = _mm256_packus_epi32(
        res, _mm256_castsi128_si256(
                 _mm256_extracti128_si256(res, 1)));  // goto 16 bit

    res128 = _mm_packus_epi16(_mm256_castsi256_si128(res1),
                              _mm256_castsi256_si128(res1));  // goto 8 bit

    res128 =
        _mm_blendv_epi8(a_mbase_x, res128, *(__m128i *)BaseMask[base_max_diff]);
    dst[r] = res128;
    x += dx;
  }
}

static void dr_prediction_z1_8xN_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, int upsample_above,
                                      int dx) {
  __m128i dstvec[32];

  dr_prediction_z1_8xN_internal_avx2(N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    _mm_storel_epi64((__m128i *)(dst + stride * i), dstvec[i]);
  }
}

static AOM_FORCE_INLINE void dr_prediction_z1_16xN_internal_avx2(
    int N, __m128i *dstvec, const uint8_t *above, int upsample_above, int dx) {
  int x;
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((16 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, diff, a32, a16;
  __m128i a_mbase_x;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm_set1_epi8((uint8_t)above[max_base_x]);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res[2];
    __m128i res128[2];
    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base);
    if (base_max_diff <= 0) {
      for (int i = r; i < N; ++i) {
        dstvec[i] = a_mbase_x;  // save 16 values
      }
      return;
    }
    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    a0 = _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base)));
    a1 = _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base + 1)));

    diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
    a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
    a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
    b = _mm256_mullo_epi32(diff, shift);

    res[0] = _mm256_add_epi32(a32, b);
    res[0] = _mm256_srli_epi32(res[0], 5);
    res[0] = _mm256_packus_epi32(
        res[0], _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));
    res128[0] = _mm_packus_epi16(_mm256_castsi256_si128(res[0]),
                                 _mm256_castsi256_si128(res[0]));  // goto 8 bit

    if (base_max_diff > 8) {
      if (base_max_diff > 16) base_max_diff = 16;
      a0_1 =
          _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base + 8)));
      a1_1 =
          _mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i *)(above + base + 9)));

      diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
      b = _mm256_mullo_epi32(diff, shift);

      res[1] = _mm256_add_epi32(a32, b);
      res[1] = _mm256_srli_epi32(res[1], 5);
      res[1] = _mm256_packus_epi32(
          res[1], _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
      res128[1] =
          _mm_packus_epi16(_mm256_castsi256_si128(res[1]),
                           _mm256_castsi256_si128(res[1]));  // goto 8 bit

    } else {
      res128[1] = a_mbase_x;
    }
    res128[0] = _mm_unpacklo_epi64(res128[0], res128[1]);  // 16 8bit values

    dstvec[r] = _mm_blendv_epi8(a_mbase_x, res128[0],
                                *(__m128i *)BaseMask[base_max_diff]);
    x += dx;
  }
}
static void dr_prediction_z1_16xN_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  __m128i dstvec[64];

  dr_prediction_z1_16xN_internal_avx2(N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    _mm_storeu_si128((__m128i *)(dst + stride * i), dstvec[i]);
  }
}

static AOM_FORCE_INLINE void dr_prediction_z1_32xN_internal_avx2(
    int N, __m256i *dstvec, const uint8_t *above, int upsample_above, int dx) {
  int x;
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((32 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, a32, a16;
  __m256i a_mbase_x, diff;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi8(above[max_base_x]);

  x = dx;
  for (int r = 0; r < N; r++) {
    __m256i b, res[2], res16[2];

    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base);
    if (base_max_diff <= 0) {
      for (int i = r; i < N; ++i) {
        dstvec[i] = a_mbase_x;  // save 32 values
      }
      return;
    }
    if (base_max_diff > 32) base_max_diff = 32;
    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    for (int j = 0, jj = 0; j < 32; j += 16, jj++) {
      int mdiff = base_max_diff - j;
      if (mdiff <= 0) {
        res16[jj] = a_mbase_x;
      } else {
        a0 = _mm256_cvtepu8_epi32(
            _mm_loadu_si128((__m128i *)(above + base + j)));
        a1 = _mm256_cvtepu8_epi32(
            _mm_loadu_si128((__m128i *)(above + base + 1 + j)));

        diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        res[0] = _mm256_add_epi32(a32, b);
        res[0] = _mm256_srli_epi32(res[0], 5);
        res[0] = _mm256_packus_epi32(
            res[0],
            _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));

        // goto 8 bit
        res[0] = _mm256_packus_epi16(res[0], res[0]);

        if (mdiff > 8) {
          a0_1 = _mm256_cvtepu8_epi32(
              _mm_loadu_si128((__m128i *)(above + base + 8 + j)));
          a1_1 = _mm256_cvtepu8_epi32(
              _mm_loadu_si128((__m128i *)(above + base + 9 + j)));

          diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
          a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
          a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
          b = _mm256_mullo_epi32(diff, shift);

          res[1] = _mm256_add_epi32(a32, b);
          res[1] = _mm256_srli_epi32(res[1], 5);
          res[1] = _mm256_packus_epi32(
              res[1],
              _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
          res[1] = _mm256_packus_epi16(res[1], res[1]);
          // goto 8 bit
        } else {
          res[1] = a_mbase_x;
        }
        res16[jj] = _mm256_unpacklo_epi64(res[0], res[1]);  // 16 8bit values
      }
    }
    res16[1] =
        _mm256_inserti128_si256(res16[0], _mm256_castsi256_si128(res16[1]),
                                1);  // 32 8bit values

    dstvec[r] = _mm256_blendv_epi8(
        a_mbase_x, res16[1],
        *(__m256i *)BaseMask[base_max_diff]);  // 32 8bit values
    x += dx;
  }
}

static void dr_prediction_z1_32xN_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  __m256i dstvec[64];
  dr_prediction_z1_32xN_internal_avx2(N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    _mm256_storeu_si256((__m256i *)(dst + stride * i), dstvec[i]);
  }
}

static void dr_prediction_z1_64xN_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  int x;

  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((64 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0, a0_1, a1, a1_1, a32, a16;
  __m256i a_mbase_x, diff;
  __m128i max_base_x128, base_inc128, mask128;

  a16 = _mm256_set1_epi32(16);
  a_mbase_x = _mm256_set1_epi8(above[max_base_x]);
  max_base_x128 = _mm_set1_epi8(max_base_x);

  x = dx;
  for (int r = 0; r < N; r++, dst += stride) {
    __m256i b, res[2];
    __m128i res1;

    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        _mm256_storeu_si256((__m256i *)dst, a_mbase_x);  // save 32 values
        _mm256_storeu_si256((__m256i *)(dst + 32), a_mbase_x);
        dst += stride;
      }
      return;
    }

    __m256i shift = _mm256_srli_epi32(
        _mm256_and_si256(_mm256_set1_epi32(x), _mm256_set1_epi32(0x3f)), 1);

    __m128i a0_128, a0_1_128, a1_128, a1_1_128;
    for (int j = 0; j < 64; j += 16) {
      int mdif = max_base_x - (base + j);
      if (mdif <= 0) {
        _mm_storeu_si128((__m128i *)(dst + j),
                         _mm256_castsi256_si128(a_mbase_x));
      } else {
        a0_128 = _mm_loadu_si128((__m128i *)(above + base + j));
        a1_128 = _mm_loadu_si128((__m128i *)(above + base + 1 + j));
        a0 = _mm256_cvtepu8_epi32(a0_128);
        a1 = _mm256_cvtepu8_epi32(a1_128);

        diff = _mm256_sub_epi32(a1, a0);   // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0, 5);    // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);  // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        res[0] = _mm256_add_epi32(a32, b);
        res[0] = _mm256_srli_epi32(res[0], 5);
        res[0] = _mm256_packus_epi32(
            res[0],
            _mm256_castsi128_si256(_mm256_extracti128_si256(res[0], 1)));
        // goto 8 bit
        res[0] = _mm256_packus_epi16(res[0], res[0]);

        if (mdif > 8) {
          a0_1_128 = _mm_loadu_si128((__m128i *)(above + base + 8 + j));
          a1_1_128 = _mm_loadu_si128((__m128i *)(above + base + 9 + j));
          a0_1 = _mm256_cvtepu8_epi32(a0_1_128);
          a1_1 = _mm256_cvtepu8_epi32(a1_1_128);

          diff = _mm256_sub_epi32(a1_1, a0_1);  // a[x+1] - a[x]
          a32 = _mm256_slli_epi32(a0_1, 5);     // a[x] * 32
          a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16
          b = _mm256_mullo_epi32(diff, shift);

          res[1] = _mm256_add_epi32(a32, b);
          res[1] = _mm256_srli_epi32(res[1], 5);
          res[1] = _mm256_packus_epi32(
              res[1],
              _mm256_castsi128_si256(_mm256_extracti128_si256(res[1], 1)));
          res[1] = _mm256_packus_epi16(res[1], res[1]);

        } else {
          res[1] = a_mbase_x;
        }
        res1 = _mm_unpacklo_epi64(
            _mm256_castsi256_si128(res[0]),
            _mm256_castsi256_si128(res[1]));  // 16 8bit values

        base_inc128 = _mm_setr_epi8(
            base + j, base + j + 1, base + j + 2, base + j + 3, base + j + 4,
            base + j + 5, base + j + 6, base + j + 7, base + j + 8,
            base + j + 9, base + j + 10, base + j + 11, base + j + 12,
            base + j + 13, base + j + 14, base + j + 15);

        mask128 = _mm_cmpgt_epi8(_mm_subs_epu8(max_base_x128, base_inc128),
                                 _mm_setzero_si128());
        res1 =
            _mm_blendv_epi8(_mm256_castsi256_si128(a_mbase_x), res1, mask128);
        _mm_storeu_si128((__m128i *)(dst + j), res1);
      }
    }
    x += dx;
  }
}

// Directional prediction, zone 1: 0 < angle < 90
void av1_dr_prediction_z1_avx2(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_above, int dx, int dy) {
  (void)left;
  (void)dy;
  switch (bw) {
    case 4:
      dr_prediction_z1_4xN_avx2(bh, dst, stride, above, upsample_above, dx);
      break;
    case 8:
      dr_prediction_z1_8xN_avx2(bh, dst, stride, above, upsample_above, dx);
      break;
    case 16:
      dr_prediction_z1_16xN_avx2(bh, dst, stride, above, upsample_above, dx);
      break;
    case 32:
      dr_prediction_z1_32xN_avx2(bh, dst, stride, above, upsample_above, dx);
      break;
    case 64:
      dr_prediction_z1_64xN_avx2(bh, dst, stride, above, upsample_above, dx);
      break;
    default: break;
  }
  return;
}

static uint8_t LoadMaskx[8][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 },
  { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 },
  { 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 },
  { 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
  { 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
  { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
};

static uint8_t EvenOddMaskx4[8][16] = {
  { 0, 2, 4, 6, 1, 3, 5, 7, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 1, 3, 5, 7, 2, 4, 6, 8, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 2, 4, 6, 8, 3, 5, 7, 9, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 3, 5, 7, 9, 4, 6, 8, 10, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 4, 6, 8, 10, 5, 7, 9, 11, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 5, 7, 9, 11, 6, 8, 10, 12, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 6, 8, 10, 12, 7, 9, 11, 13, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 7, 9, 11, 13, 8, 10, 12, 14, 0 }
};

static uint8_t EvenOddMaskx[8][16] = {
  { 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 0, 0, 0, 0 },
  { 0, 1, 3, 5, 7, 9, 11, 13, 15, 2, 4, 6, 8, 0, 0, 0 },
  { 0, 0, 2, 4, 6, 8, 10, 12, 14, 3, 5, 7, 9, 0, 0, 0 },
  { 0, 0, 0, 3, 5, 7, 9, 11, 13, 15, 4, 6, 8, 10, 0 },
  { 0, 0, 0, 0, 4, 6, 8, 10, 12, 14, 5, 7, 9, 11, 0, 0 },
  { 0, 0, 0, 0, 0, 5, 7, 9, 11, 13, 15, 6, 8, 10, 12, 0 },
  { 0, 0, 0, 0, 0, 0, 6, 8, 10, 12, 14, 7, 9, 11, 13, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 7, 9, 11, 13, 15, 8, 10, 12, 14 }
};

static void dr_prediction_z2_Nx4_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, const uint8_t *left,
                                      int upsample_above, int upsample_left,
                                      int dx, int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  // a assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a16, diff;
  __m128i c3f, min_base_y128;

  a16 = _mm256_set1_epi32(16);
  c3f = _mm_set1_epi32(0x3f);
  min_base_y128 = _mm_set1_epi32(min_base_y);

  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i resx, resy, resxy;
    __m128i a0_x128, a1_x128;
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 4) {
      base_min_diff = 4;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    if (base_shift > 3) {
      resx = _mm_setzero_si128();
    } else {
      a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift));
      if (upsample_above) {
        a0_x128 =
            _mm_shuffle_epi8(a0_x128, *(__m128i *)EvenOddMaskx4[base_shift]);
        a1_x128 = _mm_srli_si128(a0_x128, 4);

        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(
                _mm_slli_epi32(
                    _mm_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                   (2 << 6) - y * dx, (3 << 6) - y * dx),
                    upsample_above),
                c3f),
            1));
      } else {
        a0_x128 = _mm_shuffle_epi8(a0_x128, *(__m128i *)LoadMaskx[base_shift]);
        a1_x128 = _mm_srli_si128(a0_x128, 1);
        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(_mm_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                         (2 << 6) - y * dx, (3 << 6) - y * dx),
                          c3f),
            1));
      }
      a0_x = _mm256_cvtepu8_epi32(a0_x128);
      a1_x = _mm256_cvtepu8_epi32(a1_x128);

      diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resx = _mm256_castsi256_si128(res);
      resx = _mm_packus_epi32(resx, resx);
      resx = _mm_packus_epi16(resx, resx);
    }
    // y calc
    if (base_x < min_base_x) {
      DECLARE_ALIGNED(32, int, base_y_c[4]);
      __m128i r6, c1234, dy128, y_c128, base_y_c128, mask128;
      r6 = _mm_set1_epi32(r << 6);
      dy128 = _mm_set1_epi32(dy);
      c1234 = _mm_setr_epi32(1, 2, 3, 4);
      y_c128 = _mm_sub_epi32(r6, _mm_mullo_epi32(c1234, dy128));
      base_y_c128 = _mm_srai_epi32(y_c128, frac_bits_y);
      mask128 = _mm_cmpgt_epi32(min_base_y128, base_y_c128);
      base_y_c128 = _mm_andnot_si128(mask128, base_y_c128);
      _mm_store_si128((__m128i *)base_y_c, base_y_c128);

      a0_y = _mm256_castsi128_si256(
          _mm_setr_epi32(left[base_y_c[0]], left[base_y_c[1]],
                         left[base_y_c[2]], left[base_y_c[3]]));
      a1_y = _mm256_castsi128_si256(
          _mm_setr_epi32(left[base_y_c[0] + 1], left[base_y_c[1] + 1],
                         left[base_y_c[2] + 1], left[base_y_c[3] + 1]));

      if (upsample_left) {
        shift = _mm256_castsi128_si256(_mm_srli_epi32(
            _mm_and_si128(_mm_slli_epi32(y_c128, upsample_left), c3f), 1));
      } else {
        shift = _mm256_castsi128_si256(
            _mm_srli_epi32(_mm_and_si128(y_c128, c3f), 1));
      }
      diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      resy = _mm256_castsi256_si128(res);
      resy = _mm_packus_epi32(resy, resy);
      resy = _mm_packus_epi16(resy, resy);
    } else {
      resy = resx;
    }
    resxy = _mm_blendv_epi8(resx, resy, *(__m128i *)BaseMask[base_min_diff]);
    *(uint32_t *)(dst) = _mm_cvtsi128_si32(resxy);
    dst += stride;
  }
}

static void dr_prediction_z2_Nx8_avx2(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, const uint8_t *left,
                                      int upsample_above, int upsample_left,
                                      int dx, int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a16, c3f;
  __m256i diff, min_base_y256;
  __m128i a0_x128, a1_x128;

  a16 = _mm256_set1_epi32(16);
  c3f = _mm256_set1_epi32(0x3f);
  min_base_y256 = _mm256_set1_epi32(min_base_y);

  for (int r = 0; r < N; r++) {
    __m256i b, res, shift;
    __m128i resx, resy, resxy;

    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 8) {
      base_min_diff = 8;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    if (base_shift > 7) {
      resx = _mm_setzero_si128();
    } else {
      a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift));
      a1_x128 = _mm_loadu_si128((__m128i *)(above + base_x + 1 + base_shift));
      if (upsample_above) {
        a0_x128 =
            _mm_shuffle_epi8(a0_x128, *(__m128i *)EvenOddMaskx[base_shift]);
        a1_x128 =
            _mm_shuffle_epi8(a1_x128, *(__m128i *)EvenOddMaskx[base_shift]);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_slli_epi32(
                    _mm256_setr_epi32(-y * dx, (1 << 6) - y * dx,
                                      (2 << 6) - y * dx, (3 << 6) - y * dx,
                                      (4 << 6) - y * dx, (5 << 6) - y * dx,
                                      (6 << 6) - y * dx, (7 << 6) - y * dx),
                    upsample_above),
                c3f),
            1);
      } else {
        a0_x128 = _mm_shuffle_epi8(a0_x128, *(__m128i *)LoadMaskx[base_shift]);
        a1_x128 = _mm_shuffle_epi8(a1_x128, *(__m128i *)LoadMaskx[base_shift]);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(-y * dx, (1 << 6) - y * dx, (2 << 6) - y * dx,
                                  (3 << 6) - y * dx, (4 << 6) - y * dx,
                                  (5 << 6) - y * dx, (6 << 6) - y * dx,
                                  (7 << 6) - y * dx),
                c3f),
            1);
      }
      a0_x = _mm256_cvtepu8_epi32(a0_x128);
      a1_x = _mm256_cvtepu8_epi32(a1_x128);

      diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      res = _mm256_packus_epi32(
          res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
      resx = _mm_packus_epi16(_mm256_castsi256_si128(res),
                              _mm256_castsi256_si128(res));
    }
    // y calc
    if (base_x < min_base_x) {
      DECLARE_ALIGNED(32, int, base_y_c[8]);
      __m256i r6, c256, dy256, y_c256, base_y_c256, mask256;
      r6 = _mm256_set1_epi32(r << 6);
      dy256 = _mm256_set1_epi32(dy);
      c256 = _mm256_setr_epi32(1, 2, 3, 4, 5, 6, 7, 8);
      y_c256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
      base_y_c256 = _mm256_srai_epi32(y_c256, frac_bits_y);
      mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
      base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
      _mm256_store_si256((__m256i *)base_y_c, base_y_c256);

      a0_y = _mm256_setr_epi32(left[base_y_c[0]], left[base_y_c[1]],
                               left[base_y_c[2]], left[base_y_c[3]],
                               left[base_y_c[4]], left[base_y_c[5]],
                               left[base_y_c[6]], left[base_y_c[7]]);
      a1_y = _mm256_setr_epi32(left[base_y_c[0] + 1], left[base_y_c[1] + 1],
                               left[base_y_c[2] + 1], left[base_y_c[3] + 1],
                               left[base_y_c[4] + 1], left[base_y_c[5] + 1],
                               left[base_y_c[6] + 1], left[base_y_c[7] + 1]);

      if (upsample_left) {
        shift = _mm256_srli_epi32(
            _mm256_and_si256(_mm256_slli_epi32(y_c256, upsample_left), c3f), 1);
      } else {
        shift = _mm256_srli_epi32(_mm256_and_si256(y_c256, c3f), 1);
      }
      diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
      a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
      a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

      b = _mm256_mullo_epi32(diff, shift);
      res = _mm256_add_epi32(a32, b);
      res = _mm256_srli_epi32(res, 5);

      res = _mm256_packus_epi32(
          res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
      resy = _mm_packus_epi16(_mm256_castsi256_si128(res),
                              _mm256_castsi256_si128(res));
    } else {
      resy = resx;
    }
    resxy = _mm_blendv_epi8(resx, resy, *(__m128i *)BaseMask[base_min_diff]);
    _mm_storel_epi64((__m128i *)(dst), resxy);
    dst += stride;
  }
}

static void dr_prediction_z2_HxW_avx2(int H, int W, uint8_t *dst,
                                      ptrdiff_t stride, const uint8_t *above,
                                      const uint8_t *left, int upsample_above,
                                      int upsample_left, int dx, int dy) {
  // here upsample_above and upsample_left are 0 by design of
  // av1_use_intra_edge_upsample
  const int min_base_x = -1;
  const int min_base_y = -1;
  (void)upsample_above;
  (void)upsample_left;
  const int frac_bits_x = 6;
  const int frac_bits_y = 6;

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be caluculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5
  __m256i a0_x, a1_x, a0_y, a1_y, a32, a0_1_x, a1_1_x, a16;
  __m256i diff, min_base_y256, c3f;
  __m128i a0_x128, a1_x128, a0_1_x128, a1_1_x128;

  a16 = _mm256_set1_epi32(16);
  min_base_y256 = _mm256_set1_epi32(min_base_y);
  c3f = _mm256_set1_epi32(0x3f);

  for (int r = 0; r < H; r++) {
    __m256i b, res, shift;
    __m128i resx[2], resy[2];
    __m128i resxy;
    for (int j = 0; j < W; j += 16) {
      int y = r + 1;
      int base_x = (-y * dx) >> frac_bits_x;

      int base_shift = 0;
      if ((base_x + j) < (min_base_x - 1)) {
        base_shift = (min_base_x - (base_x + j) - 1);
      }
      int base_min_diff = (min_base_x - base_x - j);
      if (base_min_diff > 16) {
        base_min_diff = 16;
      } else {
        if (base_min_diff < 0) base_min_diff = 0;
      }
      if (base_shift > 7) {
        resx[0] = _mm_setzero_si128();
      } else {
        a0_x128 = _mm_loadu_si128((__m128i *)(above + base_x + base_shift + j));
        a1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 1 + j));
        a0_x128 = _mm_shuffle_epi8(a0_x128, *(__m128i *)LoadMaskx[base_shift]);
        a1_x128 = _mm_shuffle_epi8(a1_x128, *(__m128i *)LoadMaskx[base_shift]);

        a0_x = _mm256_cvtepu8_epi32(a0_x128);
        a1_x = _mm256_cvtepu8_epi32(a1_x128);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(
                    ((0 + j) << 6) - y * dx, ((1 + j) << 6) - y * dx,
                    ((2 + j) << 6) - y * dx, ((3 + j) << 6) - y * dx,
                    ((4 + j) << 6) - y * dx, ((5 + j) << 6) - y * dx,
                    ((6 + j) << 6) - y * dx, ((7 + j) << 6) - y * dx),
                c3f),
            1);

        diff = _mm256_sub_epi32(a1_x, a0_x);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_x, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        res = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
        resx[0] = _mm_packus_epi16(_mm256_castsi256_si128(res),
                                   _mm256_castsi256_si128(res));
      }
      base_shift = 0;
      if ((base_x + j + 8) < (min_base_x - 1)) {
        base_shift = (min_base_x - (base_x + j + 8) - 1);
      }
      if (base_shift > 7) {
        resx[1] = _mm_setzero_si128();
      } else {
        a0_1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 8 + j));
        a1_1_x128 =
            _mm_loadu_si128((__m128i *)(above + base_x + base_shift + 9 + j));
        a0_1_x128 =
            _mm_shuffle_epi8(a0_1_x128, *(__m128i *)LoadMaskx[base_shift]);
        a1_1_x128 =
            _mm_shuffle_epi8(a1_1_x128, *(__m128i *)LoadMaskx[base_shift]);

        a0_1_x = _mm256_cvtepu8_epi32(a0_1_x128);
        a1_1_x = _mm256_cvtepu8_epi32(a1_1_x128);

        shift = _mm256_srli_epi32(
            _mm256_and_si256(
                _mm256_setr_epi32(
                    ((8 + j) << 6) - y * dx, ((9 + j) << 6) - y * dx,
                    ((10 + j) << 6) - y * dx, ((11 + j) << 6) - y * dx,
                    ((12 + j) << 6) - y * dx, ((13 + j) << 6) - y * dx,
                    ((14 + j) << 6) - y * dx, ((15 + j) << 6) - y * dx),
                _mm256_set1_epi32(0x3f)),
            1);

        diff = _mm256_sub_epi32(a1_1_x, a0_1_x);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_1_x, 5);       // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);         // a[x] * 32 + 16
        b = _mm256_mullo_epi32(diff, shift);

        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);
        res = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
        resx[1] = _mm_packus_epi16(_mm256_castsi256_si128(res),
                                   _mm256_castsi256_si128(res));
      }
      resx[0] = _mm_unpacklo_epi64(resx[0], resx[1]);

      // y calc
      if ((base_x < min_base_x)) {
        DECLARE_ALIGNED(32, int, base_y_c[16]);
        __m256i r6, c256, dy256, y_c256, y_c_1_256, base_y_c256, mask256;
        r6 = _mm256_set1_epi32(r << 6);
        dy256 = _mm256_set1_epi32(dy);
        c256 = _mm256_setr_epi32(1 + j, 2 + j, 3 + j, 4 + j, 5 + j, 6 + j,
                                 7 + j, 8 + j);
        y_c256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
        base_y_c256 = _mm256_srai_epi32(y_c256, frac_bits_y);
        mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
        base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
        _mm256_store_si256((__m256i *)base_y_c, base_y_c256);
        c256 = _mm256_setr_epi32(9 + j, 10 + j, 11 + j, 12 + j, 13 + j, 14 + j,
                                 15 + j, 16 + j);
        y_c_1_256 = _mm256_sub_epi32(r6, _mm256_mullo_epi32(c256, dy256));
        base_y_c256 = _mm256_srai_epi32(y_c_1_256, frac_bits_y);
        mask256 = _mm256_cmpgt_epi32(min_base_y256, base_y_c256);
        base_y_c256 = _mm256_andnot_si256(mask256, base_y_c256);
        _mm256_store_si256((__m256i *)(base_y_c + 8), base_y_c256);

        a0_y = _mm256_cvtepu8_epi32(_mm_setr_epi8(
            left[base_y_c[0]], left[base_y_c[1]], left[base_y_c[2]],
            left[base_y_c[3]], left[base_y_c[4]], left[base_y_c[5]],
            left[base_y_c[6]], left[base_y_c[7]], 0, 0, 0, 0, 0, 0, 0, 0));
        a1_y = _mm256_cvtepu8_epi32(_mm_setr_epi8(
            left[base_y_c[0] + 1], left[base_y_c[1] + 1], left[base_y_c[2] + 1],
            left[base_y_c[3] + 1], left[base_y_c[4] + 1], left[base_y_c[5] + 1],
            left[base_y_c[6] + 1], left[base_y_c[7] + 1], 0, 0, 0, 0, 0, 0, 0,
            0));

        shift = _mm256_srli_epi32(_mm256_and_si256(y_c256, c3f), 1);

        diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        res = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
        resy[0] = _mm_packus_epi16(_mm256_castsi256_si128(res),
                                   _mm256_castsi256_si128(res));

        a0_y = _mm256_cvtepu8_epi32(_mm_setr_epi8(
            left[base_y_c[8]], left[base_y_c[9]], left[base_y_c[10]],
            left[base_y_c[11]], left[base_y_c[12]], left[base_y_c[13]],
            left[base_y_c[14]], left[base_y_c[15]], 0, 0, 0, 0, 0, 0, 0, 0));
        a1_y = _mm256_cvtepu8_epi32(
            _mm_setr_epi8(left[base_y_c[8] + 1], left[base_y_c[9] + 1],
                          left[base_y_c[10] + 1], left[base_y_c[11] + 1],
                          left[base_y_c[12] + 1], left[base_y_c[13] + 1],
                          left[base_y_c[14] + 1], left[base_y_c[15] + 1], 0, 0,
                          0, 0, 0, 0, 0, 0));
        shift = _mm256_srli_epi32(_mm256_and_si256(y_c_1_256, c3f), 1);

        diff = _mm256_sub_epi32(a1_y, a0_y);  // a[x+1] - a[x]
        a32 = _mm256_slli_epi32(a0_y, 5);     // a[x] * 32
        a32 = _mm256_add_epi32(a32, a16);     // a[x] * 32 + 16

        b = _mm256_mullo_epi32(diff, shift);
        res = _mm256_add_epi32(a32, b);
        res = _mm256_srli_epi32(res, 5);

        res = _mm256_packus_epi32(
            res, _mm256_castsi128_si256(_mm256_extracti128_si256(res, 1)));
        resy[1] =
            _mm_packus_epi16(_mm256_castsi256_si128(res),
                             _mm256_castsi256_si128(res));  // 8 16bit values
        resy[0] = _mm_unpacklo_epi64(resy[0], resy[1]);
      } else {
        resy[0] = resx[0];
      }
      resxy = _mm_blendv_epi8(resx[0], resy[0],
                              *(__m128i *)BaseMask[base_min_diff]);
      _mm_storeu_si128((__m128i *)(dst + j), resxy);
    }  // for j
    dst += stride;
  }
}

// Directional prediction, zone 2: 90 < angle < 180
void av1_dr_prediction_z2_avx2(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_above, int upsample_left, int dx,
                               int dy) {
  assert(dx > 0);
  assert(dy > 0);
  switch (bw) {
    case 4:
      dr_prediction_z2_Nx4_avx2(bh, dst, stride, above, left, upsample_above,
                                upsample_left, dx, dy);
      break;
    case 8:
      dr_prediction_z2_Nx8_avx2(bh, dst, stride, above, left, upsample_above,
                                upsample_left, dx, dy);

      break;
    default:
      dr_prediction_z2_HxW_avx2(bh, bw, dst, stride, above, left,
                                upsample_above, upsample_left, dx, dy);
      break;
  }
  return;
}

// z3 functions
static INLINE void transpose4x16_sse2(__m128i *x, __m128i *d) {
  __m128i w0, w1, w2, w3, ww0, ww1, ww2, ww3;
  w0 = _mm_unpacklo_epi8(x[0], x[1]);
  w1 = _mm_unpacklo_epi8(x[2], x[3]);
  w2 = _mm_unpackhi_epi8(x[0], x[1]);
  w3 = _mm_unpackhi_epi8(x[2], x[3]);

  ww0 = _mm_unpacklo_epi16(w0, w1);
  ww1 = _mm_unpacklo_epi16(w2, w3);
  ww2 = _mm_unpackhi_epi16(w0, w1);
  ww3 = _mm_unpackhi_epi16(w2, w3);

  w0 = _mm_unpacklo_epi32(ww0, ww1);
  w2 = _mm_unpacklo_epi32(ww2, ww3);
  w1 = _mm_unpackhi_epi32(ww0, ww1);
  w3 = _mm_unpackhi_epi32(ww2, ww3);

  d[0] = _mm_unpacklo_epi64(w0, w2);
  d[1] = _mm_unpackhi_epi64(w0, w2);
  d[2] = _mm_unpacklo_epi64(w1, w3);
  d[3] = _mm_unpackhi_epi64(w1, w3);

  d[4] = _mm_srli_si128(d[0], 8);
  d[5] = _mm_srli_si128(d[1], 8);
  d[6] = _mm_srli_si128(d[2], 8);
  d[7] = _mm_srli_si128(d[3], 8);

  d[8] = _mm_srli_si128(d[0], 4);
  d[9] = _mm_srli_si128(d[1], 4);
  d[10] = _mm_srli_si128(d[2], 4);
  d[11] = _mm_srli_si128(d[3], 4);

  d[12] = _mm_srli_si128(d[0], 12);
  d[13] = _mm_srli_si128(d[1], 12);
  d[14] = _mm_srli_si128(d[2], 12);
  d[15] = _mm_srli_si128(d[3], 12);
}

static INLINE void transpose16x32_avx2(__m256i *x, __m256i *d) {
  __m256i w0, w1, w2, w3, w4, w5, w6, w7, w8, w9;
  __m256i w10, w11, w12, w13, w14, w15;

  w0 = _mm256_unpacklo_epi8(x[0], x[1]);
  w1 = _mm256_unpacklo_epi8(x[2], x[3]);
  w2 = _mm256_unpacklo_epi8(x[4], x[5]);
  w3 = _mm256_unpacklo_epi8(x[6], x[7]);

  w8 = _mm256_unpacklo_epi8(x[8], x[9]);
  w9 = _mm256_unpacklo_epi8(x[10], x[11]);
  w10 = _mm256_unpacklo_epi8(x[12], x[13]);
  w11 = _mm256_unpacklo_epi8(x[14], x[15]);

  w4 = _mm256_unpacklo_epi16(w0, w1);
  w5 = _mm256_unpacklo_epi16(w2, w3);
  w12 = _mm256_unpacklo_epi16(w8, w9);
  w13 = _mm256_unpacklo_epi16(w10, w11);

  w6 = _mm256_unpacklo_epi32(w4, w5);
  w7 = _mm256_unpackhi_epi32(w4, w5);
  w14 = _mm256_unpacklo_epi32(w12, w13);
  w15 = _mm256_unpackhi_epi32(w12, w13);

  // Store first 4-line result
  d[0] = _mm256_unpacklo_epi64(w6, w14);
  d[1] = _mm256_unpackhi_epi64(w6, w14);
  d[2] = _mm256_unpacklo_epi64(w7, w15);
  d[3] = _mm256_unpackhi_epi64(w7, w15);

  w4 = _mm256_unpackhi_epi16(w0, w1);
  w5 = _mm256_unpackhi_epi16(w2, w3);
  w12 = _mm256_unpackhi_epi16(w8, w9);
  w13 = _mm256_unpackhi_epi16(w10, w11);

  w6 = _mm256_unpacklo_epi32(w4, w5);
  w7 = _mm256_unpackhi_epi32(w4, w5);
  w14 = _mm256_unpacklo_epi32(w12, w13);
  w15 = _mm256_unpackhi_epi32(w12, w13);

  // Store second 4-line result
  d[4] = _mm256_unpacklo_epi64(w6, w14);
  d[5] = _mm256_unpackhi_epi64(w6, w14);
  d[6] = _mm256_unpacklo_epi64(w7, w15);
  d[7] = _mm256_unpackhi_epi64(w7, w15);

  // upper half
  w0 = _mm256_unpackhi_epi8(x[0], x[1]);
  w1 = _mm256_unpackhi_epi8(x[2], x[3]);
  w2 = _mm256_unpackhi_epi8(x[4], x[5]);
  w3 = _mm256_unpackhi_epi8(x[6], x[7]);

  w8 = _mm256_unpackhi_epi8(x[8], x[9]);
  w9 = _mm256_unpackhi_epi8(x[10], x[11]);
  w10 = _mm256_unpackhi_epi8(x[12], x[13]);
  w11 = _mm256_unpackhi_epi8(x[14], x[15]);

  w4 = _mm256_unpacklo_epi16(w0, w1);
  w5 = _mm256_unpacklo_epi16(w2, w3);
  w12 = _mm256_unpacklo_epi16(w8, w9);
  w13 = _mm256_unpacklo_epi16(w10, w11);

  w6 = _mm256_unpacklo_epi32(w4, w5);
  w7 = _mm256_unpackhi_epi32(w4, w5);
  w14 = _mm256_unpacklo_epi32(w12, w13);
  w15 = _mm256_unpackhi_epi32(w12, w13);

  // Store first 4-line result
  d[8] = _mm256_unpacklo_epi64(w6, w14);
  d[9] = _mm256_unpackhi_epi64(w6, w14);
  d[10] = _mm256_unpacklo_epi64(w7, w15);
  d[11] = _mm256_unpackhi_epi64(w7, w15);

  w4 = _mm256_unpackhi_epi16(w0, w1);
  w5 = _mm256_unpackhi_epi16(w2, w3);
  w12 = _mm256_unpackhi_epi16(w8, w9);
  w13 = _mm256_unpackhi_epi16(w10, w11);

  w6 = _mm256_unpacklo_epi32(w4, w5);
  w7 = _mm256_unpackhi_epi32(w4, w5);
  w14 = _mm256_unpacklo_epi32(w12, w13);
  w15 = _mm256_unpackhi_epi32(w12, w13);

  // Store second 4-line result
  d[12] = _mm256_unpacklo_epi64(w6, w14);
  d[13] = _mm256_unpackhi_epi64(w6, w14);
  d[14] = _mm256_unpacklo_epi64(w7, w15);
  d[15] = _mm256_unpackhi_epi64(w7, w15);
}

static INLINE void transpose16x16_sse2(__m128i *x, __m128i *d) {
  __m128i w0, w1, w2, w3, w4, w5, w6, w7, w8, w9;
  __m128i w10, w11, w12, w13, w14, w15;

  w0 = _mm_unpacklo_epi8(x[0], x[1]);
  w1 = _mm_unpacklo_epi8(x[2], x[3]);
  w2 = _mm_unpacklo_epi8(x[4], x[5]);
  w3 = _mm_unpacklo_epi8(x[6], x[7]);

  w8 = _mm_unpacklo_epi8(x[8], x[9]);
  w9 = _mm_unpacklo_epi8(x[10], x[11]);
  w10 = _mm_unpacklo_epi8(x[12], x[13]);
  w11 = _mm_unpacklo_epi8(x[14], x[15]);

  w4 = _mm_unpacklo_epi16(w0, w1);
  w5 = _mm_unpacklo_epi16(w2, w3);
  w12 = _mm_unpacklo_epi16(w8, w9);
  w13 = _mm_unpacklo_epi16(w10, w11);

  w6 = _mm_unpacklo_epi32(w4, w5);
  w7 = _mm_unpackhi_epi32(w4, w5);
  w14 = _mm_unpacklo_epi32(w12, w13);
  w15 = _mm_unpackhi_epi32(w12, w13);

  // Store first 4-line result
  d[0] = _mm_unpacklo_epi64(w6, w14);
  d[1] = _mm_unpackhi_epi64(w6, w14);
  d[2] = _mm_unpacklo_epi64(w7, w15);
  d[3] = _mm_unpackhi_epi64(w7, w15);

  w4 = _mm_unpackhi_epi16(w0, w1);
  w5 = _mm_unpackhi_epi16(w2, w3);
  w12 = _mm_unpackhi_epi16(w8, w9);
  w13 = _mm_unpackhi_epi16(w10, w11);

  w6 = _mm_unpacklo_epi32(w4, w5);
  w7 = _mm_unpackhi_epi32(w4, w5);
  w14 = _mm_unpacklo_epi32(w12, w13);
  w15 = _mm_unpackhi_epi32(w12, w13);

  // Store second 4-line result
  d[4] = _mm_unpacklo_epi64(w6, w14);
  d[5] = _mm_unpackhi_epi64(w6, w14);
  d[6] = _mm_unpacklo_epi64(w7, w15);
  d[7] = _mm_unpackhi_epi64(w7, w15);

  // upper half
  w0 = _mm_unpackhi_epi8(x[0], x[1]);
  w1 = _mm_unpackhi_epi8(x[2], x[3]);
  w2 = _mm_unpackhi_epi8(x[4], x[5]);
  w3 = _mm_unpackhi_epi8(x[6], x[7]);

  w8 = _mm_unpackhi_epi8(x[8], x[9]);
  w9 = _mm_unpackhi_epi8(x[10], x[11]);
  w10 = _mm_unpackhi_epi8(x[12], x[13]);
  w11 = _mm_unpackhi_epi8(x[14], x[15]);

  w4 = _mm_unpacklo_epi16(w0, w1);
  w5 = _mm_unpacklo_epi16(w2, w3);
  w12 = _mm_unpacklo_epi16(w8, w9);
  w13 = _mm_unpacklo_epi16(w10, w11);

  w6 = _mm_unpacklo_epi32(w4, w5);
  w7 = _mm_unpackhi_epi32(w4, w5);
  w14 = _mm_unpacklo_epi32(w12, w13);
  w15 = _mm_unpackhi_epi32(w12, w13);

  // Store first 4-line result
  d[8] = _mm_unpacklo_epi64(w6, w14);
  d[9] = _mm_unpackhi_epi64(w6, w14);
  d[10] = _mm_unpacklo_epi64(w7, w15);
  d[11] = _mm_unpackhi_epi64(w7, w15);

  w4 = _mm_unpackhi_epi16(w0, w1);
  w5 = _mm_unpackhi_epi16(w2, w3);
  w12 = _mm_unpackhi_epi16(w8, w9);
  w13 = _mm_unpackhi_epi16(w10, w11);

  w6 = _mm_unpacklo_epi32(w4, w5);
  w7 = _mm_unpackhi_epi32(w4, w5);
  w14 = _mm_unpacklo_epi32(w12, w13);
  w15 = _mm_unpackhi_epi32(w12, w13);

  // Store second 4-line result
  d[12] = _mm_unpacklo_epi64(w6, w14);
  d[13] = _mm_unpackhi_epi64(w6, w14);
  d[14] = _mm_unpacklo_epi64(w7, w15);
  d[15] = _mm_unpackhi_epi64(w7, w15);
}

static void transpose_TX_8X8(const uint8_t *src, ptrdiff_t pitchSrc,
                             uint8_t *dst, ptrdiff_t pitchDst) {
  __m128i r0, r1, r2, r3, r4, r5, r6, r7;
  __m128i d0d1, d2d3, d4d5, d6d7;
  r0 = _mm_loadl_epi64((__m128i *)(src + 0 * pitchSrc));
  r1 = _mm_loadl_epi64((__m128i *)(src + 1 * pitchSrc));
  r2 = _mm_loadl_epi64((__m128i *)(src + 2 * pitchSrc));
  r3 = _mm_loadl_epi64((__m128i *)(src + 3 * pitchSrc));
  r4 = _mm_loadl_epi64((__m128i *)(src + 4 * pitchSrc));
  r5 = _mm_loadl_epi64((__m128i *)(src + 5 * pitchSrc));
  r6 = _mm_loadl_epi64((__m128i *)(src + 6 * pitchSrc));
  r7 = _mm_loadl_epi64((__m128i *)(src + 7 * pitchSrc));

  transpose8x8_sse2(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7, &d0d1, &d2d3, &d4d5,
                    &d6d7);

  _mm_storel_epi64((__m128i *)(dst + 0 * pitchDst), d0d1);
  _mm_storel_epi64((__m128i *)(dst + 1 * pitchDst), _mm_srli_si128(d0d1, 8));
  _mm_storel_epi64((__m128i *)(dst + 2 * pitchDst), d2d3);
  _mm_storel_epi64((__m128i *)(dst + 3 * pitchDst), _mm_srli_si128(d2d3, 8));
  _mm_storel_epi64((__m128i *)(dst + 4 * pitchDst), d4d5);
  _mm_storel_epi64((__m128i *)(dst + 5 * pitchDst), _mm_srli_si128(d4d5, 8));
  _mm_storel_epi64((__m128i *)(dst + 6 * pitchDst), d6d7);
  _mm_storel_epi64((__m128i *)(dst + 7 * pitchDst), _mm_srli_si128(d6d7, 8));
}

static void transpose(const uint8_t *src, ptrdiff_t pitchSrc, uint8_t *dst,
                      ptrdiff_t pitchDst, int width, int height) {
  for (int j = 0; j < height; j += 8)
    for (int i = 0; i < width; i += 8)
      transpose_TX_8X8(src + i * pitchSrc + j, pitchSrc, dst + j * pitchDst + i,
                       pitchDst);
}

static void dr_prediction_z3_4x4_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  __m128i dstvec[4], d[4];

  dr_prediction_z1_4xN_internal_avx2(4, dstvec, left, upsample_left, dy);
  transpose4x8_8x4_low_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3],
                            &d[0], &d[1], &d[2], &d[3]);

  *(uint32_t *)(dst + stride * 0) = _mm_cvtsi128_si32(d[0]);
  *(uint32_t *)(dst + stride * 1) = _mm_cvtsi128_si32(d[1]);
  *(uint32_t *)(dst + stride * 2) = _mm_cvtsi128_si32(d[2]);
  *(uint32_t *)(dst + stride * 3) = _mm_cvtsi128_si32(d[3]);
  return;
}

static void dr_prediction_z3_8x8_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  __m128i dstvec[8], d[8];

  dr_prediction_z1_8xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  transpose8x8_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3], &dstvec[4],
                    &dstvec[5], &dstvec[6], &dstvec[7], &d[0], &d[1], &d[2],
                    &d[3]);

  _mm_storel_epi64((__m128i *)(dst + 0 * stride), d[0]);
  _mm_storel_epi64((__m128i *)(dst + 1 * stride), _mm_srli_si128(d[0], 8));
  _mm_storel_epi64((__m128i *)(dst + 2 * stride), d[1]);
  _mm_storel_epi64((__m128i *)(dst + 3 * stride), _mm_srli_si128(d[1], 8));
  _mm_storel_epi64((__m128i *)(dst + 4 * stride), d[2]);
  _mm_storel_epi64((__m128i *)(dst + 5 * stride), _mm_srli_si128(d[2], 8));
  _mm_storel_epi64((__m128i *)(dst + 6 * stride), d[3]);
  _mm_storel_epi64((__m128i *)(dst + 7 * stride), _mm_srli_si128(d[3], 8));
}

static void dr_prediction_z3_4x8_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  __m128i dstvec[4], d[8];

  dr_prediction_z1_8xN_internal_avx2(4, dstvec, left, upsample_left, dy);
  transpose4x8_8x4_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3], &d[0],
                        &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]);
  for (int i = 0; i < 8; i++) {
    *(uint32_t *)(dst + stride * i) = _mm_cvtsi128_si32(d[i]);
  }
}

static void dr_prediction_z3_8x4_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  __m128i dstvec[8], d[4];

  dr_prediction_z1_4xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  transpose8x8_low_sse2(&dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3],
                        &dstvec[4], &dstvec[5], &dstvec[6], &dstvec[7], &d[0],
                        &d[1], &d[2], &d[3]);
  _mm_storel_epi64((__m128i *)(dst + 0 * stride), d[0]);
  _mm_storel_epi64((__m128i *)(dst + 1 * stride), d[1]);
  _mm_storel_epi64((__m128i *)(dst + 2 * stride), d[2]);
  _mm_storel_epi64((__m128i *)(dst + 3 * stride), d[3]);
}

static void dr_prediction_z3_8x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m128i dstvec[8], d[8];

  dr_prediction_z1_16xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  transpose8x16_16x8_sse2(dstvec, dstvec + 1, dstvec + 2, dstvec + 3,
                          dstvec + 4, dstvec + 5, dstvec + 6, dstvec + 7, d,
                          d + 1, d + 2, d + 3, d + 4, d + 5, d + 6, d + 7);
  for (int i = 0; i < 8; i++) {
    _mm_storel_epi64((__m128i *)(dst + i * stride), d[i]);
    _mm_storel_epi64((__m128i *)(dst + (i + 8) * stride),
                     _mm_srli_si128(d[i], 8));
  }
}

static void dr_prediction_z3_16x8_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m128i dstvec[16], d[16];

  dr_prediction_z1_8xN_internal_avx2(16, dstvec, left, upsample_left, dy);
  transpose16x8_8x16_sse2(
      &dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3], &dstvec[4], &dstvec[5],
      &dstvec[6], &dstvec[7], &dstvec[8], &dstvec[9], &dstvec[10], &dstvec[11],
      &dstvec[12], &dstvec[13], &dstvec[14], &dstvec[15], &d[0], &d[1], &d[2],
      &d[3], &d[4], &d[5], &d[6], &d[7]);

  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
  }
}

static void dr_prediction_z3_4x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m128i dstvec[4], d[16];

  dr_prediction_z1_16xN_internal_avx2(4, dstvec, left, upsample_left, dy);
  transpose4x16_sse2(dstvec, d);
  for (int i = 0; i < 16; i++) {
    *(uint32_t *)(dst + stride * i) = _mm_cvtsi128_si32(d[i]);
  }
}

static void dr_prediction_z3_16x4_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m128i dstvec[16], d[8];

  dr_prediction_z1_4xN_internal_avx2(16, dstvec, left, upsample_left, dy);
  for (int i = 4; i < 8; i++) {
    d[i] = _mm_setzero_si128();
  }
  transpose16x8_8x16_sse2(
      &dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3], &dstvec[4], &dstvec[5],
      &dstvec[6], &dstvec[7], &dstvec[8], &dstvec[9], &dstvec[10], &dstvec[11],
      &dstvec[12], &dstvec[13], &dstvec[14], &dstvec[15], &d[0], &d[1], &d[2],
      &d[3], &d[4], &d[5], &d[6], &d[7]);

  for (int i = 0; i < 4; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
  }
}

static void dr_prediction_z3_8x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m256i dstvec[16], d[16];

  dr_prediction_z1_32xN_internal_avx2(8, dstvec, left, upsample_left, dy);
  for (int i = 8; i < 16; i++) {
    dstvec[i] = _mm256_setzero_si256();
  }
  transpose16x32_avx2(dstvec, d);

  for (int i = 0; i < 16; i++) {
    _mm_storel_epi64((__m128i *)(dst + i * stride),
                     _mm256_castsi256_si128(d[i]));
  }
  for (int i = 0; i < 16; i++) {
    _mm_storel_epi64((__m128i *)(dst + (i + 16) * stride),
                     _mm256_extracti128_si256(d[i], 1));
  }
}

static void dr_prediction_z3_32x8_avx2(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  __m128i dstvec[32], d[16];

  dr_prediction_z1_8xN_internal_avx2(32, dstvec, left, upsample_left, dy);

  transpose16x8_8x16_sse2(
      &dstvec[0], &dstvec[1], &dstvec[2], &dstvec[3], &dstvec[4], &dstvec[5],
      &dstvec[6], &dstvec[7], &dstvec[8], &dstvec[9], &dstvec[10], &dstvec[11],
      &dstvec[12], &dstvec[13], &dstvec[14], &dstvec[15], &d[0], &d[1], &d[2],
      &d[3], &d[4], &d[5], &d[6], &d[7]);
  transpose16x8_8x16_sse2(
      &dstvec[0 + 16], &dstvec[1 + 16], &dstvec[2 + 16], &dstvec[3 + 16],
      &dstvec[4 + 16], &dstvec[5 + 16], &dstvec[6 + 16], &dstvec[7 + 16],
      &dstvec[8 + 16], &dstvec[9 + 16], &dstvec[10 + 16], &dstvec[11 + 16],
      &dstvec[12 + 16], &dstvec[13 + 16], &dstvec[14 + 16], &dstvec[15 + 16],
      &d[0 + 8], &d[1 + 8], &d[2 + 8], &d[3 + 8], &d[4 + 8], &d[5 + 8],
      &d[6 + 8], &d[7 + 8]);

  for (int i = 0; i < 8; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
    _mm_storeu_si128((__m128i *)(dst + i * stride + 16), d[i + 8]);
  }
}

static void dr_prediction_z3_16x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  __m128i dstvec[16], d[16];

  dr_prediction_z1_16xN_internal_avx2(16, dstvec, left, upsample_left, dy);
  transpose16x16_sse2(dstvec, d);

  for (int i = 0; i < 16; i++) {
    _mm_storeu_si128((__m128i *)(dst + i * stride), d[i]);
  }
}

static void dr_prediction_z3_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  __m256i dstvec[32], d[32];

  dr_prediction_z1_32xN_internal_avx2(32, dstvec, left, upsample_left, dy);
  transpose16x32_avx2(dstvec, d);
  transpose16x32_avx2(dstvec + 16, d + 16);
  for (int j = 0; j < 16; j++) {
    _mm_storeu_si128((__m128i *)(dst + j * stride),
                     _mm256_castsi256_si128(d[j]));
    _mm_storeu_si128((__m128i *)(dst + j * stride + 16),
                     _mm256_castsi256_si128(d[j + 16]));
  }
  for (int j = 0; j < 16; j++) {
    _mm_storeu_si128((__m128i *)(dst + (j + 16) * stride),
                     _mm256_extracti128_si256(d[j], 1));
    _mm_storeu_si128((__m128i *)(dst + (j + 16) * stride + 16),
                     _mm256_extracti128_si256(d[j + 16], 1));
  }
}

static void dr_prediction_z3_64x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  DECLARE_ALIGNED(16, uint8_t, dstT[64 * 64]);
  dr_prediction_z1_64xN_avx2(64, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 64, 64);
}

static void dr_prediction_z3_16x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  __m256i dstvec[16], d[16];

  dr_prediction_z1_32xN_internal_avx2(16, dstvec, left, upsample_left, dy);
  transpose16x32_avx2(dstvec, d);
  // store
  for (int j = 0; j < 16; j++) {
    _mm_storeu_si128((__m128i *)(dst + j * stride),
                     _mm256_castsi256_si128(d[j]));
    _mm_storeu_si128((__m128i *)(dst + (j + 16) * stride),
                     _mm256_extracti128_si256(d[j], 1));
  }
}

static void dr_prediction_z3_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  __m128i dstvec[32], d[16];

  dr_prediction_z1_16xN_internal_avx2(32, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 32; i += 16) {
    transpose16x16_sse2((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      _mm_storeu_si128((__m128i *)(dst + j * stride + i), d[j]);
    }
  }
}

static void dr_prediction_z3_32x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[64 * 32];
  dr_prediction_z1_64xN_avx2(32, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 32, 64);
}

static void dr_prediction_z3_64x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[32 * 64];
  dr_prediction_z1_32xN_avx2(64, dstT, 32, left, upsample_left, dy);
  transpose(dstT, 32, dst, stride, 64, 32);
  return;
}

static void dr_prediction_z3_16x64_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[64 * 16];
  dr_prediction_z1_64xN_avx2(16, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 16, 64);
}

static void dr_prediction_z3_64x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  __m128i dstvec[64], d[16];

  dr_prediction_z1_16xN_internal_avx2(64, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 64; i += 16) {
    transpose16x16_sse2((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      _mm_storeu_si128((__m128i *)(dst + j * stride + i), d[j]);
    }
  }
}

void av1_dr_prediction_z3_avx2(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_left, int dx, int dy) {
  (void)above;
  (void)dx;
  assert(dx == 1);
  assert(dy > 0);

  if (bw == bh) {
    switch (bw) {
      case 4:
        dr_prediction_z3_4x4_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 8:
        dr_prediction_z3_8x8_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 16:
        dr_prediction_z3_16x16_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 32:
        dr_prediction_z3_32x32_avx2(dst, stride, left, upsample_left, dy);
        break;
      case 64:
        dr_prediction_z3_64x64_avx2(dst, stride, left, upsample_left, dy);
        break;
    }
  } else {
    if (bw < bh) {
      if (bw + bw == bh) {
        switch (bw) {
          case 4:
            dr_prediction_z3_4x8_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_8x16_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_16x32_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 32:
            dr_prediction_z3_32x64_avx2(dst, stride, left, upsample_left, dy);
            break;
        }
      } else {
        switch (bw) {
          case 4:
            dr_prediction_z3_4x16_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_8x32_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_16x64_avx2(dst, stride, left, upsample_left, dy);
            break;
        }
      }
    } else {
      if (bh + bh == bw) {
        switch (bh) {
          case 4:
            dr_prediction_z3_8x4_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_16x8_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_32x16_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 32:
            dr_prediction_z3_64x32_avx2(dst, stride, left, upsample_left, dy);
            break;
        }
      } else {
        switch (bh) {
          case 4:
            dr_prediction_z3_16x4_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_32x8_avx2(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_64x16_avx2(dst, stride, left, upsample_left, dy);
            break;
        }
      }
    }
  }
  return;
}
