/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_CPU_CHECK_H_
#define TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_CPU_CHECK_H_

// This include is superfluous. However, it's been here for a while, and a
// number of files have been relying on it to include neon_check.h for them.
// This should be removed, but with a global run of presubmits to catch
// any such issues. This requires running more than just TFLite presubmits.
#include "tensorflow/lite/kernels/internal/optimized/neon_check.h"

namespace tflite {

// On A64, returns true if the dotprod extension is present.
// On other architectures, returns false unconditionally.
bool DetectArmNeonDotprod();

struct CpuFlags {
  bool neon_dotprod = false;
};

inline void GetCpuFlags(CpuFlags* cpu_flags) {
  cpu_flags->neon_dotprod = DetectArmNeonDotprod();
}

}  // namespace tflite

#endif  // TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_CPU_CHECK_H_
