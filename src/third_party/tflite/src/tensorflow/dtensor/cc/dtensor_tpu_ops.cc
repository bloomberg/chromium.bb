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

#include <string>
#include <utility>
#include <vector>

#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"

namespace tensorflow {
namespace dtensor {

using shape_inference::InferenceContext;
using shape_inference::ShapeHandle;
using shape_inference::UnchangedShape;

// Initializes global TPU's for mutli-client execution.
//
// This op does the work of both ConfigureDistributedTpuOp and
// InitializeHostForDistributedTpuOp, and outputs the latter's result.
REGISTER_OP("ConfigureAndInitializeGlobalTPU")
    .Output("output: int32")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) {
      ShapeHandle input;
      // Validate that all the inputs are scalars.
      for (int i = 0; i < c->num_inputs(); ++i) {
        TF_RETURN_IF_ERROR(c->WithRank(c->input(i), 0, &input));
      }
      c->set_output(0, c->Vector(c->UnknownDim()));
      return Status::OK();
    })
    .Doc(R"doc(
An op that sets up the centralized structures for a distributed TPU
system.

output: A vector containing the global TPU id of each TPU on the host.
)doc");

REGISTER_OP("ShutdownTPUSystem")
    .SetIsStateful()
    .Output("success: bool")
    .Doc(R"doc(
An op that shuts down the TPU system.

success: A boolean that indicates if the shut down process succeeds.
)doc");


}  // namespace dtensor
}  // namespace tensorflow
