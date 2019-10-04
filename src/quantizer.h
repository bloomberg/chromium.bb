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

#ifndef LIBGAV1_SRC_QUANTIZER_H_
#define LIBGAV1_SRC_QUANTIZER_H_

#include <cstdint>

#include "src/utils/constants.h"
#include "src/utils/segmentation.h"

namespace libgav1 {

// Stores the quantization parameters of Section 5.9.12.
struct QuantizerParameters {
  // base_index is in the range [0, 255].
  uint8_t base_index;
  int8_t delta_dc[kMaxPlanes];
  // delta_ac[kPlaneY] is always 0.
  int8_t delta_ac[kMaxPlanes];
  bool use_matrix;
  // The |matrix_level| array is used only when |use_matrix| is true.
  // matrix_level[plane] specifies the level in the quantizer matrix that
  // should be used for decoding |plane|. The quantizer matrix has 15 levels,
  // from 0 to 14. The range of matrix_level[plane] is [0, 15]. If
  // matrix_level[plane] is 15, the quantizer matrix is not used.
  int8_t matrix_level[kMaxPlanes];
};

// Implements the dequantization functions of Section 7.12.2.
class Quantizer {
 public:
  Quantizer(int bitdepth, const QuantizerParameters* params);

  // Returns the quantizer value for the dc coefficient for the given plane.
  // The caller should call GetQIndex() with Tile::current_quantizer_index_ as
  // the |base_qindex| argument, and pass the return value as the |qindex|
  // argument to this method.
  int GetDcValue(Plane plane, int qindex) const;

  // Returns the quantizer value for the ac coefficient for the given plane.
  // The caller should call GetQIndex() with Tile::current_quantizer_index_ as
  // the |base_qindex| argument, and pass the return value as the |qindex|
  // argument to this method.
  int GetAcValue(Plane plane, int qindex) const;

 private:
  const QuantizerParameters& params_;
  const int16_t* dc_lookup_;
  const int16_t* ac_lookup_;
};

// Get the quantizer index for the |index|th segment.
//
// This function has two use cases. What should be passed as the |base_qindex|
// argument depends on the use case.
// 1. While parsing the uncompressed header or transform type, pass
//    Quantizer::base_index.
//    Note: In this use case, the caller only cares about whether the return
//    value is zero.
// 2. To generate the |qindex| argument to Quantizer::GetDcQuant() or
//    Quantizer::GetAcQuant(), pass Tile::current_quantizer_index_.
int GetQIndex(const Segmentation& segmentation, int index, int base_qindex);

}  // namespace libgav1

#endif  // LIBGAV1_SRC_QUANTIZER_H_
