/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/lite/delegates/nnapi/acceleration_test_util.h"

#include "tensorflow/lite/kernels/acceleration_test_util_internal.h"

namespace tflite {

absl::optional<NnapiAccelerationTestParams> GetNnapiAccelerationTestParam(
    std::string test_id) {
  return GetAccelerationTestParam<NnapiAccelerationTestParams>(test_id);
}

// static
NnapiAccelerationTestParams NnapiAccelerationTestParams::ParseConfigurationLine(
    const std::string& conf_line) {
  if (conf_line.empty()) {
    return NnapiAccelerationTestParams();
  }

  int min_sdk_version = std::stoi(conf_line);

  return NnapiAccelerationTestParams{min_sdk_version};
}

}  // namespace tflite
