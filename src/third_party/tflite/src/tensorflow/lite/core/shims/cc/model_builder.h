/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_LITE_CORE_SHIMS_CC_MODEL_BUILDER_H_
#define TENSORFLOW_LITE_CORE_SHIMS_CC_MODEL_BUILDER_H_

/// For documentation, see third_party/tensorflow/lite/model_builder.h.

#include "tensorflow/lite/model_builder.h"

namespace tflite_shims {
using FlatBufferModel = ::tflite::FlatBufferModel;
}  // namespace tflite_shims

#endif  // TENSORFLOW_LITE_CORE_SHIMS_CC_MODEL_BUILDER_H_
