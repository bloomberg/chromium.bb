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
#include "src/utils/common.h"
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

inline int16x8_t GetSource8(const int8_t* src) {
  return vmovl_s8(vld1_s8(src));
}

#if LIBGAV1_MAX_BITDEPTH >= 10
inline int16x8_t GetSource8(const int16_t* src) { return vld1q_s16(src); }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

template <int position_offset>
inline int32x4x2_t AccumulateWeightedGrain(const int16x8_t grain_lo,
                                           const int16x8_t grain_hi,
                                           int16_t coeff, int32x4x2_t sum) {
  const int16x8_t grain = vextq_s16(grain_lo, grain_hi, position_offset);
  sum.val[0] = vmlal_n_s16(sum.val[0], vget_low_s16(grain), coeff);
  sum.val[1] = vmlal_n_s16(sum.val[1], vget_high_s16(grain), coeff);
  return sum;
}

template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegression(int8_t* luma_grain_cursor, int32x4x2_t sum,
                                     const int8_t* coeffs, int pos, int shift) {
  int32_t result = vgetq_lane_s32(sum.val[lane >> 2], lane & 3);

  for (int delta_col = -auto_regression_coeff_lag; delta_col < 0; ++delta_col) {
    result += luma_grain_cursor[lane + delta_col] * coeffs[pos];
    ++pos;
  }
  luma_grain_cursor[lane] =
      Clip3(luma_grain_cursor[lane] + RightShiftWithRounding(result, shift),
            FilmGrain<bitdepth>::kGrainMin, FilmGrain<bitdepth>::kGrainMax);
}

#if LIBGAV1_MAX_BITDEPTH >= 10
template <int bitdepth, int auto_regression_coeff_lag, int lane>
inline void WriteFinalAutoRegression(int16_t* luma_grain_cursor,
                                     int32x4x2_t sum, const int8_t* coeffs,
                                     int pos, int shift) {
  int32_t result = vgetq_lane_s32(sum.val[lane >> 2], lane & 3);

  for (int delta_col = -auto_regression_coeff_lag; delta_col < 0; ++delta_col) {
    result += luma_grain_cursor[lane + delta_col] * coeffs[pos];
    ++pos;
  }
  luma_grain_cursor[lane] =
      Clip3(luma_grain_cursor[lane] + RightShiftWithRounding(result, shift),
            FilmGrain<bitdepth>::kGrainMin, FilmGrain<bitdepth>::kGrainMax);
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

inline void SetZero(int32x4x2_t* v) {
  v->val[0] = vdupq_n_s32(0);
  v->val[1] = vdupq_n_s32(0);
}

constexpr int kAutoRegressionBorder = 3;
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

  int y = 3;
  luma_grain += kLumaWidth * y;
  do {
    // Each row is computed 8 values at a time in the following loop. At the
    // end of the loop, 4 values remain to write. They are given a special
    // reduced iteration at the end.
    int x = 3;
    do {
      int pos = 0;
      int32x4x2_t sum;
      SetZero(&sum);
      for (int delta_row = -auto_regression_coeff_lag; delta_row < 0;
           ++delta_row) {
        const int16x8_t src_grain_lo =
            GetSource8(luma_grain + x + delta_row * kLumaWidth -
                       auto_regression_coeff_lag);
        const int16x8_t src_grain_hi =
            GetSource8(luma_grain + x + delta_row * kLumaWidth -
                       auto_regression_coeff_lag + 8);

#define ACCUMULATE_WEIGHTED_GRAIN(offset)                           \
  sum = AccumulateWeightedGrain<offset>(src_grain_lo, src_grain_hi, \
                                        auto_regression_coeff_y[pos++], sum);

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
      // At this point in the filter, the source addresses and destination
      // addresses overlap. Because this is a causal filter, the higher lanes
      // cannot be computed without the results of the lower lanes.
      // Each call to WriteFinalAutoRegression incorporates preceding values
      // on the final row, and writes a single sample. This allows the next
      // pixel's value to be computed in the next call.
#define WRITE_AUTO_REGRESSION_RESULT(lane)                             \
  WriteFinalAutoRegression<bitdepth, auto_regression_coeff_lag, lane>( \
      luma_grain + x, sum, auto_regression_coeff_y, pos,               \
      auto_regression_shift);

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
bool FilmGrain<bitdepth>::Init_NEON() {
  // Section 7.18.3.3. Generate grain process.
  GenerateLumaGrain(params_, luma_grain_);
  using AutoRegressionFn =
      void (*)(const FilmGrainParams& params, int grain_min, int grain_max,
               GrainType* luma_grain);
  static const AutoRegressionFn kAutoRegressionFnTableLuma[3] = {
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<1>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<2>,
      FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain_NEON<3>};
  if (params_.auto_regression_coeff_lag > 0 && params_.num_y_points > 0) {
    kAutoRegressionFnTableLuma[params_.auto_regression_coeff_lag - 1](
        params_, kGrainMin, kGrainMax, luma_grain_);
  }
  if (!is_monochrome_) {
    GenerateChromaGrains(params_, chroma_width_, chroma_height_, u_grain_,
                         v_grain_);
    ApplyAutoRegressiveFilterToChromaGrains(
        params_, kGrainMin, kGrainMax, luma_grain_, subsampling_x_,
        subsampling_y_, chroma_width_, chroma_height_, u_grain_, v_grain_);
  }

  // Section 7.18.3.4. Scaling lookup initialization process.
  InitializeScalingLookupTable(params_.num_y_points, params_.point_y_value,
                               params_.point_y_scaling, scaling_lut_y_);
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
