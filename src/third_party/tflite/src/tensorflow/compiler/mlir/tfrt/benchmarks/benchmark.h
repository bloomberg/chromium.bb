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

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_BENCHMARKS_BENCHMARK_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_BENCHMARKS_BENCHMARK_H_

#define EIGEN_USE_THREADS

#include <memory>

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_cpurt_pipeline.h"
#include "tensorflow/core/platform/test_benchmark.h"
#include "tfrt/cpu/jit/cpurt.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime
#include "tfrt/host_context/host_context.h"  // from @tf_runtime
#include "tfrt/tensor/dense_host_tensor.h"  // from @tf_runtime
#include "tfrt/tensor/tensor_shape.h"  // from @tf_runtime

namespace tensorflow {

// This header is a part of the library with private visibility and will be
// used only to build benchmarks for different functions in this folder, so
// it is ok to put convenience using-declarations here.

using ::tfrt::HostContext;
using ::tfrt::RemainingResults;
using ::tfrt::cpu::jit::JitExecutable;
using ::tfrt::cpu::jit::MemrefDesc;
using ::tfrt::cpu::jit::Type;

std::unique_ptr<HostContext> CreateSingleThreadedHostContext();
std::unique_ptr<HostContext> CreateMultiThreadedHostContext(int num_threads);

// Generate random Eigen Tensor of the given dimensions:
//   (rand<T>() + offset) * scale
template <typename T, int rank>
Eigen::Tensor<T, rank, Eigen::RowMajor> GenRandomTensor(
    std::array<ssize_t, rank> dims, T scale = static_cast<T>(1.0),
    T offset = static_cast<T>(0.0)) {
  Eigen::DSizes<Eigen::Index, rank> dsizes;
  for (int d = 0; d < rank; ++d) dsizes[d] = dims[d];
  Eigen::Tensor<T, rank, Eigen::RowMajor> tensor(dsizes);
  tensor.setRandom();
  tensor = scale * (tensor + offset);  // shift random numbers
  return tensor;
}

// -------------------------------------------------------------------------- //
// Run benchmark by compiling MLIR function using TFRT CPURT API.
// -------------------------------------------------------------------------- //

// Do not record any information about operands for the results conversion.
struct ResultConversionCtx {};

// Result converter that simply frees the memrefs returned from the compiled
// functions. We are not interested in the computed results, and constructing
// async values and tensors adds noticeable overhead.
mlir::LogicalResult FreeReturnedMemref(const ResultConversionCtx&,
                                       RemainingResults results,
                                       unsigned result_index, const Type* type,
                                       void* result_ptr);

// Compile serialized mlir module and convert entrypoint function into TFRT JIT
// executable.
JitExecutable& CreateJitExecutable(const HostContext& host,
                                   llvm::StringRef mlir_input,
                                   llvm::StringRef function_name,
                                   bool lower_from_tensorflow,
                                   const TfCpuRtPipelineOptions& tf_cpurt_opts);

// Converts Eigen Tensor to Memref descriptor.
template <typename T, int rank>
MemrefDesc TensorToMemrefDesc(Eigen::Tensor<T, rank, Eigen::RowMajor>& tensor) {
  tfrt::TensorShape shape(tensor.dimensions().values);
  MemrefDesc desc;
  desc.dtype = tfrt::GetDType<T>();
  desc.data = tensor.data();
  desc.offset = 0;
  shape.GetDimensions(&desc.sizes);
  shape.GetStrides(&desc.strides);
  return desc;
}

// Converts Tensorflow Tensor to Memref descriptor.
MemrefDesc TensorToMemrefDesc(const Tensor& tensor);

// -------------------------------------------------------------------------- //
// Run benchmark using Eigen expression evaluation.
// -------------------------------------------------------------------------- //

// Explicitly control if Eigen assignment should use SIMD instructions or not.
template <bool vectorize, typename Device, typename Dst, typename Expr>
struct ExecuteAssignOp {
  static void run(Device& d, Dst& dst, const Expr& expr) {
    using Assign = Eigen::TensorAssignOp<Dst, const Expr>;
    using Executor =
        Eigen::internal::TensorExecutor<const Assign, Device,
                                        /*Vectorizable=*/vectorize>;
    Executor::run(Assign(dst, expr), d);
  }
};

// -------------------------------------------------------------------------- //
// Common utilities.
// -------------------------------------------------------------------------- //

static constexpr int64_t kDynSize = mlir::ShapedType::kDynamicSize;

// Prints an MLIR tensor type, i.e. for `shape` {1, kDynSize} and `element_type`
// "f32" the output is "tensor<1x?xf32>".
std::string PrintTensorType(llvm::ArrayRef<int64_t> shape,
                            llvm::StringRef element_type);

// Prints an MLIR dense array attribute, i.e. for `array` {1, 2} the output is
// "dense<[1, 2]>".
std::string PrintDenseArray(llvm::ArrayRef<int32_t> array);

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_BENCHMARKS_BENCHMARK_H_
