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

#include "tensorflow/c/experimental/saved_model/core/concrete_function.h"

#include "tensorflow/c/eager/immediate_execution_tensor_handle.h"
#include "tensorflow/c/experimental/saved_model/core/function_metadata.h"

namespace tensorflow {

const std::vector<tensorflow::ImmediateExecutionTensorHandle*>&
ConcreteFunction::GetCaptures() const {
  return captures_;
}

const FunctionMetadata& ConcreteFunction::GetFunctionMetadata() const {
  return metadata_;
}

}  // namespace tensorflow
