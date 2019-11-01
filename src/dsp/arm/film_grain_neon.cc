// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/film_grain.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON
#include <arm_neon.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/arm/film_grain_neon.h"
#include "src/dsp/common.h"
#include "src/dsp/dsp.h"
#include "src/dsp/film_grain_impl.h"
#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/logging.h"

namespace libgav1 {
namespace dsp {
namespace {

// Section 7.18.3.1.
template <int bitdepth>
bool FilmGrainSynthesis_NEON(
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v,
    const FilmGrainParams& film_grain_params, const bool is_monochrome,
    const bool color_matrix_is_identity, const int width, const int height,
    const int subsampling_x, const int subsampling_y, void* dest_plane_y,
    ptrdiff_t dest_stride_y, void* dest_plane_u, ptrdiff_t dest_stride_u,
    void* dest_plane_v, ptrdiff_t dest_stride_v) {
  FilmGrain<bitdepth> film_grain(film_grain_params, is_monochrome,
                                 color_matrix_is_identity, subsampling_x,
                                 subsampling_y, width, height);
  return film_grain.AddNoise_NEON(
      source_plane_y, source_stride_y, source_plane_u, source_stride_u,
      source_plane_v, source_stride_v, dest_plane_y, dest_stride_y,
      dest_plane_u, dest_stride_u, dest_plane_v, dest_stride_v);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
  dsp->film_grain_synthesis = FilmGrainSynthesis_NEON<8>;
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
  dsp->film_grain_synthesis = FilmGrainSynthesis_NEON<10>;
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

// This function is overloaded for both possible GrainTypes in order to simplify
// loading in a template function.
inline int16x8_t GetSource8(const int8_t* src) {
  return vmovl_s8(vld1_s8(src));
}

#if LIBGAV1_MAX_BITDEPTH >= 10
inline int16x8_t GetSource8(const int16_t* src) { return vld1q_s16(src); }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

// Each element in |sum| represents one destination value's running
// autoregression formula. The fixed source values in |grain_lo| and |grain_hi|
// allow for a sliding window in successive calls to this function.
template <int position_offset>
inline int32x4x2_t AccumulateWeightedGrain(const int16x8_t grain_lo,
                                           const int16x8_t grain_hi,
                                           int16_t coeff, int32x4x2_t sum) {
  const int16x8_t grain = vextq_s16(grain_lo, grain_hi, position_offset);
  sum.val[0] = vmlal_n_s16(sum.val[0], vget_low_s16(grain), coeff);
  sum.val[1] = vmlal_n_s16(sum.val[1], vget_high_s16(grain), coeff);
  return sum;
}

// Because the autoregressive filter requires the output of each pixel to
// compute pixels that come after in the row, we have to finish the calculations
// one at a time.
template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegression(int8_t* grain_cursor, int32x4x2_t sum,
                                     const int8_t* coeffs, int pos, int shift) {
  int32_t result = vgetq_lane_s32(sum.val[lane >> 2], lane & 3);

  for (int delta_col = -auto_regression_coeff_lag; delta_col < 0; ++delta_col) {
    result += grain_cursor[lane + delta_col] * coeffs[pos];
    ++pos;
  }
  grain_cursor[lane] =
      Clip3(grain_cursor[lane] + RightShiftWithRounding(result, shift),
            GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>());
}

#if LIBGAV1_MAX_BITDEPTH >= 10
template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegression(int16_t* grain_cursor, int32x4x2_t sum,
                                     const int8_t* coeffs, int pos, int shift) {
  int32_t result = vgetq_lane_s32(sum.val[lane >> 2], lane & 3);

  for (int delta_col = -auto_regression_coeff_lag; delta_col < 0; ++delta_col) {
    result += grain_cursor[lane + delta_col] * coeffs[pos];
    ++pos;
  }
  grain_cursor[lane] =
      Clip3(grain_cursor[lane] + RightShiftWithRounding(result, shift),
            GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>());
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

// Because the autoregressive filter requires the output of each pixel to
// compute pixels that come after in the row, we have to finish the calculations
// one at a time.
template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegressionChroma(int8_t* u_grain_cursor,
                                           int8_t* v_grain_cursor,
                                           int32x4x2_t sum_u, int32x4x2_t sum_v,
                                           const int8_t* coeffs_u,
                                           const int8_t* coeffs_v, int pos,
                                           int shift) {
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>(
      u_grain_cursor, sum_u, coeffs_u, pos, shift);
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>(
      v_grain_cursor, sum_v, coeffs_v, pos, shift);
}

#if LIBGAV1_MAX_BITDEPTH >= 10
template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegressionChroma(int16_t* u_grain_cursor,
                                           int16_t* v_grain_cursor,
                                           int32x4x2_t sum_u, int32x4x2_t sum_v,
                                           const int8_t* coeffs_u,
                                           const int8_t* coeffs_v, int pos,
                                           int shift) {
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>(
      u_grain_cursor, sum_u, coeffs_u, pos, shift);
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>(
      v_grain_cursor, sum_v, coeffs_v, pos, shift);
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

inline void SetZero(int32x4x2_t* v) {
  v->val[0] = vdupq_n_s32(0);
  v->val[1] = vdupq_n_s32(0);
}

// Computes subsampled luma for use with chroma, by averaging in the x direction
// or y direction when applicable.
template <int subsampling_x, int subsampling_y>
int16x8_t GetSubsampledLuma(const int8_t* const luma, ptrdiff_t stride) {
  if (subsampling_y != 0) {
    assert(subsampling_x != 0);
    const int8x16_t src0 = vld1q_s8(luma);
    const int8x16_t src1 = vld1q_s8(luma + stride);
    const int16x8_t ret0 = vcombine_s16(vpaddl_s8(vget_low_s8(src0)),
                                        vpaddl_s8(vget_high_s8(src0)));
    const int16x8_t ret1 = vcombine_s16(vpaddl_s8(vget_low_s8(src1)),
                                        vpaddl_s8(vget_high_s8(src1)));
    return vrshrq_n_s16(vaddq_s16(ret0, ret1), 2);
  }
  if (subsampling_x != 0) {
    const int8x16_t src = vld1q_s8(luma);
    return vrshrq_n_s16(
        vcombine_s16(vpaddl_s8(vget_low_s8(src)), vpaddl_s8(vget_high_s8(src))),
        1);
  }
  return vmovl_s8(vld1_s8(luma));
}

#if LIBGAV1_MAX_BITDEPTH >= 10
// Computes subsampled luma for use with chroma, by averaging in the x direction
// or y direction when applicable.
template <int subsampling_x, int subsampling_y>
int16x8_t GetSubsampledLuma(const int16_t* const luma, ptrdiff_t stride) {
  if (subsampling_y != 0) {
    assert(subsampling_x != 0);
    int16x8_t src0_lo = vld1q_s16(luma);
    int16x8_t src0_hi = vld1q_s16(luma + 8);
    const int16x8_t src1_lo = vld1q_s16(luma + stride);
    const int16x8_t src1_hi = vld1q_s16(luma + stride + 8);
    const int16x8_t src0 =
        vcombine_s16(vpadd_s16(vget_low_s16(src0_lo), vget_high_s16(src0_lo)),
                     vpadd_s16(vget_low_s16(src0_hi), vget_high_s16(src0_hi)));
    const int16x8_t src1 =
        vcombine_s16(vpadd_s16(vget_low_s16(src1_lo), vget_high_s16(src1_lo)),
                     vpadd_s16(vget_low_s16(src1_hi), vget_high_s16(src1_hi)));
    return vrshrq_n_s16(vaddq_s16(src0, src1), 2);
  }
  if (subsampling_x != 0) {
    const int16x8_t src_lo = vld1q_s16(luma);
    const int16x8_t src_hi = vld1q_s16(luma + 8);
    const int16x8_t ret =
        vcombine_s16(vpadd_s16(vget_low_s16(src_lo), vget_high_s16(src_lo)),
                     vpadd_s16(vget_low_s16(src_hi), vget_high_s16(src_hi)));
    return vrshrq_n_s16(ret, 1);
  }
  return vld1q_s16(luma);
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

template <typename GrainType, int bitdepth, int auto_regression_coeff_lag,
          int subsampling_x, int subsampling_y, bool use_luma>
void ApplyAutoRegressiveFilterChroma(const FilmGrainParams& params,
                                     int /*grain_min*/, int /*grain_max*/,
                                     const GrainType* luma_grain,
                                     int /*chroma_width*/,
                                     int /*chroma_height*/, GrainType* u_grain,
                                     GrainType* v_grain) {
  static_assert(auto_regression_coeff_lag <= 3, "Invalid autoregression lag.");
  const int auto_regression_shift = params.auto_regression_shift;
  const int chroma_width =
      (subsampling_x == 0) ? kMaxChromaWidth : kMinChromaWidth;
  const int chroma_height =
      (subsampling_y == 0) ? kMaxChromaHeight : kMinChromaHeight;
  // When |chroma_width| == 44, we write 8 at a time from x in [3, 34],
  // leaving [35, 40] to write at the end.
  const int chroma_width_remainder =
      (chroma_width - 2 * kAutoRegressionBorder) & 7;

  int y = kAutoRegressionBorder;
  luma_grain += kLumaWidth * y;
  u_grain += chroma_width * y;
  v_grain += chroma_width * y;
  do {
    // Each row is computed 8 values at a time in the following loop. At the
    // end of the loop, 4 values remain to write. They are given a special
    // reduced iteration at the end.
    int x = kAutoRegressionBorder;
    int luma_x = kAutoRegressionBorder;
    do {
      int pos = 0;
      int32x4x2_t sum_u;
      int32x4x2_t sum_v;
      SetZero(&sum_u);
      SetZero(&sum_v);

      if (auto_regression_coeff_lag > 0) {
        for (int delta_row = -auto_regression_coeff_lag; delta_row < 0;
             ++delta_row) {
          // These loads may overflow to the next row, but they are never called
          // on the final row of a grain block. Therefore, they will never
          // exceed the block boundaries.
          const int16x8_t u_grain_lo =
              GetSource8(u_grain + x + delta_row * chroma_width -
                         auto_regression_coeff_lag);
          const int16x8_t u_grain_hi =
              GetSource8(u_grain + x + delta_row * chroma_width -
                         auto_regression_coeff_lag + 8);
          const int16x8_t v_grain_lo =
              GetSource8(v_grain + x + delta_row * chroma_width -
                         auto_regression_coeff_lag);
          const int16x8_t v_grain_hi =
              GetSource8(v_grain + x + delta_row * chroma_width -
                         auto_regression_coeff_lag + 8);
#define ACCUMULATE_WEIGHTED_GRAIN(offset)                                  \
  sum_u = AccumulateWeightedGrain<offset>(                                 \
      u_grain_lo, u_grain_hi, params.auto_regression_coeff_u[pos], sum_u); \
  sum_v = AccumulateWeightedGrain<offset>(                                 \
      v_grain_lo, v_grain_hi, params.auto_regression_coeff_v[pos++], sum_v)

          ACCUMULATE_WEIGHTED_GRAIN(0);
          ACCUMULATE_WEIGHTED_GRAIN(1);
          ACCUMULATE_WEIGHTED_GRAIN(2);
          // The horizontal |auto_regression_coeff_lag| loop is replaced with
          // if-statements to give vextq_s16 an immediate param.
          if (auto_regression_coeff_lag > 1) {
            ACCUMULATE_WEIGHTED_GRAIN(3);
            ACCUMULATE_WEIGHTED_GRAIN(4);
          }
          if (auto_regression_coeff_lag > 2) {
            assert(auto_regression_coeff_lag == 3);
            ACCUMULATE_WEIGHTED_GRAIN(5);
            ACCUMULATE_WEIGHTED_GRAIN(6);
          }
        }
      }

      if (use_luma) {
        const int16x8_t luma = GetSubsampledLuma<subsampling_x, subsampling_y>(
            luma_grain + luma_x, kLumaWidth);

        // Luma samples get the final coefficient in the formula, but are best
        // computed all at once before the final row.
        const int coeff_u =
            params.auto_regression_coeff_u[pos + auto_regression_coeff_lag];
        const int coeff_v =
            params.auto_regression_coeff_v[pos + auto_regression_coeff_lag];

        sum_u.val[0] = vmlal_n_s16(sum_u.val[0], vget_low_s16(luma), coeff_u);
        sum_u.val[1] = vmlal_n_s16(sum_u.val[1], vget_high_s16(luma), coeff_u);
        sum_v.val[0] = vmlal_n_s16(sum_v.val[0], vget_low_s16(luma), coeff_v);
        sum_v.val[1] = vmlal_n_s16(sum_v.val[1], vget_high_s16(luma), coeff_v);
      }
      // At this point in the filter, the source addresses and destination
      // addresses overlap. Because this is an auto-regressive filter, the
      // higher lanes cannot be computed without the results of the lower lanes.
      // Each call to WriteFinalAutoRegression incorporates preceding values
      // on the final row, and writes a single sample. This allows the next
      // pixel's value to be computed in the next call.
#define WRITE_AUTO_REGRESSION_RESULT(lane)                                    \
  WriteFinalAutoRegressionChroma<bitdepth, auto_regression_coeff_lag, lane>(  \
      u_grain + x, v_grain + x, sum_u, sum_v, params.auto_regression_coeff_u, \
      params.auto_regression_coeff_v, pos, auto_regression_shift)

      WRITE_AUTO_REGRESSION_RESULT(0);
      WRITE_AUTO_REGRESSION_RESULT(1);
      WRITE_AUTO_REGRESSION_RESULT(2);
      WRITE_AUTO_REGRESSION_RESULT(3);
      WRITE_AUTO_REGRESSION_RESULT(4);
      WRITE_AUTO_REGRESSION_RESULT(5);
      WRITE_AUTO_REGRESSION_RESULT(6);
      WRITE_AUTO_REGRESSION_RESULT(7);

      x += 8;
      luma_x += 8 << subsampling_x;
    } while (x < chroma_width - kAutoRegressionBorder - chroma_width_remainder);

    // This is the "final iteration" of the above loop over width. We fill in
    // the remainder of the width, which is less than 8.
    int pos = 0;
    int32x4x2_t sum_u;
    int32x4x2_t sum_v;
    SetZero(&sum_u);
    SetZero(&sum_v);

    for (int delta_row = -auto_regression_coeff_lag; delta_row < 0;
         ++delta_row) {
      // These loads may overflow to the next row, but they are never called on
      // the final row of a grain block. Therefore, they will never exceed the
      // block boundaries.
      const int16x8_t u_grain_lo = GetSource8(
          u_grain + x + delta_row * chroma_width - auto_regression_coeff_lag);
      const int16x8_t u_grain_hi =
          GetSource8(u_grain + x + delta_row * chroma_width -
                     auto_regression_coeff_lag + 8);
      const int16x8_t v_grain_lo = GetSource8(
          v_grain + x + delta_row * chroma_width - auto_regression_coeff_lag);
      const int16x8_t v_grain_hi =
          GetSource8(v_grain + x + delta_row * chroma_width -
                     auto_regression_coeff_lag + 8);

      ACCUMULATE_WEIGHTED_GRAIN(0);
      ACCUMULATE_WEIGHTED_GRAIN(1);
      ACCUMULATE_WEIGHTED_GRAIN(2);
      // The horizontal |auto_regression_coeff_lag| loop is replaced with
      // if-statements to give vextq_s16 an immediate param.
      if (auto_regression_coeff_lag > 1) {
        ACCUMULATE_WEIGHTED_GRAIN(3);
        ACCUMULATE_WEIGHTED_GRAIN(4);
      }
      if (auto_regression_coeff_lag > 2) {
        assert(auto_regression_coeff_lag == 3);
        ACCUMULATE_WEIGHTED_GRAIN(5);
        ACCUMULATE_WEIGHTED_GRAIN(6);
      }
    }

    if (use_luma) {
      const int16x8_t luma = GetSubsampledLuma<subsampling_x, subsampling_y>(
          luma_grain + luma_x, kLumaWidth);

      // Luma samples get the final coefficient in the formula, but are best
      // computed all at once before the final row.
      const int coeff_u =
          params.auto_regression_coeff_u[pos + auto_regression_coeff_lag];
      const int coeff_v =
          params.auto_regression_coeff_v[pos + auto_regression_coeff_lag];

      sum_u.val[0] = vmlal_n_s16(sum_u.val[0], vget_low_s16(luma), coeff_u);
      sum_u.val[1] = vmlal_n_s16(sum_u.val[1], vget_high_s16(luma), coeff_u);
      sum_v.val[0] = vmlal_n_s16(sum_v.val[0], vget_low_s16(luma), coeff_v);
      sum_v.val[1] = vmlal_n_s16(sum_v.val[1], vget_high_s16(luma), coeff_v);
    }

    WRITE_AUTO_REGRESSION_RESULT(0);
    WRITE_AUTO_REGRESSION_RESULT(1);
    WRITE_AUTO_REGRESSION_RESULT(2);
    WRITE_AUTO_REGRESSION_RESULT(3);
    if (chroma_width_remainder == 6) {
      WRITE_AUTO_REGRESSION_RESULT(4);
      WRITE_AUTO_REGRESSION_RESULT(5);
    }

    luma_grain += kLumaWidth << subsampling_y;
    u_grain += chroma_width;
    v_grain += chroma_width;
  } while (++y < chroma_height);
#undef ACCUMULATE_WEIGHTED_GRAIN
#undef WRITE_AUTO_REGRESSION_RESULT
}

}  // namespace

void FilmGrainInit_NEON() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
}

// Applies an auto-regressive filter to the white noise in luma_grain.
template <int bitdepth>
template <int auto_regression_coeff_lag>
void FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON(
    const FilmGrainParams& params, int /*grain_min*/, int /*grain_max*/,
    GrainType* luma_grain) {
  static_assert(auto_regression_coeff_lag > 0, "");
  const int8_t* const auto_regression_coeff_y = params.auto_regression_coeff_y;
  const uint8_t auto_regression_shift = params.auto_regression_shift;

  int y = kAutoRegressionBorder;
  luma_grain += kLumaWidth * y;
  do {
    // Each row is computed 8 values at a time in the following loop. At the
    // end of the loop, 4 values remain to write. They are given a special
    // reduced iteration at the end.
    int x = kAutoRegressionBorder;
    do {
      int pos = 0;
      int32x4x2_t sum;
      SetZero(&sum);
      for (int delta_row = -auto_regression_coeff_lag; delta_row < 0;
           ++delta_row) {
        // These loads may overflow to the next row, but they are never called
        // on the final row of a grain block. Therefore, they will never exceed
        // the block boundaries.
        const int16x8_t src_grain_lo =
            GetSource8(luma_grain + x + delta_row * kLumaWidth -
                       auto_regression_coeff_lag);
        const int16x8_t src_grain_hi =
            GetSource8(luma_grain + x + delta_row * kLumaWidth -
                       auto_regression_coeff_lag + 8);

        // A pictorial representation of the auto-regressive filter for
        // various values of params.auto_regression_coeff_lag. The letter 'O'
        // represents the current sample. (The filter always operates on the
        // current sample with filter coefficient 1.) The letters 'X'
        // represent the neighboring samples that the filter operates on, below
        // their corresponding "offset" number.
        //
        // params.auto_regression_coeff_lag == 3:
        //   0 1 2 3 4 5 6
        //   X X X X X X X
        //   X X X X X X X
        //   X X X X X X X
        //   X X X O
        // params.auto_regression_coeff_lag == 2:
        //     0 1 2 3 4
        //     X X X X X
        //     X X X X X
        //     X X O
        // params.auto_regression_coeff_lag == 1:
        //       0 1 2
        //       X X X
        //       X O
        // params.auto_regression_coeff_lag == 0:
        //         O
        // The function relies on the caller to skip the call in the 0 lag
        // case.

#define ACCUMULATE_WEIGHTED_GRAIN(offset)                           \
  sum = AccumulateWeightedGrain<offset>(src_grain_lo, src_grain_hi, \
                                        auto_regression_coeff_y[pos++], sum)
        ACCUMULATE_WEIGHTED_GRAIN(0);
        ACCUMULATE_WEIGHTED_GRAIN(1);
        ACCUMULATE_WEIGHTED_GRAIN(2);
        // The horizontal |auto_regression_coeff_lag| loop is replaced with
        // if-statements to give vextq_s16 an immediate param.
        if (auto_regression_coeff_lag > 1) {
          ACCUMULATE_WEIGHTED_GRAIN(3);
          ACCUMULATE_WEIGHTED_GRAIN(4);
        }
        if (auto_regression_coeff_lag > 2) {
          assert(auto_regression_coeff_lag == 3);
          ACCUMULATE_WEIGHTED_GRAIN(5);
          ACCUMULATE_WEIGHTED_GRAIN(6);
        }
      }
      // At this point in the filter, the source addresses and destination
      // addresses overlap. Because this is an auto-regressive filter, the
      // higher lanes cannot be computed without the results of the lower lanes.
      // Each call to WriteFinalAutoRegression incorporates preceding values
      // on the final row, and writes a single sample. This allows the next
      // pixel's value to be computed in the next call.
#define WRITE_AUTO_REGRESSION_RESULT(lane)                             \
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>( \
      luma_grain + x, sum, auto_regression_coeff_y, pos,               \
      auto_regression_shift)

      WRITE_AUTO_REGRESSION_RESULT(0);
      WRITE_AUTO_REGRESSION_RESULT(1);
      WRITE_AUTO_REGRESSION_RESULT(2);
      WRITE_AUTO_REGRESSION_RESULT(3);
      WRITE_AUTO_REGRESSION_RESULT(4);
      WRITE_AUTO_REGRESSION_RESULT(5);
      WRITE_AUTO_REGRESSION_RESULT(6);
      WRITE_AUTO_REGRESSION_RESULT(7);
      x += 8;
      // Leave the final four pixels for the special iteration below.
    } while (x < kLumaWidth - kAutoRegressionBorder - 4);

    // Final 4 pixels in the row.
    int pos = 0;
    int32x4x2_t sum;
    SetZero(&sum);
    for (int delta_row = -auto_regression_coeff_lag; delta_row < 0;
         ++delta_row) {
      const int16x8_t src_grain_lo = GetSource8(
          luma_grain + x + delta_row * kLumaWidth - auto_regression_coeff_lag);
      const int16x8_t src_grain_hi =
          GetSource8(luma_grain + x + delta_row * kLumaWidth -
                     auto_regression_coeff_lag + 8);

      ACCUMULATE_WEIGHTED_GRAIN(0);
      ACCUMULATE_WEIGHTED_GRAIN(1);
      ACCUMULATE_WEIGHTED_GRAIN(2);
      // The horizontal |auto_regression_coeff_lag| loop is replaced with
      // if-statements to give vextq_s16 an immediate param.
      if (auto_regression_coeff_lag > 1) {
        ACCUMULATE_WEIGHTED_GRAIN(3);
        ACCUMULATE_WEIGHTED_GRAIN(4);
      }
      if (auto_regression_coeff_lag > 2) {
        assert(auto_regression_coeff_lag == 3);
        ACCUMULATE_WEIGHTED_GRAIN(5);
        ACCUMULATE_WEIGHTED_GRAIN(6);
      }
    }
    // delta_row == 0
    WRITE_AUTO_REGRESSION_RESULT(0);
    WRITE_AUTO_REGRESSION_RESULT(1);
    WRITE_AUTO_REGRESSION_RESULT(2);
    WRITE_AUTO_REGRESSION_RESULT(3);
    luma_grain += kLumaWidth;
  } while (++y < kLumaHeight);

#undef WRITE_AUTO_REGRESSION_RESULT
#undef ACCUMULATE_WEIGHTED_GRAIN
}

template <int bitdepth>
template <int auto_regression_coeff_lag>
void FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains_NEON(
    const FilmGrainParams& params, int grain_min, int grain_max,
    const GrainType* luma_grain, int subsampling_x, int subsampling_y,
    int chroma_width, int chroma_height, GrainType* u_grain,
    GrainType* v_grain) {
  const bool use_luma = params.num_y_points > 0;
  if (subsampling_x == 0) {
    assert(subsampling_x == 0 && subsampling_y == 0);
    if (use_luma) {
      ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                      auto_regression_coeff_lag, 0, 0, true>(
          params, grain_min, grain_max, luma_grain, chroma_width, chroma_height,
          u_grain, v_grain);
    } else {
      ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                      auto_regression_coeff_lag, 0, 0, false>(
          params, grain_min, grain_max, luma_grain, chroma_width, chroma_height,
          u_grain, v_grain);
    }
  } else {
    if (subsampling_y == 0) {
      assert(subsampling_x == 1 && subsampling_y == 0);
      if (use_luma) {
        ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                        auto_regression_coeff_lag, 1, 0, true>(
            params, grain_min, grain_max, luma_grain, chroma_width,
            chroma_height, u_grain, v_grain);
      } else {
        ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                        auto_regression_coeff_lag, 1, 0, false>(
            params, grain_min, grain_max, luma_grain, chroma_width,
            chroma_height, u_grain, v_grain);
      }
    } else {
      assert(subsampling_x == 1 && subsampling_y == 1);
      if (use_luma) {
        ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                        auto_regression_coeff_lag, 1, 1, true>(
            params, grain_min, grain_max, luma_grain, chroma_width,
            chroma_height, u_grain, v_grain);
      } else {
        ApplyAutoRegressiveFilterChroma<GrainType, bitdepth,
                                        auto_regression_coeff_lag, 1, 1, false>(
            params, grain_min, grain_max, luma_grain, chroma_width,
            chroma_height, u_grain, v_grain);
      }
    }
  }
}

template <int bitdepth>
bool FilmGrain<bitdepth>::Init_NEON() {
  // Section 7.18.3.3. Generate grain process.

  using LumaAutoRegressionFunc =
      void (*)(const FilmGrainParams& params, int grain_min, int grain_max,
               GrainType* luma_grain);
  static const LumaAutoRegressionFunc kAutoRegressionFuncTableLuma[3] = {
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<1>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<2>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<3>};
  // If params_.num_y_points is 0, luma_grain_ will never be read, so we don't
  // need to generate it.
  if (params_.num_y_points > 0) {
    GenerateLumaGrain(params_, luma_grain_);
    // If params_.auto_regression_coeff_lag is 0, the filter is the identity
    // filter and therefore can be skipped.
    if (params_.auto_regression_coeff_lag > 0) {
      kAutoRegressionFuncTableLuma[params_.auto_regression_coeff_lag - 1](
          params_, GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>(),
          luma_grain_);
    }
  } else {
    ASAN_POISON_MEMORY_REGION(luma_grain_, sizeof(luma_grain_));
  }
  using ChromaAutoRegressionFunc =
      void (*)(const FilmGrainParams& params, int grain_min, int grain_max,
               const GrainType* luma_grain, int subsampling_x,
               int subsampling_y, int chroma_width, int chroma_height,
               GrainType* u_grain, GrainType* v_grain);
  static const ChromaAutoRegressionFunc kAutoRegressionFuncTableChroma[4] = {
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains_NEON<0>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains_NEON<1>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains_NEON<2>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains_NEON<3>};
  if (!is_monochrome_) {
    GenerateChromaGrains(params_, chroma_width_, chroma_height_, u_grain_,
                         v_grain_);
    if (params_.auto_regression_coeff_lag > 0 || params_.num_y_points > 0) {
      kAutoRegressionFuncTableChroma[params_.auto_regression_coeff_lag](
          params_, GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>(),
          luma_grain_, subsampling_x_, subsampling_y_, chroma_width_,
          chroma_height_, u_grain_, v_grain_);
    }
  }

  // Section 7.18.3.4. Scaling lookup initialization process.

  // Initialize scaling_lut_y_. If params_.num_y_points > 0, scaling_lut_y_
  // is used for the Y plane. If params_.chroma_scaling_from_luma is true,
  // scaling_lut_u_ and scaling_lut_v_ are the same as scaling_lut_y_ and are
  // set up as aliases. So we need to initialize scaling_lut_y_ under these
  // two conditions.
  //
  // Note: Although it does not seem to make sense, there are test vectors
  // with chroma_scaling_from_luma=true and params_.num_y_points=0.
  if (params_.num_y_points > 0 || params_.chroma_scaling_from_luma) {
    InitializeScalingLookupTable(params_.num_y_points, params_.point_y_value,
                                 params_.point_y_scaling, scaling_lut_y_);
  } else {
    ASAN_POISON_MEMORY_REGION(scaling_lut_y_, sizeof(scaling_lut_y_));
  }
  if (!is_monochrome_) {
    if (params_.chroma_scaling_from_luma) {
      scaling_lut_u_ = scaling_lut_y_;
      scaling_lut_v_ = scaling_lut_y_;
    } else {
      scaling_lut_chroma_buffer_.reset(new (std::nothrow) uint8_t[256 * 2]);
      if (scaling_lut_chroma_buffer_ == nullptr) return false;
      scaling_lut_u_ = &scaling_lut_chroma_buffer_[0];
      scaling_lut_v_ = &scaling_lut_chroma_buffer_[256];
      InitializeScalingLookupTable(params_.num_u_points, params_.point_u_value,
                                   params_.point_u_scaling, scaling_lut_u_);
      InitializeScalingLookupTable(params_.num_v_points, params_.point_v_value,
                                   params_.point_v_scaling, scaling_lut_v_);
    }
  }
  return true;
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AddNoise_NEON(
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v, void* dest_plane_y,
    ptrdiff_t dest_stride_y, void* dest_plane_u, ptrdiff_t dest_stride_u,
    void* dest_plane_v, ptrdiff_t dest_stride_v) {
  if (!Init_NEON()) {
    LIBGAV1_DLOG(ERROR, "Init() failed.");
    return false;
  }
  if (!AllocateNoiseStripes()) {
    LIBGAV1_DLOG(ERROR, "AllocateNoiseStripes() failed.");
    return false;
  }
  ConstructNoiseStripes();

  if (!AllocateNoiseImage()) {
    LIBGAV1_DLOG(ERROR, "AllocateNoiseImage() failed.");
    return false;
  }
  ConstructNoiseImage();

  BlendNoiseWithImage(source_plane_y, source_stride_y, source_plane_u,
                      source_stride_u, source_plane_v, source_stride_v,
                      dest_plane_y, dest_stride_y, dest_plane_u, dest_stride_u,
                      dest_plane_v, dest_stride_v);

  return true;
}

// Explicit instantiations.
template class FilmGrain<8>;
#if LIBGAV1_MAX_BITDEPTH >= 10
template class FilmGrain<10>;
#endif

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void FilmGrainInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
