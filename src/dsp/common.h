/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_DSP_COMMON_H_
#define LIBGAV1_SRC_DSP_COMMON_H_

#include <cstddef>  // ptrdiff_t
#include <cstdint>

#include "src/dsp/constants.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"

namespace libgav1 {

struct LoopRestoration {
  LoopRestorationType type[kMaxPlanes];
  int unit_size[kMaxPlanes];
};

// Self guided projection filter.
struct SgrProjInfo {
  int index;
  int multiplier[2];
};

struct WienerInfo {
  static const int kVertical = 0;
  static const int kHorizontal = 1;

  alignas(kMaxAlignment) int16_t filter[2][kSubPixelTaps];
};

struct RestorationUnitInfo : public MaxAlignedAllocable {
  LoopRestorationType type;
  SgrProjInfo sgr_proj_info;
  WienerInfo wiener_info;
};

struct RestorationBuffer {
  // For self-guided filter.
  int* box_filter_process_output[2];
  ptrdiff_t box_filter_process_output_stride;
  uint32_t* box_filter_process_intermediate[2];
  ptrdiff_t box_filter_process_intermediate_stride;
  // For wiener filter.
  uint16_t* wiener_buffer;
  ptrdiff_t wiener_buffer_stride;
};

// Section 6.8.20.
// Note: In spec, film grain section uses YCbCr to denote variable names,
// such as num_cb_points, num_cr_points. To keep it consistent with other
// parts of code, we use YUV, i.e., num_u_points, num_v_points, etc.
struct FilmGrainParams {
  bool apply_grain;
  bool update_grain;
  bool chroma_scaling_from_luma;
  bool overlap_flag;
  bool clip_to_restricted_range;

  uint8_t num_y_points;  // [0, 14].
  uint8_t num_u_points;  // [0, 10].
  uint8_t num_v_points;  // [0, 10].
  // Must be [0, 255]. 10/12 bit /= 4 or 16. Must be in increasing order.
  uint8_t point_y_value[14];
  uint8_t point_y_scaling[14];
  uint8_t point_u_value[10];
  uint8_t point_u_scaling[10];
  uint8_t point_v_value[10];
  uint8_t point_v_scaling[10];

  uint8_t chroma_scaling;              // [8, 11].
  uint8_t auto_regression_coeff_lag;   // [0, 3].
  int8_t auto_regression_coeff_y[24];  // [-128, 127]
  int8_t auto_regression_coeff_u[25];  // [-128, 127]
  int8_t auto_regression_coeff_v[25];  // [-128, 127]
  // Shift value: auto regression coeffs range
  // 6: [-2, 2)
  // 7: [-1, 1)
  // 8: [-0.5, 0.5)
  // 9: [-0.25, 0.25)
  uint8_t auto_regression_shift;

  uint16_t grain_seed;
  int reference_index;
  int grain_scale_shift;
  // These multipliers are encoded as nonnegative values by adding 128 first.
  // The 128 is subtracted during parsing.
  int8_t u_multiplier;       // [-128, 127]
  int8_t u_luma_multiplier;  // [-128, 127]
  // These offsets are encoded as nonnegative values by adding 256 first. The
  // 256 is subtracted during parsing.
  int16_t u_offset;          // [-256, 255]
  int8_t v_multiplier;       // [-128, 127]
  int8_t v_luma_multiplier;  // [-128, 127]
  int16_t v_offset;          // [-256, 255]
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_COMMON_H_
