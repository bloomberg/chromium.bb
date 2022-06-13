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

#include "tensorflow/compiler/tf2xla/kernels/random_ops_util.h"

#include "tensorflow/compiler/xla/client/lib/constants.h"

namespace tensorflow {

xla::XlaOp GetU64FromS32Seeds(xla::XlaOp seed0, xla::XlaOp seed1) {
  // Here, the seeds are cast to unsigned type of the same width to have leading
  // zeros in the 64 bit representation.
  xla::XlaOp u64_seed0 =
      ConvertElementType(ConvertElementType(seed0, xla::U32), xla::U64);
  xla::XlaOp u64_seed1 =
      ConvertElementType(ConvertElementType(seed1, xla::U32), xla::U64);
  return u64_seed0 |
         (u64_seed1 << ConstantR0WithType(seed0.builder(), xla::U64, 32));
}

}  // namespace tensorflow
