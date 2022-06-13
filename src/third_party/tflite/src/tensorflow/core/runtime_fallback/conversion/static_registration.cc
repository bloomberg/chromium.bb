/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

// This file uses a static constructor to automatically register all of the
// conversion function in this directory.  This can be used to simplify clients
// that don't care about selective registration of conversion functions.

#include "tensorflow/core/runtime_fallback/conversion/conversion.h"
#include "tfrt/tensor/conversion_registry.h"  // from @tf_runtime

namespace tensorflow {
namespace tfd {

static bool runtime_fallback_conversion_fn_registration = []() {
  tfrt::AddStaticTensorConversionFn(
      RegisterRuntimeFallbackTensorToKernelFallbackConversionFn);
  return true;
}();

}  // namespace tfd
}  // namespace tensorflow
