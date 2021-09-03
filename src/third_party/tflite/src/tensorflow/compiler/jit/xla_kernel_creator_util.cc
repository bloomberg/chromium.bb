/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/compiler/jit/xla_kernel_creator_util.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "tensorflow/compiler/jit/compilability_check_util.h"
#include "tensorflow/compiler/jit/defs.h"
#include "tensorflow/compiler/jit/kernels/xla_ops.h"
#include "tensorflow/compiler/jit/mark_for_compilation_pass.h"
#include "tensorflow/compiler/tf2xla/const_analysis.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/util/ptr_util.h"

namespace tensorflow {
namespace {

// Utility which searches for values in a sorted list by scanning over it once.
// No matter how many times ScanForValue is called, the list is scanned at most
// once. However, if a call to ScanForValue skips over a value, that value is
// not revisited in future calls to ScanForValue, so callers must take
// care to order their calls.
//
// Useful for merging multiple sorted lists in O(n) time.
class SinglePassSearch {
 public:
  // Creates a SinglePassSearch object that can be used to search in `values`.
  // Does not take ownership of `values`. `values` must outlive this.
  // `values` must be sorted.
  explicit SinglePassSearch(const std::vector<int>* values)
      : current_index_(0), values_(values) {}

  // Scans forward in the vector looking for "value", updating the internal
  // position in to the vector.
  // Returns true iff the vector contains the given value at or after current
  // position.
  // Not thread-safe.
  bool ScanForValue(int value) {
    while (current_index_ < values_->size() &&
           (*values_)[current_index_] <= value) {
      if ((*values_)[current_index_] == value) {
        current_index_++;
        return true;
      }
      current_index_++;
    }
    return false;
  }

 private:
  int current_index_;
  const std::vector<int>* values_;
};
}  // namespace

Status CreateXlaKernel(FunctionLibraryRuntime* flr, const NodeDef& node_def,
                       std::unique_ptr<OpKernel>* kernel) {
  if (!CanCreateXlaKernel(node_def)) {
    return errors::Internal("Invalid node: ", node_def.ShortDebugString());
  }

  VLOG(3) << "Attempting to create XlaLaunchOp for " << node_def.DebugString();

  // Make sure that kernels have been registered on the JIT device.
  XlaOpRegistry::RegisterCompilationKernels();
  RecursiveCompilabilityChecker::UncompilableNodesMap uncompilable_nodes_map;
  if (!IsCompilable(flr, node_def, &uncompilable_nodes_map)) {
    std::vector<RecursiveCompilabilityChecker::UncompilableNodeInfo>
        uncompilable_node_info;
    for (const auto& it : uncompilable_nodes_map) {
      for (const auto& info : it.second.second) {
        uncompilable_node_info.emplace_back(info);
      }
    }
    string message = absl::StrCat(
        "Function invoked by the following node is not compilable: ",
        SummarizeNodeDef(node_def, /*max_inputs_in_summary=*/10), ".\n");
    absl::StrAppend(&message, "Uncompilable nodes:");
    for (const auto& node_info : uncompilable_node_info) {
      string node_message =
          absl::StrCat("\n", node_info.name, ": ",
                       node_info.uncompilable_reason, "\n", "\tStacktrace:\n");
      for (const auto& stack_frame : node_info.stack_trace) {
        absl::StrAppendFormat(&node_message, "\t\tNode: %s, function: %s\n",
                              stack_frame.name, stack_frame.function_name);
      }
      absl::StrAppend(&message, node_message);
    }
    VLOG(1) << message;
    return errors::InvalidArgument(message);
  }

  // Get function body, constant args, and resource args.
  const FunctionBody* fbody = nullptr;
  std::vector<int> constant_arg_indices;
  std::vector<int> resource_arg_indices;
  TF_RETURN_IF_ERROR(GetBodyAndConstantsAndResources(
      flr, node_def, &fbody, &constant_arg_indices, &resource_arg_indices));

  // Set input and output memory types.
  MemoryTypeVector input_memory_types(fbody->arg_types.size(), DEVICE_MEMORY);
  // These indices are used only for optimization purposes. They allow us
  // to loop over constant_arg_indices and resource_arg_indices only once
  // while iterating over all the function arguments checking if it is a
  // resource or a constant.
  // The reason we optimized this code is because functions can have a lot of
  // captured arguments. For example, the backward pass of ResNet50 takes in all
  // 214 variables and a similar number of activations.
  SinglePassSearch constants_search(&constant_arg_indices);
  SinglePassSearch resources_search(&resource_arg_indices);
  for (size_t i = 0; i < fbody->arg_types.size(); ++i) {
    if (resources_search.ScanForValue(i) || constants_search.ScanForValue(i)) {
      // Compile-time constants and resource handles are expected to be in
      // host memory.
      input_memory_types[i] = HOST_MEMORY;
    }
  }
  // One might wonder, about the case where a compile-time constant argument
  // (which must be in host memory) is also used as an input into an op,
  // e.g. Add, that expects its inputs in device memory. Here is how it
  // works now.
  // First, what do we mean by "op expects an input in XYZ memory"?
  // There are two types of "ops" here: the tf2xla kernel and the HLO
  // computation it builds. The tf2xla kernel needs to retrieve the actual
  // numeric value of the compile-time constant tensors, so it really expects
  // them to be on in host memory. However, for other inputs, it refers to them
  // using xla::ComputationDataHandle, which is just a symbolic handle that
  // xla::ComputationBuilder assigns. How does this handle gets assigned for
  // constant arguments? Even constant arguments get an _Arg node in the graph
  // instantiated for Function compilation. The tf2xla kernel for constant _Arg
  // nodes takes the constant value, converts it to XlaLiteral, and feeds it
  // to xla::ComputationBuilder.ConstantLiteral, which returns the handle. This
  // constant XlaLiteral is included in the HLO graph, and subsequently, in
  // the actual executable, which is copied to the device before being
  // executed. Thus, when this executable runs, the constant is available in
  // device memory.

  // XlaLaunch kernel keeps all outputs (including constants, which it copies),
  // in device memory except for resources.
  MemoryTypeVector output_memory_types(fbody->ret_types.size(), DEVICE_MEMORY);
  for (size_t i = 0; i < fbody->ret_types.size(); ++i) {
    if (fbody->ret_types[i] == DT_RESOURCE) {
      output_memory_types[i] = HOST_MEMORY;
    }
  }

  // Create the kernel.
  NameAttrList function;
  TF_RETURN_IF_ERROR(NameAndAttrsFromFunctionCall(node_def, &function));
  Device* dev = flr->device();
  Status s;
  auto props = std::make_shared<NodeProperties>(
      &fbody->fdef.signature(), node_def, fbody->arg_types, fbody->ret_types);
  OpKernelConstruction construction(DeviceType(dev->device_type()), dev,
                                    dev->GetAllocator(AllocatorAttributes()),
                                    flr, dev->resource_manager(), props,
                                    input_memory_types, output_memory_types,
                                    flr->graph_def_version(), &s);

  *kernel = absl::make_unique<XlaLocalLaunchBase>(
      &construction, constant_arg_indices, resource_arg_indices, function,
      /*has_ref_vars=*/false);
  return s;
}
}  // namespace tensorflow
