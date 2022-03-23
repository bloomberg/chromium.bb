/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_JIT_PYTHON_BINDING_CONVERSION_UTILS_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_JIT_PYTHON_BINDING_CONVERSION_UTILS_H_

#include "pybind11/numpy.h"
#include "tfrt/jitrt/types.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime

namespace tensorflow {

// Returns Python buffer protocol's type string from TFRT's dtype.
const char* ToPythonStructFormat(tfrt::DType dtype_kind);

// Returns TFRT's dtype for the Python buffer protocol's type string.
tfrt::DType FromPythonStructFormat(char dtype);

// Converts Python array to the Memref Descriptor.
void ConvertPyArrayMemrefDesc(const pybind11::array& array,
                              tfrt::jitrt::MemrefDesc* memref);

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_JIT_PYTHON_BINDING_CONVERSION_UTILS_H_
