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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_CPU_CPU_XFEED_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_CPU_CPU_XFEED_H_

#include <vector>

#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/service/hlo_cost_analysis.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/status.h"

// This provides a lower level API than TransferManager that does not depend on
// StreamExecutor. It is intended to be used by callers that do not want to use
// Stream or StreamExecutor.

namespace xla {

// Helper function to transfers to infeed on CPU.
Status TransferLiteralToInfeedOnCpu(int device_ordinal,
                                    const LiteralSlice& literal);

// Helper function to transfers from outfeed on CPU.
Status TransferLiteralFromOutfeedOnCpu(int device_ordinal,
                                       MutableBorrowingLiteral literal);

// Helper function to retrieve dynamic shape on CPU.
Status ReadDynamicShapesOnCpu(ShapedBuffer* device_buffer, Shape* device_shape,
                              HloCostAnalysis::ShapeSizeFunction shape_size_fn);
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_CPU_CPU_XFEED_H_
