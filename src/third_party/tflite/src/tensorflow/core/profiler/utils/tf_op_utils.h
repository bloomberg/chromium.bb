/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_PROFILER_UTILS_TF_OP_UTILS_H_
#define TENSORFLOW_CORE_PROFILER_UTILS_TF_OP_UTILS_H_

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace tensorflow {
namespace profiler {

// Special op types.
ABSL_CONST_INIT extern const absl::string_view kUnknownOp;
ABSL_CONST_INIT extern const absl::string_view kDatasetOp;
ABSL_CONST_INIT extern const absl::string_view kMemcpyHToDOp;
ABSL_CONST_INIT extern const absl::string_view kMemcpyDToHOp;

enum class Category {
  kTensorFlow,
  kJax,
  kTfData,
  kMemcpyHToD,
  kMemcpyDToH,
  kUnknown,
};

// Breaks a TensorFlow op fullname into name and type.
struct TfOp {
  Category category;
  absl::string_view name;
  absl::string_view type;
};

TfOp ParseTfOpFullname(absl::string_view tf_op_fullname);

// Returns a vector of TF name scopes extracted from tf_op_full_name.
std::vector<absl::string_view> ParseTfNameScopes(const TfOp& tf_op);

// Trace event name for TF ops is the op type so they have the same color in
// trace viewer.
std::string TfOpEventName(const TfOp& tf_op);
std::string TfOpEventName(absl::string_view tf_op_fullname);

// Returns true if the given name is a TensorFlow Dataset Op.
inline bool IsDatasetOp(absl::string_view tf_op_type) {
  return tf_op_type == kDatasetOp;
}

// Returns true if the given name is a TensorFlow Infeed Enqueue Op.
inline bool IsInfeedEnqueueOp(absl::string_view tf_op_type) {
  return tf_op_type == "InfeedEnqueue" || tf_op_type == "InfeedEnqueueTuple";
}

// Returns true if the given name is a TensorFlow embedding op.
inline bool IsEmbeddingOp(absl::string_view tf_op_fullname) {
  return absl::StrContains(tf_op_fullname, "Embedding");
}

// Returns true if the given op is for copying data from host to device.
inline bool IsMemcpyHToDOp(absl::string_view tf_op_type) {
  return tf_op_type == kMemcpyHToDOp;
}

// Returns true if the given op is for copying data from device to host.
inline bool IsMemcpyDToHOp(absl::string_view tf_op_type) {
  return tf_op_type == kMemcpyDToHOp;
}

// Splits a string of tensor shapes in "(shape1;shape2;...)" format, i.e.,
// delimited by '(' and ')' and separated by ';', into the individual shapes.
std::vector<absl::string_view> ParseTensorShapes(
    absl::string_view tensor_shapes);

}  // namespace profiler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PROFILER_UTILS_TF_OP_UTILS_H_
