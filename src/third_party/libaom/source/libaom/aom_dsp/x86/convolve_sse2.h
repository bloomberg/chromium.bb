/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>  // SSSE3

#include "av1/common/resize.h"
#include "config/av1_rtcd.h"
#include "config/aom_scale_rtcd.h"

#ifndef AOM_AOM_DSP_X86_CONVOLVE_SSE2_H_
#define AOM_AOM_DSP_X86_CONVOLVE_SSE2_H_

// Note:
//  This header file should be put below any x86 intrinsics head file

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m128i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeff = _mm_loadu_si128((__m128i *)filter);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm_shuffle_epi32(coeff, 0xff);
}

static INLINE __m128i convolve(const __m128i *const s,
                               const __m128i *const coeffs) {
  const __m128i res_0 = _mm_madd_epi16(s[0], coeffs[0]);
  const __m128i res_1 = _mm_madd_epi16(s[1], coeffs[1]);
  const __m128i res_2 = _mm_madd_epi16(s[2], coeffs[2]);
  const __m128i res_3 = _mm_madd_epi16(s[3], coeffs[3]);

  const __m128i res =
      _mm_add_epi32(_mm_add_epi32(res_0, res_1), _mm_add_epi32(res_2, res_3));

  return res;
}

static INLINE __m128i convolve_lo_x(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[1], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[3], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_lo_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_hi_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpackhi_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpackhi_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpackhi_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpackhi_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i comp_avg(const __m128i *const data_ref_0,
                               const __m128i *const res_unsigned,
                               const __m128i *const wt,
                               const int use_dist_wtd_avg) {
  __m128i res;
  if (use_dist_wtd_avg) {
    const __m128i data_lo = _mm_unpacklo_epi16(*data_ref_0, *res_unsigned);
    const __m128i data_hi = _mm_unpackhi_epi16(*data_ref_0, *res_unsigned);

    const __m128i wt_res_lo = _mm_madd_epi16(data_lo, *wt);
    const __m128i wt_res_hi = _mm_madd_epi16(data_hi, *wt);

    const __m128i res_lo = _mm_srai_epi32(wt_res_lo, DIST_PRECISION_BITS);
    const __m128i res_hi = _mm_srai_epi32(wt_res_hi, DIST_PRECISION_BITS);

    res = _mm_packs_epi32(res_lo, res_hi);
  } else {
    const __m128i wt_res = _mm_add_epi16(*data_ref_0, *res_unsigned);
    res = _mm_srai_epi16(wt_res, 1);
  }
  return res;
}

static INLINE __m128i convolve_rounding(const __m128i *const res_unsigned,
                                        const __m128i *const offset_const,
                                        const __m128i *const round_const,
                                        const int round_shift) {
  const __m128i res_signed = _mm_sub_epi16(*res_unsigned, *offset_const);
  const __m128i res_round =
      _mm_srai_epi16(_mm_add_epi16(res_signed, *round_const), round_shift);
  return res_round;
}

static INLINE __m128i highbd_convolve_rounding_sse2(
    const __m128i *const res_unsigned, const __m128i *const offset_const,
    const __m128i *const round_const, const int round_shift) {
  const __m128i res_signed = _mm_sub_epi32(*res_unsigned, *offset_const);
  const __m128i res_round =
      _mm_srai_epi32(_mm_add_epi32(res_signed, *round_const), round_shift);

  return res_round;
}

static INLINE void shuffle_filter_ssse3(const int16_t *const filter,
                                        __m128i *const f) {
  const __m128i f_values = _mm_load_si128((const __m128i *)filter);
  // pack and duplicate the filter values
  f[0] = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  f[1] = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  f[2] = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  f[3] = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
}

static INLINE __m128i convolve8_8_ssse3(const __m128i *const s,
                                        const __m128i *const f) {
  // multiply 2 adjacent elements with the filter and add the result
  const __m128i k_64 = _mm_set1_epi16(1 << 6);
  const __m128i x0 = _mm_maddubs_epi16(s[0], f[0]);
  const __m128i x1 = _mm_maddubs_epi16(s[1], f[1]);
  const __m128i x2 = _mm_maddubs_epi16(s[2], f[2]);
  const __m128i x3 = _mm_maddubs_epi16(s[3], f[3]);
  __m128i sum1, sum2;

  // sum the results together, saturating only on the final step
  // adding x0 with x2 and x1 with x3 is the only order that prevents
  // outranges for all filters
  sum1 = _mm_add_epi16(x0, x2);
  sum2 = _mm_add_epi16(x1, x3);
  // add the rounding offset early to avoid another saturated add
  sum1 = _mm_add_epi16(sum1, k_64);
  sum1 = _mm_adds_epi16(sum1, sum2);
  // shift by 7 bit each 16 bit
  sum1 = _mm_srai_epi16(sum1, 7);
  return sum1;
}

#endif  // AOM_AOM_DSP_X86_CONVOLVE_SSE2_H_
