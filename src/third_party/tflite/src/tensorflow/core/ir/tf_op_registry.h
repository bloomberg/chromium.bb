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

#ifndef TENSORFLOW_CORE_IR_TF_OP_REGISTRY_H_
#define TENSORFLOW_CORE_IR_TF_OP_REGISTRY_H_

#include "tensorflow/core/ir/interfaces.h"

// Forward declaration of TensorFlow types.
namespace tensorflow {
class OpRegistry;
}  // namespace tensorflow

namespace mlir {
namespace tfg {
class TensorFlowOpRegistryInterface : public TensorFlowRegistryInterfaceBase {
 public:
  // Create the interface model with a provided registry.
  TensorFlowOpRegistryInterface(Dialect *dialect,
                                tensorflow::OpRegistry *registry)
      : TensorFlowRegistryInterfaceBase(dialect), registry_(registry) {}
  // Create the interface model with the global registry.
  explicit TensorFlowOpRegistryInterface(Dialect *dialect);

  // Returns true if the operation is stateful.
  bool isStateful(Operation *op) const override;

 private:
  // The TensorFlow op registry instance.
  tensorflow::OpRegistry *registry_;
};
}  // namespace tfg
}  // namespace mlir

#endif  // TENSORFLOW_CORE_IR_TF_OP_REGISTRY_H_
