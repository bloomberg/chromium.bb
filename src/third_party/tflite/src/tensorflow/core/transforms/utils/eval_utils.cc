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

#define EIGEN_USE_THREADS

#include "tensorflow/core/transforms/utils/eval_utils.h"

#include <cassert>
#include <utility>

#include "llvm/ADT/STLExtras.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "tensorflow/c/tf_status_internal.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/control_flow.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/ir/importexport/convert_tensor.h"
#include "tensorflow/core/ir/importexport/export.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/public/version.h"
#include "tensorflow/core/transforms/utils/utils.h"

namespace mlir {
namespace tfg {
namespace util {

SimpleDevice::SimpleDevice() : DeviceBase(tensorflow::Env::Default()) {
  eigen_worker_ = std::make_unique<tensorflow::thread::ThreadPool>(
      tensorflow::Env::Default(), "eval_utils", kThreads);

  eigen_worker_threads_.num_threads = kThreads;
  eigen_worker_threads_.workers = eigen_worker_.get();

  eigen_device_ = std::make_unique<Eigen::ThreadPoolDevice>(
      eigen_worker_threads_.workers->AsEigenThreadPool(),
      eigen_worker_threads_.num_threads);
  set_tensorflow_cpu_worker_threads(&eigen_worker_threads_);
  set_eigen_cpu_device(eigen_device_.get());
}

SimpleDevice::~SimpleDevice() {}

tensorflow::Allocator *SimpleDevice::GetAllocator(
    tensorflow::AllocatorAttributes attr) {
  return tensorflow::cpu_allocator();
}

tensorflow::Status SimpleDevice::MakeTensorFromProto(
    const tensorflow::TensorProto &tensor_proto,
    const tensorflow::AllocatorAttributes alloc_attrs,
    tensorflow::Tensor *tensor) {
  tensorflow::Tensor parsed(tensor_proto.dtype());
  if (!parsed.FromProto(tensorflow::cpu_allocator(), tensor_proto)) {
    return tensorflow::errors::InvalidArgument(
        "Cannot parse tensor from tensor_proto.");
  }
  *tensor = std::move(parsed);
  return tensorflow::Status::OK();
}

LogicalResult EvaluateOperation(tensorflow::DeviceBase *cpu_device,
                                tensorflow::ResourceMgr *resource_mgr, TFOp op,
                                ArrayRef<ElementsAttr> operands,
                                SmallVectorImpl<Attribute> &results) {
  assert(cpu_device && "cpu device can't be null");
  assert(resource_mgr && "ResourceMgr can't be null");

  if (llvm::any_of(operands, [](Attribute operand) { return !operand; })) {
    return op->emitError() << "cannot be evaluated with null operands";
  }

  tensorflow::NodeDef node_def;
  if (!ConvertOperationToNode(*op, &node_def).ok())
    return op->emitError() << "failed to convert operation to NodeDef";

  absl::InlinedVector<tensorflow::Tensor, 4> input_tensors(operands.size());
  absl::InlinedVector<tensorflow::TensorValue, 4> input_tensor_values(
      operands.size());
  // For each operand, convert its ElementsAttr to a Tensor and the Tensor will
  // be referenced by a TensorValue. To ensure Tensor/TensorValue have thier
  // lifecycle across the later evaluation. They are stored in
  // `input_tensors`\`input_tensor_values` respectively. The following loop zips
  // them together so that the bundled values are related. Note that the
  // accessor index associates with the order of arguments in llvm::zip.
  for (auto it : llvm::zip(operands, input_tensors, input_tensor_values)) {
    tensorflow::Tensor *input_tensor = &std::get<1>(it);
    if (!ConvertToTensor(std::get<0>(it), input_tensor).ok()) {
      return op->emitError()
             << "failed to convert " << std::get<0>(it) << " to tensor";
    }
    std::get<2>(it).tensor = input_tensor;
  }

  tensorflow::Status status;
  std::unique_ptr<tensorflow::OpKernel> op_kernel = tensorflow::CreateOpKernel(
      "CPU", cpu_device, cpu_device->GetAllocator({}), node_def,
      TF_GRAPH_DEF_VERSION, &status);
  if (!status.ok()) return op->emitError() << status.error_message();

  tensorflow::OpKernelContext::Params params;
  params.device = cpu_device;
  params.frame_iter = tensorflow::FrameAndIter(0, 0);
  params.inputs = &input_tensor_values;
  params.op_kernel = op_kernel.get();
  params.resource_manager = resource_mgr;

  absl::InlinedVector<tensorflow::AllocatorAttributes, 4> output_attrs(
      op_kernel->num_outputs());
  for (auto &attr : output_attrs) attr.set_on_host(true);
  params.output_attr_array = output_attrs.data();

  // Evaluate the operation.
  tensorflow::OpKernelContext op_context(&params);
  op_kernel->Compute(&op_context);

  // Converts the outputs to MLIR attributes.
  Builder builder(op->getContext());
  for (int i = 0; i < op_kernel->num_outputs(); ++i) {
    tensorflow::StatusOr<ElementsAttr> attr_or =
        ConvertTensor(*(op_context.mutable_output(i)), builder,
                      cast<TFGraphDialect>(op->getDialect()));
    if (!attr_or.status().ok())
      return op->emitError() << attr_or.status().error_message();
    results.push_back(attr_or.ValueOrDie());
  }

  return success();
}

}  // namespace util
}  // namespace tfg
}  // namespace mlir
