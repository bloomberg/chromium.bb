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

#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_cpurt_passes.h"

namespace tensorflow {

bool IsContiguousMemref(mlir::Value value) {
  auto memref_type = value.getType().dyn_cast<mlir::MemRefType>();
  if (!memref_type) return false;
  mlir::MemRefType canonical_type = canonicalizeStridedLayout(memref_type);
  return canonical_type.getLayout().isIdentity();
}

}  // namespace tensorflow
