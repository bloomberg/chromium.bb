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

#include "tensorflow/compiler/mlir/tfrt/benchmarks/cwise_op_unary_benchmark.h"

namespace tensorflow {

using f32 = float;

static const char* mlir_input = R"(
func @log1p_1d(%arg0: tensor<?xf32>) -> tensor<?xf32> {
  %0 = "tf.Log1p"(%arg0): (tensor<?xf32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}
)";

#define EXPR_BUILDER [](auto& in) { return in.log1p(); }

BM_TFMlir(Log1p, mlir_input, "log1p_1d", 1, f32, 1.0, 0.0, /* num_threads */ 0)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1024)
    ->Arg(10 * 1024);

BM_EigenScalar(Log1p, EXPR_BUILDER, 1, f32, 1.0, 0.0, /* num_threads */ 0)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1024)
    ->Arg(10 * 1024);

BM_EigenVectorized(Log1p, EXPR_BUILDER, 1, f32, 1.0, 0.0, /* num_threads */ 0)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1024)
    ->Arg(10 * 1024);

}  // namespace tensorflow
