/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/kernels/cwise_ops_common.h"

namespace tensorflow {

// REGISTER# macros ignore all but first type (assumed to be float) when
// __ANDROID_TYPES_SLIM__ is defined.  Since this file is the second of two
// sharded files, only make its register calls when not __ANDROID_TYPES_SLIM__.
#if !defined(__ANDROID_TYPES_SLIM__)

REGISTER6(BinaryOp, CPU, "Add", functor::add, int8, int16, complex64, uint8,
          complex128, tstring);
#if !defined(MLIR_GENERATED_CPU_KERNELS_ENABLED) || \
    !defined(MLIR_GENERATED_EXPERIMENTAL_KERNELS_ENABLED)
// Notice: String is excluded to allow marking AddV2 is_commutative and
// is_aggregate.
REGISTER8(BinaryOp, CPU, "AddV2", functor::add, int8, int16, complex64, uint8,
          uint16, uint32, uint64, complex128);
#endif

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
#if !defined(MLIR_GENERATED_GPU_KERNELS_ENABLED)
REGISTER4(BinaryOp, GPU, "Add", functor::add, uint8, int64, complex64,
          complex128);

REGISTER7(BinaryOp, GPU, "AddV2", functor::add, uint8, uint16, uint32, uint64,
          int64, complex64, complex128);
#endif

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#endif  // !defined(__ANDROID_TYPES_SLIM__)

}  // namespace tensorflow
