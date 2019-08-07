// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated using tf.native from a neural network trained by
// TensorFlow, then cleaned up by hand. Please do not edit except to update
// the constants for a new model.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_TF_NATIVE_INFERENCE_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_TF_NATIVE_INFERENCE_H_

#include <cstdint>

namespace chromeos {
namespace power {
namespace ml {
namespace tfnative_model {

constexpr int DNN_WEIGHTS_SIZE = 33900;
constexpr int DNN_RANK = 2;
constexpr int FEATURES_SIZE = 565;
constexpr int DNN_BIASES_SIZE = 60;

struct alignas(16) FixedAllocations {
  float alloc0[DNN_WEIGHTS_SIZE];
  int32_t alloc0_shape[DNN_RANK];
  float alloc1[DNN_BIASES_SIZE];
  int32_t alloc1_shape[DNN_RANK];
};

void Inference(
    /* size: FEATURES_SIZE */
    const float* __restrict features,
    /* size: 1 */
    float* __restrict prediction,
    FixedAllocations* __restrict fixed);

}  // namespace tfnative_model
}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_TF_NATIVE_INFERENCE_H_
