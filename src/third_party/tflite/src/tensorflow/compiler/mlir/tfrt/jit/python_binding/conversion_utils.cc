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

#include "tensorflow/compiler/mlir/tfrt/jit/python_binding/conversion_utils.h"

#include <stdexcept>

#include "pybind11/numpy.h"
#include "tfrt/jitrt/types.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime

namespace tensorflow {

using ::tfrt::DType;

using ::tfrt::jitrt::MemrefDesc;

// Returns Python buffer protocol's type string from TFRT's dtype.
const char* ToPythonStructFormat(DType dtype_kind) {
  // Reference: https://docs.python.org/3/library/struct.html

  switch (dtype_kind) {
    case DType::Invalid:
      throw std::runtime_error("Invalid dtype.");
    case DType::Unsupported:
      throw std::runtime_error("Unsupported dtype.");
    case DType::UI8:
      return "B";
    case DType::UI16:
      return "H";
    case DType::UI32:
      return "I";
    case DType::UI64:
      return "Q";
    case DType::I1:
      return "?";
    case DType::I8:
      return "b";
    case DType::I16:
      return "h";
    case DType::I32:
      return "i";
    case DType::I64:
      return "q";
    case DType::F32:
      return "f";
    case DType::F64:
      return "d";
    case DType::Complex64:
      throw std::runtime_error("Unimplemented.");
    case DType::Complex128:
      throw std::runtime_error("Unimplemented.");
    case DType::F16:
      throw std::runtime_error("Unimplemented.");
    case DType::BF16:
      throw std::runtime_error("Unimplemented.");
    case DType::String:
      throw std::runtime_error("Unimplemented.");
    default:
      throw std::runtime_error("Unimplemented.");
  }
}

// Returns TFRT's dtype for the Python buffer protocol's type string.
DType FromPythonStructFormat(char dtype) {
  // Reference: https://docs.python.org/3/library/struct.html
  switch (dtype) {
    case 'B':
      return DType::UI8;
    case 'H':
      return DType::UI16;
    case 'I':
      return DType::UI32;
    case 'Q':
      return DType::UI64;
    case '?':
      return DType::I1;
    case 'b':
      return DType::I8;
    case 'h':
      return DType::I16;
    case 'i':
      return DType::I32;
    case 'q':
      return DType::I64;
    case 'f':
      return DType::F32;
    case 'd':
      return DType::F64;
    default:
      throw std::runtime_error("Unsupported python dtype.");
  }
}

// Converts Python array to the Memref Descriptor.
void ConvertPyArrayMemrefDesc(const pybind11::array& array,
                              MemrefDesc* memref) {
  auto py_dtype = [](pybind11::dtype dtype) -> char {
    // np.int64 array for some reason has `i` dtype, however according to the
    // documentation it must be `q`.
    if (dtype.kind() == 'i' && dtype.itemsize() == 8) return 'q';

    return dtype.char_();
  };

  memref->dtype = DType(FromPythonStructFormat(py_dtype(array.dtype())));
  memref->data = const_cast<void*>(array.data());
  memref->offset = 0;

  auto rank = array.ndim();
  memref->sizes.resize(rank);
  memref->strides.resize(rank);

  for (ssize_t d = 0; d < rank; ++d) {
    memref->sizes[d] = array.shape(d);
    memref->strides[d] = array.strides(d) / array.itemsize();
  }
}

}  // namespace tensorflow
