/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/stream_executor/cuda/cuda_dnn.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Core"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/tensor_float_32_utils.h"
#include "tensorflow/core/util/determinism.h"
#include "tensorflow/core/util/env_var.h"
#include "tensorflow/core/util/use_cudnn.h"
#include "tensorflow/stream_executor/cuda/cuda_activation.h"
#include "tensorflow/stream_executor/cuda/cuda_diagnostics.h"
#include "tensorflow/stream_executor/cuda/cuda_driver.h"
#include "tensorflow/stream_executor/cuda/cuda_gpu_executor.h"
#include "tensorflow/stream_executor/cuda/cuda_platform_id.h"
#include "tensorflow/stream_executor/cuda/cuda_stream.h"
#include "tensorflow/stream_executor/cuda/cuda_timer.h"
#include "tensorflow/stream_executor/cuda/cudnn_version.h"
#include "tensorflow/stream_executor/dnn.h"
#include "tensorflow/stream_executor/lib/env.h"
#include "tensorflow/stream_executor/lib/error.h"
#include "tensorflow/stream_executor/lib/initialize.h"
#include "tensorflow/stream_executor/lib/mathutil.h"
#include "tensorflow/stream_executor/lib/threadpool.h"
#include "tensorflow/stream_executor/platform/logging.h"
#include "tensorflow/stream_executor/plugin_registry.h"
#include "tensorflow/stream_executor/scratch_allocator.h"
#include "tensorflow/stream_executor/stream.h"
#include "tensorflow/stream_executor/stream_executor_internal.h"
#include "tensorflow/stream_executor/stream_executor_pimpl.h"
// clang-format off
#include "third_party/gpus/cudnn/cudnn.h"
#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
#include "third_party/cudnn_frontend/include/cudnn_frontend.h"
#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
#include "absl/strings/string_view.h"
// clang-format on

#pragma clang diagnostic push

// Make sure that Eigen::half forward declaration in dnn.h matches the
// declaration in Eigen.
#pragma clang diagnostic warning "-Wmismatched-tags"

namespace stream_executor {
namespace gpu {

PLUGIN_REGISTRY_DEFINE_PLUGIN_ID(kCuDnnPlugin);

namespace {

static_assert(CUDNN_VERSION >= 7300, "cuDNN needs to be version 7.3 or higher");

// Exits the program if 'expr' doesn't return CUDNN_STATUS_SUCCESS.
#define CHECK_CUDNN_OK(expr) CHECK_EQ(expr, CUDNN_STATUS_SUCCESS)

// If 'expr' doesn't return CUDNN_STATUS_SUCCESS, returns from the current
// function with a non-successful port::Status.
#define RETURN_IF_CUDNN_ERROR(expr)                                     \
  do {                                                                  \
    cudnnStatus_t _status = (expr);                                     \
    if (!SE_PREDICT_TRUE(_status == CUDNN_STATUS_SUCCESS)) {            \
      std::ostringstream oss;                                           \
      oss << CudnnStatusToString(_status) << "\nin " << __FILE__ << "(" \
          << __LINE__ << "): '" << #expr << "'";                        \
      return port::Status(port::error::UNKNOWN, oss.str());             \
    }                                                                   \
  } while (false)

#define RETURN_MSG_IF_CUDNN_ERROR(expr)                                 \
  do {                                                                  \
    cudnnStatus_t _status = (expr).get_status();                        \
    if (!SE_PREDICT_TRUE(_status == CUDNN_STATUS_SUCCESS)) {            \
      std::ostringstream oss;                                           \
      oss << CudnnStatusToString(_status) << "\nin " << __FILE__ << "(" \
          << __LINE__ << "): '" << #expr << "' " << (expr).get_error(); \
      return port::Status(port::error::UNKNOWN, oss.str());             \
    }                                                                   \
  } while (false)

#define RETURN_FALSE_IF_CUDNN_ERROR(expr)                                \
  do {                                                                   \
    if (!SE_PREDICT_TRUE((expr).get_status() == CUDNN_STATUS_SUCCESS)) { \
      return false;                                                      \
    }                                                                    \
  } while (false)

// Converts (via narrowing) a type T value to a type U, and checks that the
// value has no value change due to the conversion.
template <typename WideT, typename NarrowT>
NarrowT CheckedNarrowing(const WideT& wide) {
  NarrowT narrow = wide;
  CHECK_EQ(narrow, wide)
      << "checked narrowing failed; values not equal post-conversion";
  return narrow;
}

std::string CudnnStatusToString(cudnnStatus_t status) {
  switch (status) {
    case CUDNN_STATUS_SUCCESS:
      return "CUDNN_STATUS_SUCCESS";
    case CUDNN_STATUS_NOT_INITIALIZED:
      return "CUDNN_STATUS_NOT_INITIALIZED";
    case CUDNN_STATUS_ALLOC_FAILED:
      return "CUDNN_STATUS_ALLOC_FAILED";
    case CUDNN_STATUS_BAD_PARAM:
      return "CUDNN_STATUS_BAD_PARAM";
    case CUDNN_STATUS_INTERNAL_ERROR:
      return "CUDNN_STATUS_INTERNAL_ERROR";
    case CUDNN_STATUS_INVALID_VALUE:
      return "CUDNN_STATUS_INVALID_VALUE";
    case CUDNN_STATUS_ARCH_MISMATCH:
      return "CUDNN_STATUS_ARCH_MISMATCH";
    case CUDNN_STATUS_MAPPING_ERROR:
      return "CUDNN_STATUS_MAPPING_ERROR";
    case CUDNN_STATUS_EXECUTION_FAILED:
      return "CUDNN_STATUS_EXECUTION_FAILED";
    case CUDNN_STATUS_NOT_SUPPORTED:
      return "CUDNN_STATUS_NOT_SUPPORTED";
    case CUDNN_STATUS_LICENSE_ERROR:
      return "CUDNN_STATUS_LICENSE_ERROR";
    case CUDNN_STATUS_RUNTIME_PREREQUISITE_MISSING:
      return "CUDNN_STATUS_RUNTIME_PREREQUISITE_MISSING";
    case CUDNN_STATUS_RUNTIME_IN_PROGRESS:
      return "CUDNN_STATUS_RUNTIME_IN_PROGRESS";
    case CUDNN_STATUS_RUNTIME_FP_OVERFLOW:
      return "CUDNN_STATUS_RUNTIME_FP_OVERFLOW";
    default:
      return absl::StrCat("<unknown cudnn status: ", static_cast<int>(status),
                          ">");
  }
}

// RAII wrapper for all calls to cuDNN with a cuDNN handle argument.
//
// See CudnnAccess::GetHandle() for details.
class CudnnHandle {
 public:
  // Takes ownership of the executor context and the lock to access cuDNN
  // using handle.
  CudnnHandle(gpu::ScopedActivateExecutorContext context,
              std::unique_ptr<absl::MutexLock> lock, cudnnHandle_t handle)
      : context_(std::move(context)), lock_(std::move(lock)), handle_(handle) {}

  // Returns cuDNN handle. To be passed directly to cuDNN APIs, don't keep
  // a copy.
  cudnnHandle_t handle() const { return handle_; }

 private:
  gpu::ScopedActivateExecutorContext context_;
  std::unique_ptr<absl::MutexLock> lock_;
  cudnnHandle_t handle_;  // Not owned.
};

}  // namespace

// Wraps a cuDNN handle and provides access to it through CudnnHandle
// instances, which also locks a mutex, acquires the CUDA context, and sets
// the stream that cuDNN should use to enqueue any work.
//
// Note: CudnnSupport::cudnn_ should be the only instantiation of this class.
class CudnnAccess {
 public:
  // Takes ownership of the handle.
  explicit CudnnAccess(cudnnHandle_t handle) : handle_(handle) {}

  ~CudnnAccess() {
    absl::MutexLock lock(&mutex_);
    cudnnDestroy(handle_);
  }

  // Creates a CudnnHandle instance for stream.
  //
  // cuDNN API calls using the same handle instance need to be serialized
  // across threads. This is guaranteed by CudnnHandle instances locking the
  // mutex owned by this class.
  //
  // Most cuDNN APIs taking a handle perform work on a CUDA stream. The
  // CudnnHandle instance acquires the executor's CUDA context and sets cuDNN
  // to use the provided stream.
  //
  // The stream argument may be null, which translates to the legacy default
  // stream. See
  // https://docs.nvidia.com/cuda/cuda-driver-api/stream-sync-behavior.html.
  // The legacy default stream synchronizes with all other streams and it is
  // therefore a bad idea (performance wise) to call any cuDNN APIs that
  // enqueue work in the stream.
  CudnnHandle GetHandle(GpuExecutor* executor, Stream* stream) {
    auto lock = absl::make_unique<absl::MutexLock>(&mutex_);
    mutex_.AssertHeld();
    gpu::ScopedActivateExecutorContext context(executor);
    CUstream cu_stream = stream ? AsGpuStreamValue(stream) : cudaStreamLegacy;
    const auto status = cudnnSetStream(handle_, cu_stream);
    CHECK_EQ(status, CUDNN_STATUS_SUCCESS) << "Failed to set cuDNN stream.";
    return CudnnHandle(std::move(context), std::move(lock), handle_);
  }

 private:
  // Guards the enqueueing of cuDNN operations via the handle_ below.
  absl::Mutex mutex_;

  // cuDNN library handle.
  cudnnHandle_t handle_ TF_GUARDED_BY(mutex_);  // Owned.
};

namespace {

// A helper function to return the internal compute type for
// RNNs in cudnn.
cudnnDataType_t GetRnnComputeType(dnn::DataType data_type);

cudnnConvolutionFwdAlgo_t ToConvForwardAlgo(dnn::AlgorithmDesc algorithm) {
  cudnnConvolutionFwdAlgo_t algo =
      cudnnConvolutionFwdAlgo_t(algorithm.algo_id());
  switch (algo) {
    case CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM:
    case CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM:
    case CUDNN_CONVOLUTION_FWD_ALGO_GEMM:
    case CUDNN_CONVOLUTION_FWD_ALGO_DIRECT:
    case CUDNN_CONVOLUTION_FWD_ALGO_FFT:
    case CUDNN_CONVOLUTION_FWD_ALGO_FFT_TILING:
    case CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD:
    case CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD_NONFUSED:
      return algo;
    default:
      LOG(FATAL) << "Unsupported Cudnn convolution forward algorithm: "
                 << algorithm.algo_id();
  }
}

cudnnConvolutionBwdDataAlgo_t ToConvBackwardDataAlgo(
    dnn::AlgorithmDesc algorithm) {
  cudnnConvolutionBwdDataAlgo_t algo =
      cudnnConvolutionBwdDataAlgo_t(algorithm.algo_id());
  switch (algo) {
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_0:
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_1:
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT:
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT_TILING:
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD:
    case CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD_NONFUSED:
      return algo;
    default:
      LOG(FATAL)
          << "Unsupported Cudnn convolution backward algorithm for data: "
          << algorithm.algo_id();
  }
}

cudnnConvolutionBwdFilterAlgo_t ToConvBackwardFilterAlgo(
    dnn::AlgorithmDesc algorithm) {
  cudnnConvolutionBwdFilterAlgo_t algo =
      cudnnConvolutionBwdFilterAlgo_t(algorithm.algo_id());
  switch (algo) {
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_0:
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_1:
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT:
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_3:
    // Based on cudnn.h, the following is not implemented.
    // case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD:
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD_NONFUSED:
    case CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT_TILING:
      return algo;
    default:
      LOG(FATAL)
          << "Unsupported Cudnn convolution backward algorithm for filter: "
          << algorithm.algo_id();
  }
}

port::StatusOr<int> GetCudnnProperty(libraryPropertyType type) {
  int value;
  RETURN_IF_CUDNN_ERROR(cudnnGetProperty(type, &value));
  return value;
}

cudnnRNNAlgo_t ToCudnnRNNAlgo(absl::optional<dnn::AlgorithmDesc> algorithm) {
  if (!algorithm.has_value()) {
    return CUDNN_RNN_ALGO_STANDARD;
  }
  cudnnRNNAlgo_t algo = static_cast<cudnnRNNAlgo_t>(algorithm->algo_id());
  switch (algo) {
    case CUDNN_RNN_ALGO_STANDARD:
    case CUDNN_RNN_ALGO_PERSIST_STATIC:
    case CUDNN_RNN_ALGO_PERSIST_DYNAMIC:
      return algo;
    default:
      LOG(FATAL) << "Unsupported Cudnn RNN algorithm: " << algorithm->algo_id();
  }
}

port::Status GetLoadedCudnnVersion(CudnnVersion* version) {
  SE_ASSIGN_OR_RETURN(version->major_version, GetCudnnProperty(MAJOR_VERSION));
  SE_ASSIGN_OR_RETURN(version->minor_version, GetCudnnProperty(MINOR_VERSION));
  SE_ASSIGN_OR_RETURN(version->patch_level, GetCudnnProperty(PATCH_LEVEL));
  return port::Status::OK();
}

#if CUDNN_MAJOR >= 8 && (CUDNN_MINOR > 0 || CUDNN_PATCHLEVEL >= 4)
void PreloadCudnnLibrary(cudnnStatus_t (*version_check_fn)(),
                         absl::string_view sub_library) {
  cudnnStatus_t status = version_check_fn();
  if (status != CUDNN_STATUS_SUCCESS) {
    VLOG(1) << "Could not pre-initialize cuDNN sub-library " << sub_library
            << ".  Error: " << cudnnGetErrorString(status) << ".";
  }
}
#endif

}  // namespace

CudnnSupport::CudnnSupport(GpuExecutor* parent) : parent_(parent) {}

port::Status CudnnSupport::Init() {
  ScopedActivateExecutorContext context(parent_);

  // Peek at the last error to give more information in cases of errors.
  cudaError_t cerr = cudaPeekAtLastError();
  if (cerr != cudaSuccess) {
    LOG(WARNING) << "There was an error before creating cudnn handle: "
                 << cudaGetErrorName(cerr) << " : " << cudaGetErrorString(cerr);
  }

  cudnnHandle_t cudnn_handle = nullptr;
  const auto status = cudnnCreate(&cudnn_handle);
  if (status == CUDNN_STATUS_SUCCESS) {
    CudnnVersion source_version(CUDNN_MAJOR, CUDNN_MINOR, CUDNN_PATCHLEVEL);

    CudnnVersion loaded_version;
    TF_RETURN_IF_ERROR(GetLoadedCudnnVersion(&loaded_version));
    if (!IsSourceCompatibleWithCudnnLibrary(source_version, loaded_version)) {
      const std::string error = absl::StrCat(
          "Loaded runtime CuDNN library: ", loaded_version.ToString(),
          " but source was compiled with: ", source_version.ToString(),
          ".  CuDNN library needs to have matching major version and equal or "
          "higher minor version. If using a binary install, upgrade your CuDNN "
          "library.  If building from sources, make sure the library loaded at "
          "runtime is compatible with the version specified during compile "
          "configuration.");
      LOG(ERROR) << error;
      cudnnDestroy(cudnn_handle);
      return port::Status(port::error::INTERNAL, error);
    }

    cudnn_.reset(new CudnnAccess(cudnn_handle));

    LOG(INFO) << "Loaded cuDNN version " << cudnnGetVersion();
    return port::Status::OK();
  }

  CHECK_EQ(cudnn_handle, nullptr);
  LOG(ERROR) << "Could not create cudnn handle: "
             << CudnnStatusToString(status);
  if (status == CUDNN_STATUS_NOT_INITIALIZED) {
    auto result = gpu::Diagnostician::FindKernelDriverVersion();
    if (!result.ok()) {
      LOG(ERROR) << "Error retrieving driver version: "
                 << cuda::DriverVersionStatusToString(result);
    } else {
      const auto& version = result.ValueOrDie();
      LOG(ERROR) << "Possibly insufficient driver version: "
                 << cuda::DriverVersionToString(version);
    }
  }

  return port::Status(port::error::INTERNAL,
                      absl::StrCat("cudnn library could not create a handle: ",
                                   CudnnStatusToString(status)));
}

port::StatusOr<perftools::gputools::dnn::VersionInfo>
CudnnSupport::GetVersion() {
  CudnnVersion version;
  TF_RETURN_IF_ERROR(GetLoadedCudnnVersion(&version));
  return perftools::gputools::dnn::VersionInfo(
      version.major_version, version.minor_version, version.patch_level);
}

namespace {

// Deleter functors for cuDNN types that need to be deleted.
struct TensorDescriptorDeleter {
  void operator()(cudnnTensorDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyTensorDescriptor(descriptor));
  }
};
struct RNNDataDescriptorDeleter {
  void operator()(cudnnRNNDataDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyRNNDataDescriptor(descriptor));
  }
};
struct FilterDescriptorDeleter {
  void operator()(cudnnFilterDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyFilterDescriptor(descriptor));
  }
};
struct ConvolutionDescriptorDeleter {
  void operator()(cudnnConvolutionDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyConvolutionDescriptor(descriptor));
  }
};
struct PoolingDescriptorDeleter {
  void operator()(cudnnPoolingDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyPoolingDescriptor(descriptor));
  }
};
struct LrnDescriptorDeleter {
  void operator()(cudnnLRNDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyLRNDescriptor(descriptor));
  }
};

struct ActivationDescriptorDeleter {
  void operator()(cudnnActivationDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyActivationDescriptor(descriptor));
  }
};
struct DropoutDescriptorDeleter {
  void operator()(cudnnDropoutDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyDropoutDescriptor(descriptor));
  }
};
struct RnnDescriptorDeleter {
  void operator()(cudnnRNNDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyRNNDescriptor(descriptor));
  }
};
struct PersistentRnnPlanDeleter {
  void operator()(cudnnPersistentRNNPlan_t plan) const {
    CHECK_CUDNN_OK(cudnnDestroyPersistentRNNPlan(plan));
  }
};
#if CUDNN_VERSION >= 7603
struct CtcLossDescriptorDeleter {
  void operator()(cudnnCTCLossDescriptor_t descriptor) const {
    CHECK_CUDNN_OK(cudnnDestroyCTCLossDescriptor(descriptor));
  }
};
#endif

// RAII wrappers for cuDNN types.
using TensorDescriptor =
    std::unique_ptr<cudnnTensorStruct, TensorDescriptorDeleter>;
using RNNDataDescriptor =
    std::unique_ptr<cudnnRNNDataStruct, RNNDataDescriptorDeleter>;
using FilterDescriptor =
    std::unique_ptr<cudnnFilterStruct, FilterDescriptorDeleter>;
using ConvolutionDescriptor =
    std::unique_ptr<cudnnConvolutionStruct, ConvolutionDescriptorDeleter>;
using PoolingDescriptor =
    std::unique_ptr<cudnnPoolingStruct, PoolingDescriptorDeleter>;
using LrnDescriptor = std::unique_ptr<cudnnLRNStruct, LrnDescriptorDeleter>;
using ActivationDescriptor =
    std::unique_ptr<cudnnActivationStruct, ActivationDescriptorDeleter>;
using DropoutDescriptor =
    std::unique_ptr<cudnnDropoutStruct, DropoutDescriptorDeleter>;
using RnnDescriptor = std::unique_ptr<cudnnRNNStruct, RnnDescriptorDeleter>;
using PersistentRnnPlan =
    std::unique_ptr<cudnnPersistentRNNPlan, PersistentRnnPlanDeleter>;
#if CUDNN_VERSION >= 7603
using CtcLossDescriptor =
    std::unique_ptr<cudnnCTCLossStruct, CtcLossDescriptorDeleter>;
#endif

// Factory methods for cuDNN types.
TensorDescriptor CreateTensorDescriptor() {
  cudnnTensorDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateTensorDescriptor(&result));
  return TensorDescriptor(result);
}
RNNDataDescriptor CreateRNNDataDescriptor() {
  cudnnRNNDataDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateRNNDataDescriptor(&result));
  return RNNDataDescriptor(result);
}
FilterDescriptor CreateFilterDescriptor() {
  cudnnFilterDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateFilterDescriptor(&result));
  return FilterDescriptor(result);
}
ConvolutionDescriptor CreateConvolutionDescriptor() {
  cudnnConvolutionDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateConvolutionDescriptor(&result));
  return ConvolutionDescriptor(result);
}
PoolingDescriptor CreatePoolingDescriptor() {
  cudnnPoolingDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreatePoolingDescriptor(&result));
  return PoolingDescriptor(result);
}
LrnDescriptor CreateLrnDescriptor() {
  cudnnLRNDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateLRNDescriptor(&result));
  return LrnDescriptor(result);
}
ActivationDescriptor CreateActivationDescriptor() {
  cudnnActivationDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateActivationDescriptor(&result));
  return ActivationDescriptor(result);
}
DropoutDescriptor CreateDropoutDescriptor() {
  cudnnDropoutDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateDropoutDescriptor(&result));
  return DropoutDescriptor(result);
}
RnnDescriptor CreateRnnDescriptor() {
  cudnnRNNDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateRNNDescriptor(&result));
  return RnnDescriptor(result);
}
#if CUDNN_VERSION >= 7603
CtcLossDescriptor CreateCtcLossDescriptor() {
  cudnnCTCLossDescriptor_t result;
  CHECK_CUDNN_OK(cudnnCreateCTCLossDescriptor(&result));
  return CtcLossDescriptor(result);
}
#endif

port::StatusOr<PersistentRnnPlan> CreatePersistentRnnPlan(
    cudnnRNNDescriptor_t rnn_desc, int batch_size, cudnnDataType_t data_type) {
  cudnnPersistentRNNPlan_t result;
  RETURN_IF_CUDNN_ERROR(
      cudnnCreatePersistentRNNPlan(rnn_desc, batch_size, data_type, &result));
  return port::StatusOr<PersistentRnnPlan>(PersistentRnnPlan(result));
}

// Turns a BatchDescriptor structure into a cudnn tensor handle within a
// scope.
class CudnnTensorDescriptor {
 public:
  CudnnTensorDescriptor(const dnn::BatchDescriptor& batch_descriptor,
                        cudnnDataType_t elem_type)
      : handle_(CreateTensorDescriptor()) {
    switch (batch_descriptor.layout()) {
      case dnn::DataLayout::kBatchYXDepth:
      case dnn::DataLayout::kBatchDepthYX: {
        const int nd = batch_descriptor.ndims() + 2;
        // cuDNN requires the strides and dims to be ordered as BDYX.
        std::vector<int64_t> strides64 =
            batch_descriptor.full_strides(dnn::DataLayout::kBatchDepthYX);
        std::vector<int64_t> dims64 =
            batch_descriptor.full_dims(dnn::DataLayout::kBatchDepthYX);

        // cuDNN requires arrays of ints.
        std::vector<int> strides(nd);
        std::vector<int> dims(nd);
        std::transform(strides64.cbegin(), strides64.cend(), strides.begin(),
                       &CheckedNarrowing<int64_t, int>);
        std::transform(dims64.cbegin(), dims64.cend(), dims.begin(),
                       &CheckedNarrowing<int64_t, int>);
        CHECK_CUDNN_OK(cudnnSetTensorNdDescriptor(handle_.get(), elem_type, nd,
                                                  dims.data(), strides.data()))
            << "batch_descriptor: " << batch_descriptor.ToString();
        break;
      }
      case dnn::DataLayout::kBatchDepthYX4:
      case dnn::DataLayout::kBatchDepthYX32: {
        auto expected_elem_ty =
            batch_descriptor.layout() == dnn::DataLayout::kBatchDepthYX4
                ? CUDNN_DATA_INT8x4
                : CUDNN_DATA_INT8x32;
        CHECK_EQ(elem_type, expected_elem_ty);
        CHECK_CUDNN_OK(cudnnSetTensor4dDescriptor(
            handle_.get(), CUDNN_TENSOR_NCHW_VECT_C, elem_type,
            batch_descriptor.count(), batch_descriptor.feature_map_count(),
            batch_descriptor.height(), batch_descriptor.width()))
            << "batch_descriptor: " << batch_descriptor.ToString();
        break;
      }
      default:
        LOG(FATAL) << "Unsupported tensor format "
                   << DataLayoutString(batch_descriptor.layout());
        break;
    }
  }

  cudnnTensorDescriptor_t handle() const { return handle_.get(); }

 private:
  TensorDescriptor handle_;
};

// Turns a FilterDescriptor structure into a cudnn filter handle within a
// scope.
class CudnnFilterDescriptor {
 public:
  CudnnFilterDescriptor(const dnn::FilterDescriptor& filter_descriptor,
                        cudnnDataType_t elem_type)
      : handle_(CreateFilterDescriptor()) {
    // TODO(b/23032134): Even if the filter layout is not supported,
    // cudnnSetFilter4DDescriptor_v4 will return CUDNN_STATUS_SUCCESS because
    // it does not take layout as an input. Maybe force cuDNN by giving wrong
    // inputs intentionally?
    cudnnTensorFormat_t format;
    switch (filter_descriptor.layout()) {
      case dnn::FilterLayout::kOutputInputYX:
        format = CUDNN_TENSOR_NCHW;
        break;
      case dnn::FilterLayout::kOutputYXInput:
        format = CUDNN_TENSOR_NHWC;
        break;
      case dnn::FilterLayout::kOutputInputYX4:
      case dnn::FilterLayout::kOutputInputYX32: {
        auto expected_elem_ty =
            filter_descriptor.layout() == dnn::FilterLayout::kOutputInputYX4
                ? CUDNN_DATA_INT8x4
                : CUDNN_DATA_INT8x32;
        CHECK_EQ(elem_type, expected_elem_ty);
        format = CUDNN_TENSOR_NCHW_VECT_C;
        break;
      }
      default:
        LOG(FATAL) << "Unsupported filter format "
                   << FilterLayoutString(filter_descriptor.layout());
        break;
    }

    std::vector<int> dims(2 + filter_descriptor.ndims());
    dims[0] = filter_descriptor.output_feature_map_count();
    dims[1] = filter_descriptor.input_feature_map_count();
    absl::Span<const int64_t> spatial_dims =
        filter_descriptor.input_filter_dims();
    std::copy(spatial_dims.begin(), spatial_dims.end(), dims.begin() + 2);

    CHECK_CUDNN_OK(cudnnSetFilterNdDescriptor(handle_.get(), elem_type, format,
                                              dims.size(), dims.data()));
  }

  cudnnFilterDescriptor_t handle() const { return handle_.get(); }

 private:
  FilterDescriptor handle_;  // Owned.
};

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
// The errata sheet (JSON format) for marking the cudnn engines that might be
// buggy. For example, we don't want the engine 999 of forward convolution:
// R"({ "version" : 1,
//      "rules"   : [
//        { "rule_id"             : "ConvFwd_eng999",
//          "operation"           : "ConvFwd",
//          "engine"              : 999,
//          "knob"                : [],
//          "cudnn_version_start" : 8000,
//          "cudnn_version_end"   : -1
//        }
// ]})"
// We skip a non-existing eng999 in the static filter as a placeholder.
// Additionally, users can specify an additional errata JSON file via
// CUDNN_ERRATA_JSON_FILE at runtime.
const json* CudnnExecutionPlanEngineFilterStatic() {
  static absl::string_view filter_str = R"({
      "version" : 1,
        "rules"   : [
          { "rule_id"             : "ConvFwd_eng999",
            "operation"           : "ConvFwd",
            "engine"              : 999,
            "knob"                : [],
            "cudnn_version_start" : 8000,
            "cudnn_version_end"   : -1
          }
      ]})";
  static const json* json_handle = new json(json::parse(filter_str));
  return json_handle;
}

const json* CudnnExecutionPlanEngineFilterRuntime() {
  static const json* json_handle = []() -> const json* {
    json j;
    if (cudnn_frontend::load_from_config(j, "")) {
      return new json(j);
    }
    return nullptr;
  }();
  return json_handle;
}

#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

// A helper function to decide whether to use
// CUDNN_BATCHNORM_SPATIAL_PERSISTENT in batchnorm. This mode can be faster in
// some tasks because an optimized path may be selected for CUDNN_DATA_FLOAT
// and CUDNN_DATA_HALF data types, compute capability 6.0 or higher. The
// reason we set it to false by default is that this mode may use scaled
// atomic integer reduction that may cause a numerical overflow for certain
// input data range.
// TODO(yangzihao): Use autotune to choose between this mode and
// CUDNN_BATCHNORM_SPATIAL mode.
bool BatchnormSpatialPersistentEnabled() {
  static bool is_enabled = [] {
    bool is_enabled = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar(
        "TF_USE_CUDNN_BATCHNORM_SPATIAL_PERSISTENT",
        /*default_val=*/false, &is_enabled));
    return is_enabled;
  }();
  return is_enabled;
}

bool RequireCudnnDeterminism() {
  static bool require_cudnn_determinism = [] {
    // TODO(reedwm): Remove the TF_CUDNN_DETERMINISTIC env var.
    bool cudnn_deterministic = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar("TF_CUDNN_DETERMINISTIC",
                                               /*default_val=*/false,
                                               &cudnn_deterministic));
    return cudnn_deterministic;
  }();
  return tensorflow::OpDeterminismRequired() || require_cudnn_determinism;
}

// A helper function to decide whether to force the default conv algorithm.
bool ConvUseDefaultAlgorithm() {
  static bool use_default = [] {
    bool use_default = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar("TF_USE_DEFAULT_CONV_ALGO",
                                               /*default_val=*/false,
                                               &use_default));
    return use_default;
  }();
  return use_default;
}

// Turns a ConvolutionDescriptor structure into a cudnn convolution handle
// within a scope.
class CudnnConvolutionDescriptor {
 public:
  CudnnConvolutionDescriptor(
      const dnn::ConvolutionDescriptor& convolution_descriptor,
      cudnnDataType_t data_type)
      : handle_(CreateConvolutionDescriptor()) {
    absl::Span<const int64_t> strides64 = convolution_descriptor.strides();
    absl::Span<const int64_t> padding64 = convolution_descriptor.padding();
    absl::Span<const int64_t> dilations64 = convolution_descriptor.dilations();
    CHECK_NE(convolution_descriptor.pad_alignment(),
             dnn::PadAlignment::kTensorFlowPadding)
        << "TensorFlow padding alignment is not supported.";

    // cuDNN requires arrays of ints.
    std::vector<int> strides(convolution_descriptor.ndims());
    std::vector<int> padding(convolution_descriptor.ndims());
    std::vector<int> dilations(convolution_descriptor.ndims());
    std::transform(strides64.cbegin(), strides64.cend(), strides.begin(),
                   &CheckedNarrowing<int64_t, int>);
    std::transform(padding64.cbegin(), padding64.cend(), padding.begin(),
                   &CheckedNarrowing<int64_t, int>);
    // TODO(yangzihao): Test with negative dilation to make sure that cudnn
    // doesn't crash.
    std::transform(dilations64.cbegin(), dilations64.cend(), dilations.begin(),
                   &CheckedNarrowing<int64_t, int>);

    CHECK_CUDNN_OK(cudnnSetConvolutionNdDescriptor(
        handle_.get(), convolution_descriptor.ndims(), padding.data(),
        strides.data(), dilations.data(),
        convolution_descriptor.convolution_not_crosscorr()
            ? CUDNN_CONVOLUTION
            : CUDNN_CROSS_CORRELATION,
        data_type));

#if CUDNN_MAJOR >= 7
    VLOG(2) << "Requesting grouped convolution: "
            << convolution_descriptor.group_count();
    CHECK_CUDNN_OK(cudnnSetConvolutionGroupCount(
        handle_.get(), convolution_descriptor.group_count()));
#else
    CHECK_EQ(convolution_descriptor.group_count(), 1)
        << "Requested grouped convolution for cuDNN version < 7";
#endif
  }

  void set_use_tensor_op_math(bool use_tensor_op_math) {
    cudnnMathType_t math_type =
#if CUDNN_VERSION >= 8000
        (use_tensor_op_math ? CUDNN_TENSOR_OP_MATH : CUDNN_FMA_MATH);
#else
        (use_tensor_op_math ? CUDNN_TENSOR_OP_MATH : CUDNN_DEFAULT_MATH);
#endif
    CHECK_CUDNN_OK(cudnnSetConvolutionMathType(handle_.get(), math_type));
  }

  cudnnConvolutionDescriptor_t handle() const { return handle_.get(); }

 private:
  ConvolutionDescriptor handle_;  // Owned.
};

// A helper function to query if a CudnnConvolutionDescriptor has tensor_op_math
// set
static bool IsTensorMathOpSet(const CudnnConvolutionDescriptor& conv) {
  cudnnMathType_t math_type;
  CHECK_CUDNN_OK(cudnnGetConvolutionMathType(conv.handle(), &math_type));
#if CUDNN_VERSION >= 8000
  return math_type != CUDNN_FMA_MATH;
#else
  return math_type == CUDNN_TENSOR_OP_MATH;
#endif
}

static bool TensorOpMathAvailable(
    CudaComputeCapability cuda_compute_capability) {
  return cuda_compute_capability.IsAtLeast(7);
}

static bool IsTensorMathEnabled(Stream* stream, dnn::DataType input_type) {
  if (!TensorOpMathAvailable(stream->GetCudaComputeCapability())) {
    return false;
  }
  if (input_type == dnn::DataType::kFloat) {
#if CUDNN_VERSION < 8000
    return false;
#else
    if (!tensorflow::tensor_float_32_execution_enabled()) {
      return false;
    }
#endif
  }
  return true;
}

// Turns a PoolingDescriptor structure into a cudnn pooling descriptor handle
// within a scope.
class CudnnPoolingDescriptor {
 public:
  explicit CudnnPoolingDescriptor(
      const dnn::PoolingDescriptor& pooling_descriptor)
      : handle_(CreatePoolingDescriptor()) {
    absl::Span<const int64_t> strides64 = pooling_descriptor.strides();
    absl::Span<const int64_t> padding64 = pooling_descriptor.padding();
    absl::Span<const int64_t> shape64 = pooling_descriptor.window();

    const int nd = pooling_descriptor.ndims();
    std::vector<int> shape(nd);
    std::vector<int> padding(nd);
    std::vector<int> strides(nd);
    std::transform(strides64.cbegin(), strides64.cend(), strides.begin(),
                   &CheckedNarrowing<int64_t, int>);
    std::transform(padding64.cbegin(), padding64.cend(), padding.begin(),
                   &CheckedNarrowing<int64_t, int>);
    std::transform(shape64.cbegin(), shape64.cend(), shape.begin(),
                   &CheckedNarrowing<int64_t, int>);
    bool propagate_nans = pooling_descriptor.propagate_nans();
    const auto cudnn_max_pooling_mode = RequireCudnnDeterminism()
                                            ? CUDNN_POOLING_MAX_DETERMINISTIC
                                            : CUDNN_POOLING_MAX;
    CHECK_CUDNN_OK(cudnnSetPoolingNdDescriptor(
        handle_.get(),
        (pooling_descriptor.mode() == dnn::PoolingMode::kMaximum
             ? cudnn_max_pooling_mode
             : CUDNN_POOLING_AVERAGE_COUNT_EXCLUDE_PADDING),
        propagate_nans ? CUDNN_PROPAGATE_NAN : CUDNN_NOT_PROPAGATE_NAN, nd,
        shape.data(), padding.data(), strides.data()));
  }

  cudnnPoolingDescriptor_t handle() const { return handle_.get(); }

 private:
  PoolingDescriptor handle_;  // Owned.

  SE_DISALLOW_COPY_AND_ASSIGN(CudnnPoolingDescriptor);
};

// Turns a NormalizeDescriptor structure into a cudnn LRN descriptor handle.
class CudnnNormalizeDescriptor {
 public:
  explicit CudnnNormalizeDescriptor(
      const dnn::NormalizeDescriptor& normalize_descriptor)
      : handle_(CreateLrnDescriptor()) {
    // The range specifies that the indices in the closed range
    // [i - range, i + range] should be included in the normalization for index
    // i. The lrnN value is the total number of elements in the range, so
    // lrnN = 2*range + 1.
    unsigned lrnN = 2 * normalize_descriptor.range() + 1;

    // Note that SE defines the normalization operation as
    //
    //  U_i = V_i / ((bias +  alpha      * (sum_j V_j^2)) ^ beta)
    //
    // but cuDNN defines it as
    //
    //  U_i = V_i / ((bias + (alpha / n) * (sum_j V_j^2)) ^ beta)
    //
    // i.e. there is a factor of n difference between the meaning of the alphas
    // in the two contexts. The cuDNN alpha is n times the SE alpha.
    double lrnAlpha = lrnN * normalize_descriptor.alpha();

    double lrnBeta = normalize_descriptor.beta();
    double lrnK = normalize_descriptor.bias();
    CHECK_CUDNN_OK(
        cudnnSetLRNDescriptor(handle_.get(), lrnN, lrnAlpha, lrnBeta, lrnK));
  }

  cudnnLRNDescriptor_t handle() const { return handle_.get(); }

 private:
  LrnDescriptor handle_;  // Owned.

  SE_DISALLOW_COPY_AND_ASSIGN(CudnnNormalizeDescriptor);
};

// Turns a ActivationDescriptor structure into a cudnn activation
// descriptor handle within a scope.
class CudnnActivationDescriptor {
 public:
  CudnnActivationDescriptor(dnn::ActivationMode activation_mode,
                            cudnnNanPropagation_t nan_propagation,
                            double value_max)
      : handle_(CreateActivationDescriptor()) {
    double relu_ceiling = 0.0;
    cudnnActivationMode_t mode;
    switch (activation_mode) {
      case dnn::ActivationMode::kNone:
        mode = CUDNN_ACTIVATION_IDENTITY;
        break;
      case dnn::ActivationMode::kRelu6:
        relu_ceiling = 6.0;
        mode = CUDNN_ACTIVATION_CLIPPED_RELU;
        break;
      case dnn::ActivationMode::kReluX:
        relu_ceiling = value_max;
        mode = CUDNN_ACTIVATION_CLIPPED_RELU;
        break;
      case dnn::ActivationMode::kRelu:
        mode = CUDNN_ACTIVATION_RELU;
        break;
      case dnn::ActivationMode::kSigmoid:
        mode = CUDNN_ACTIVATION_SIGMOID;
        break;
      case dnn::ActivationMode::kTanh:
        mode = CUDNN_ACTIVATION_TANH;
        break;
      default:
        LOG(FATAL) << "unrecognized activation mode: "
                   << static_cast<int>(activation_mode);
    }

    CHECK_CUDNN_OK(cudnnSetActivationDescriptor(handle_.get(), mode,
                                                nan_propagation, relu_ceiling));
  }

  cudnnActivationDescriptor_t handle() const { return handle_.get(); }

 private:
  ActivationDescriptor handle_;  // Owned.
};

cudnnDataType_t ToCudnnDataType(
    dnn::DataType data_type,
    dnn::DataLayout data_layout = dnn::DataLayout::kBatchDepthYX) {
  switch (data_type) {
    case dnn::DataType::kFloat:
      return CUDNN_DATA_FLOAT;
    case dnn::DataType::kDouble:
      return CUDNN_DATA_DOUBLE;
    case dnn::DataType::kHalf:
      return CUDNN_DATA_HALF;
    case dnn::DataType::kInt8:
      switch (data_layout) {
        case dnn::DataLayout::kBatchDepthYX4:
          return CUDNN_DATA_INT8x4;
        case dnn::DataLayout::kBatchDepthYX32:
          return CUDNN_DATA_INT8x32;
        default:
          return CUDNN_DATA_INT8;
      }
    case dnn::DataType::kInt32:
      return CUDNN_DATA_INT32;
#if CUDNN_VERSION >= 8200
    case dnn::DataType::kBF16:
      return CUDNN_DATA_BFLOAT16;
#endif
    default:
      LOG(FATAL) << "Invalid DNN data type: " << static_cast<int>(data_type);
  }
}

cudnnDataType_t ToCudnnDataType(dnn::DataType data_type,
                                dnn::FilterLayout filter_layout) {
  if (data_type == dnn::DataType::kInt8 &&
      filter_layout == dnn::FilterLayout::kOutputInputYX4) {
    return CUDNN_DATA_INT8x4;
  }
  if (data_type == dnn::DataType::kInt8 &&
      filter_layout == dnn::FilterLayout::kOutputInputYX32) {
    return CUDNN_DATA_INT8x32;
  }
  return ToCudnnDataType(data_type);
}

template <typename T>
cudnnDataType_t GetCudnnDataType(
    dnn::DataLayout data_layout = dnn::DataLayout::kBatchDepthYX) {
  return ToCudnnDataType(dnn::ToDataType<T>::value, data_layout);
}

template <typename T>
cudnnDataType_t GetCudnnDataType(dnn::FilterLayout filter_layout) {
  return ToCudnnDataType(dnn::ToDataType<T>::value, filter_layout);
}

cudnnRNNInputMode_t ToCudnnRnnInputMode(dnn::RnnInputMode input_mode) {
  switch (input_mode) {
    case dnn::RnnInputMode::kRnnLinearSkip:
    case dnn::RnnInputMode::kRnnSkipInput:
      return static_cast<cudnnRNNInputMode_t>(input_mode);
    default:
      LOG(FATAL) << "Invalid RNN input mode: " << static_cast<int>(input_mode);
  }
}

cudnnDirectionMode_t ToCudnnRnnDirectionMode(
    dnn::RnnDirectionMode direction_mode) {
  switch (direction_mode) {
    case dnn::RnnDirectionMode::kRnnUnidirectional:
    case dnn::RnnDirectionMode::kRnnBidirectional:
      return static_cast<cudnnDirectionMode_t>(direction_mode);
    default:
      LOG(FATAL) << "Invalid RNN direction mode: "
                 << static_cast<int>(direction_mode);
  }
}

cudnnRNNMode_t ToCudnnRnnMode(dnn::RnnMode rnn_mode) {
  switch (rnn_mode) {
    case dnn::RnnMode::kRnnRelu:
    case dnn::RnnMode::kRnnTanh:
    case dnn::RnnMode::kRnnLstm:
    case dnn::RnnMode::kRnnGru:
      return static_cast<cudnnRNNMode_t>(rnn_mode);
    default:
      LOG(FATAL) << "Invalid RNN Mode: " << static_cast<int>(rnn_mode);
  }
}

int CudnnDataTypeToByteSize(cudnnDataType_t data_type) {
  switch (data_type) {
    case CUDNN_DATA_FLOAT:
      return sizeof(float);
    case CUDNN_DATA_DOUBLE:
      return sizeof(double);
    case CUDNN_DATA_HALF:
      return sizeof(Eigen::half);
    default:
      LOG(FATAL) << "Invalid DNN data type: " << static_cast<int>(data_type);
  }
}

class CudnnDropoutDescriptor {
  explicit CudnnDropoutDescriptor(DropoutDescriptor handle)
      : handle_(std::move(handle)) {}

 public:
  CudnnDropoutDescriptor(CudnnDropoutDescriptor&&) = default;

  static port::StatusOr<CudnnDropoutDescriptor> Create(
      const CudnnHandle& cudnn, float dropout, uint64_t seed,
      ScratchAllocator* state_allocator) {
    DropoutDescriptor handle = CreateDropoutDescriptor();

    if (dropout == 0.0f) {
      // Return 'empty' dropout descriptor.
      return CudnnDropoutDescriptor(std::move(handle));
    }

    DeviceMemory<uint8> state_memory;
    if (state_allocator) {
      size_t state_sizes_in_bytes = 0;
      RETURN_IF_CUDNN_ERROR(
          cudnnDropoutGetStatesSize(cudnn.handle(), &state_sizes_in_bytes));
      SE_ASSIGN_OR_RETURN(state_memory,
                          state_allocator->AllocateBytes(state_sizes_in_bytes));
    }
    RETURN_IF_CUDNN_ERROR(cudnnSetDropoutDescriptor(
        handle.get(), cudnn.handle(), dropout, state_memory.opaque(),
        state_memory.size(), seed));

    return CudnnDropoutDescriptor(std::move(handle));
  }

  cudnnDropoutDescriptor_t handle() const { return handle_.get(); }

 private:
  DropoutDescriptor handle_;  // Owned.
  SE_DISALLOW_COPY_AND_ASSIGN(CudnnDropoutDescriptor);
};

class CudnnRnnParamsDescriptor {
  typedef dnn::RnnDescriptor::ParamsRegions ParamsRegions;

  CudnnRnnParamsDescriptor(FilterDescriptor handle,
                           int64_t params_size_in_bytes, ParamsRegions weights,
                           ParamsRegions biases)
      : handle_(std::move(handle)),
        params_size_in_bytes_(params_size_in_bytes),
        weights_(std::move(weights)),
        biases_(std::move(biases)) {}

 public:
  CudnnRnnParamsDescriptor(CudnnRnnParamsDescriptor&&) = default;

  static port::StatusOr<CudnnRnnParamsDescriptor> Create(
      const CudnnHandle& cudnn, int input_size, cudnnDataType_t data_type,
      cudnnRNNDescriptor_t rnn_desc, cudnnRNNMode_t rnn_mode,
      cudnnDirectionMode_t direction_mode, int num_layers);

  cudnnFilterDescriptor_t handle() const { return handle_.get(); }
  int64_t params_size_in_bytes() const { return params_size_in_bytes_; }
  ParamsRegions params_weights() const { return weights_; }
  ParamsRegions params_biases() const { return biases_; }

 private:
  FilterDescriptor handle_;
  int64_t params_size_in_bytes_;
  ParamsRegions weights_;
  ParamsRegions biases_;
  SE_DISALLOW_COPY_AND_ASSIGN(CudnnRnnParamsDescriptor);
};

}  // namespace

class CudnnRnnDescriptor : public dnn::RnnDescriptor {
  CudnnRnnDescriptor(const CudnnHandle& cudnn, gpu::RnnDescriptor rnn_desc,
                     PersistentRnnPlan rnn_plan, int num_layers,
                     int hidden_size, int input_size, int cell_size,
                     int batch_size, cudnnRNNInputMode_t input_mode,
                     cudnnDirectionMode_t direction_mode,
                     cudnnRNNMode_t rnn_mode, cudnnDataType_t data_type,
                     cudnnDataType_t compute_type,
                     const dnn::AlgorithmConfig& algorithm_config,
                     CudnnDropoutDescriptor dropout_desc,
                     CudnnRnnParamsDescriptor params_desc)
      : rnn_desc_(std::move(rnn_desc)),
        rnn_plan_(std::move(rnn_plan)),
        num_layers_(num_layers),
        hidden_size_(hidden_size),
        input_size_(input_size),
        cell_size_(cell_size),
        batch_size_(batch_size),
        rnn_algo_(ToCudnnRNNAlgo(algorithm_config.algorithm())),
        input_mode_(input_mode),
        direction_mode_(direction_mode),
        rnn_mode_(rnn_mode),
        data_type_(data_type),
        compute_type_(compute_type),
        algorithm_config_(algorithm_config),
        dropout_desc_(std::move(dropout_desc)),
        params_desc_(std::move(params_desc)) {}

 public:
  CudnnRnnDescriptor(CudnnRnnDescriptor&& other) = default;

  static port::StatusOr<CudnnRnnDescriptor> Create(
      const CudnnHandle& cudnn, int num_layers, int hidden_size, int input_size,
      int cell_size, int batch_size, cudnnRNNInputMode_t input_mode,
      cudnnDirectionMode_t direction_mode, cudnnRNNMode_t rnn_mode,
      cudnnDataType_t data_type, cudnnDataType_t compute_type,
      const dnn::AlgorithmConfig& algorithm_config, float dropout,
      uint64_t seed, ScratchAllocator* state_allocator, bool use_padded_io) {
    SE_ASSIGN_OR_RETURN(
        CudnnDropoutDescriptor dropout_desc,
        CudnnDropoutDescriptor::Create(cudnn, dropout, seed, state_allocator));

    gpu::RnnDescriptor rnn_desc = CreateRnnDescriptor();
    cudnnRNNAlgo_t rnn_algo = ToCudnnRNNAlgo(algorithm_config.algorithm());

    // TODO: allow the user to choose an algorithm.
    auto proj_size = hidden_size;
    hidden_size = std::max(hidden_size, cell_size);

    // Require explicit algorithm config to enable tensor cores. Some configs
    // return CUDNN_NOT_SUPPORTED when tensor ops are enabled (which is against
    // the idiom that enabling tensor ops is only a hint: see nvbugs/2172799).
    // We can only reasonably expect the user to handle the subsequent failure
    // in profile mode, which is run with algorithms returned from
    // GetRnnAlgorithms() (which are non-default and explicitly set whether to
    // use tensor ops). CuDNN 7.2.1 fixed this issue.
    // TODO(csigg): Minimal support cuDNN version is 7.3, clean up.
    bool allow_tensor_ops = data_type == CUDNN_DATA_HALF;
    if (data_type == CUDNN_DATA_FLOAT)
      allow_tensor_ops = tensorflow::tensor_float_32_execution_enabled();
    bool use_tensor_ops =
        algorithm_config.algorithm().has_value()
            ? algorithm_config.algorithm()->tensor_ops_enabled()
            : allow_tensor_ops;
    if (use_tensor_ops && !allow_tensor_ops) {
      return port::Status(port::error::INVALID_ARGUMENT,
                          "Algo requests disallowed tensor op evaluation.");
    }

#if CUDNN_VERSION >= 8000
    cudnnMathType_t math_type =
        use_tensor_ops ? CUDNN_TENSOR_OP_MATH : CUDNN_FMA_MATH;
#else
    cudnnMathType_t math_type =
        use_tensor_ops ? CUDNN_TENSOR_OP_MATH : CUDNN_DEFAULT_MATH;
#endif

#if CUDNN_VERSION >= 8000
    cudnnRNNBiasMode_t bias_mode = CUDNN_RNN_DOUBLE_BIAS;
    uint32_t aux_flags = 0;
    if (use_padded_io) aux_flags |= CUDNN_RNN_PADDED_IO_ENABLED;
    RETURN_IF_CUDNN_ERROR(cudnnSetRNNDescriptor_v8(
        /*rnnDesc=*/rnn_desc.get(), /*algo=*/rnn_algo, /*cellMode=*/rnn_mode,
        /*biasMode=*/bias_mode, /*dirMode=*/direction_mode,
        /*inputMode=*/input_mode,
        /*dataType=*/data_type, /*mathPrec=*/compute_type,
        /*mathType=*/math_type,
        /*inputSize=*/input_size,
        /*hiddenSize=*/hidden_size, /*projSize=*/proj_size,
        /*numLayers=*/num_layers,
        /*dropoutDesc=*/dropout_desc.handle(),
        /*auxFlags=*/aux_flags));
#else
    RETURN_IF_CUDNN_ERROR(cudnnSetRNNDescriptor_v6(
        cudnn.handle(), /*rnnDesc=*/rnn_desc.get(),
        /*hiddenSize=*/hidden_size, /*numLayers=*/num_layers,
        /*dropoutDesc=*/dropout_desc.handle(), /*inputMode=*/input_mode,
        /*direction=*/direction_mode, /*mode=*/rnn_mode, /*algo=*/rnn_algo,
        /*dataType=*/compute_type));
    CHECK_CUDNN_OK(cudnnSetRNNMatrixMathType(rnn_desc.get(), math_type));

    if (proj_size < hidden_size) {
      RETURN_IF_CUDNN_ERROR(cudnnSetRNNProjectionLayers(
          cudnn.handle(), /*rnnDesc=*/rnn_desc.get(),
          /*recProjSize=*/proj_size, /*outProjSize=*/0));
    }

    // TODO: For now, we only use cudnnRNN**Ex API to process padded inputs.
    // But in the future if these APIs are used to process full length arrays,
    // we need to distinguish when to set it.
    if (use_padded_io) {
      RETURN_IF_CUDNN_ERROR(
          cudnnSetRNNPaddingMode(rnn_desc.get(), CUDNN_RNN_PADDED_IO_ENABLED));
    }
#endif

    port::StatusOr<PersistentRnnPlan> rnn_plan_wrapper;
    PersistentRnnPlan rnn_plan;
    if (rnn_algo == CUDNN_RNN_ALGO_PERSIST_DYNAMIC) {
      CHECK_GE(batch_size, 0);
      rnn_plan_wrapper =
          CreatePersistentRnnPlan(rnn_desc.get(), batch_size, data_type);
      if (!rnn_plan_wrapper.ok()) {
        return port::StatusOr<CudnnRnnDescriptor>(rnn_plan_wrapper.status());
      } else {
        rnn_plan = rnn_plan_wrapper.ConsumeValueOrDie();
        RETURN_IF_CUDNN_ERROR(
            cudnnSetPersistentRNNPlan(rnn_desc.get(), rnn_plan.get()));
      }
    }

    // Create the params handle.
    // TODO(kaixih@nvidia.com): Should be removed when cudnnRNNForward*** and
    // cudnnRNNForward***Ex are removed from the codebase, since the new API
    // doesn't need param descriptors any more.
    SE_ASSIGN_OR_RETURN(auto params_desc,
                        CudnnRnnParamsDescriptor::Create(
                            cudnn, input_size, data_type, rnn_desc.get(),
                            rnn_mode, direction_mode, num_layers));

    return CudnnRnnDescriptor(cudnn, std::move(rnn_desc), std::move(rnn_plan),
                              num_layers, hidden_size, input_size, cell_size,
                              batch_size, input_mode, direction_mode, rnn_mode,
                              data_type, compute_type, algorithm_config,
                              std::move(dropout_desc), std::move(params_desc));
  }

  cudnnRNNDescriptor_t handle() const { return rnn_desc_.get(); }
  int num_layers() const { return num_layers_; }
  int hidden_size() const { return hidden_size_; }
  int input_size() const { return input_size_; }
  int cell_size() const { return cell_size_; }
  int batch_size() const { return batch_size_; }
  cudnnRNNInputMode_t input_mode() const { return input_mode_; }
  cudnnDirectionMode_t direction_mode() const { return direction_mode_; }
  cudnnRNNMode_t rnn_mode() const { return rnn_mode_; }
  cudnnDataType_t data_type() const { return data_type_; }
  cudnnDataType_t compute_type() const { return compute_type_; }
  const dnn::AlgorithmConfig& algorithm_config() const {
    return algorithm_config_;
  }
  int64_t ParamsSizeInBytes() const override {
    return params_desc_.params_size_in_bytes();
  }
  cudnnFilterDescriptor_t params_handle() const {
    return params_desc_.handle();
  }
  ParamsRegions ParamsWeightRegions() const override {
    return params_desc_.params_weights();
  }
  ParamsRegions ParamsBiasRegions() const override {
    return params_desc_.params_biases();
  }

 private:
  gpu::RnnDescriptor rnn_desc_;
  PersistentRnnPlan rnn_plan_;
  int num_layers_;
  int hidden_size_;
  int input_size_;
  // cell_size_ is the size of cell state, which will be different from
  // hidden_size_ if the projection is used.
  int cell_size_;
  // batch_size_ is set to -1 when not using CUDNN_RNN_ALGO_PERSIST_DYNAMIC
  // algorithm.
  int batch_size_;
  cudnnRNNAlgo_t rnn_algo_;
  cudnnRNNInputMode_t input_mode_;
  cudnnDirectionMode_t direction_mode_;
  cudnnRNNMode_t rnn_mode_;
  cudnnDataType_t data_type_;
  cudnnDataType_t compute_type_;
  dnn::AlgorithmConfig algorithm_config_;
  CudnnDropoutDescriptor dropout_desc_;
  CudnnRnnParamsDescriptor params_desc_;
  SE_DISALLOW_COPY_AND_ASSIGN(CudnnRnnDescriptor);
};

#if CUDNN_VERSION >= 7603
class CudnnCtcLossDescriptor {
 public:
  explicit CudnnCtcLossDescriptor(cudnnDataType_t data_type)
      : handle_(CreateCtcLossDescriptor()) {
    CHECK_CUDNN_OK(cudnnSetCTCLossDescriptorEx(
        /*ctcLossDesc=*/handle_.get(),
        /*compType=*/data_type,
        /*normMode=*/CUDNN_LOSS_NORMALIZATION_SOFTMAX,
        /*gradMode=*/CUDNN_NOT_PROPAGATE_NAN));
  }

  cudnnCTCLossDescriptor_t handle() const { return handle_.get(); }

 private:
  CtcLossDescriptor handle_;  // Owned

  SE_DISALLOW_COPY_AND_ASSIGN(CudnnCtcLossDescriptor);
};
#else
// dummy class
class CudnnCtcLossDescriptor {
 public:
  CudnnCtcLossDescriptor(cudnnDataType_t data_type) {}
};
#endif

namespace {

// Check if the LSTM projection is used. If yes, an additional weight matrix
// (projection matrix) will be fetched to the 'weights'. Otherwise, nothing will
// be done.
port::Status CheckAndFetchProjectionWeights(
    const CudnnHandle& cudnn, cudnnRNNDescriptor_t rnn_desc, const int layer,
    const TensorDescriptor& input_desc, const FilterDescriptor& filter_desc,
    const FilterDescriptor& region_desc_handle,
    dnn::RnnDescriptor::ParamsRegions* weights) {
  int hidden_size_v;
  int num_layers_v;
  cudnnDropoutDescriptor_t dropout_desc;
  cudnnRNNInputMode_t input_mode;
  cudnnDirectionMode_t direction;
  cudnnRNNMode_t mode;
  cudnnRNNAlgo_t algo;
  cudnnDataType_t data_type;
#if CUDNN_VERSION >= 8000
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNDescriptor_v6(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
      /*hiddenSize=*/&hidden_size_v,
      /*numLayers=*/&num_layers_v,
      /*dropoutDesc=*/&dropout_desc,
      /*inputMode=*/&input_mode,
      /*direction=*/&direction,
      /*mode=*/&mode,
      /*algo=*/&algo,
      /*mathPrec=*/&data_type));
#else
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNDescriptor(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
      /*hiddenSize=*/&hidden_size_v,
      /*numLayers=*/&num_layers_v,
      /*dropoutDesc=*/&dropout_desc,
      /*inputMode=*/&input_mode,
      /*direction=*/&direction,
      /*mode=*/&mode,
      /*algo=*/&algo,
      /*mathPrec=*/&data_type));
#endif
  int rec_proj_size_v;
  int out_proj_size_v;
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNProjectionLayers(
      /*handle=*/cudnn.handle(),
      /*rnnDesc=*/rnn_desc,
      /*recProjSize*/ &rec_proj_size_v,
      /*outProjSize*/ &out_proj_size_v));
  if (rec_proj_size_v != hidden_size_v) {
    void* offset = nullptr;
    int region_id = 8;
    RETURN_IF_CUDNN_ERROR(cudnnGetRNNLinLayerMatrixParams(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
        /*layer=*/layer, /*xDesc=*/input_desc.get(),
        /*wDesc=*/filter_desc.get(),
        /*w=*/nullptr, /*linLayerID=*/region_id,
        /*linLayerMatDesc=*/region_desc_handle.get(),
        /*linLayerMat or linLayerBias=*/&offset));
    int dims[] = {1, 1, 1};
    cudnnDataType_t data_type;
    cudnnTensorFormat_t tensor_format;
    int n_dims;
    RETURN_IF_CUDNN_ERROR(cudnnGetFilterNdDescriptor(
        /*filterDesc=*/region_desc_handle.get(),
        /*nbDimsRequested=*/sizeof(dims) / sizeof(dims[0]),
        /*dataType=*/&data_type, /*format=*/&tensor_format,
        /*nbDims=*/&n_dims, /*filterDimA=*/dims));
    int64_t size =
        dims[0] * dims[1] * dims[2] * CudnnDataTypeToByteSize(data_type);
    dnn::RnnDescriptor::ParamsRegion region = {
        reinterpret_cast<int64_t>(offset), size};
    weights->push_back(region);
  }
  return port::Status::OK();
}

port::StatusOr<CudnnRnnParamsDescriptor> CudnnRnnParamsDescriptor::Create(
    const CudnnHandle& cudnn, int input_size, cudnnDataType_t data_type,
    cudnnRNNDescriptor_t rnn_desc, cudnnRNNMode_t rnn_mode,
    cudnnDirectionMode_t direction_mode, int num_layers) {
  // Query the params size.
  TensorDescriptor input_desc = CreateTensorDescriptor();
  int tensor_dims[] = {1, input_size, 1};
  int strides[] = {tensor_dims[1] * tensor_dims[2], tensor_dims[2], 1};
  RETURN_IF_CUDNN_ERROR(cudnnSetTensorNdDescriptor(
      /*tensorDesc=*/input_desc.get(), /*dataType=*/data_type,
      /*nbDims=*/sizeof(tensor_dims) / sizeof(tensor_dims[0]),
      /*dimA=*/tensor_dims,
      /*strideA=*/strides));

  size_t params_size = 0;
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNParamsSize(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
      /*xDesc=*/input_desc.get(), /*sizeInBytes=*/&params_size,
      /*dataType=*/data_type));
  int64_t params_size_in_bytes = static_cast<int64_t>(params_size);

  FilterDescriptor filter_desc = CreateFilterDescriptor();
  int64_t filter_dim0 =
      params_size_in_bytes / CudnnDataTypeToByteSize(data_type);
  int filter_dims[] = {static_cast<int>(filter_dim0), 1, 1};
  RETURN_IF_CUDNN_ERROR(cudnnSetFilterNdDescriptor(
      /*filterDesc=*/filter_desc.get(), /*dataType=*/data_type,
      /*format=*/CUDNN_TENSOR_NCHW,
      /*nbDims=*/sizeof(filter_dims) / sizeof(filter_dims[0]),
      /*filterDimA=*/filter_dims));

  // Create the weights and biases into the params buffer
  int region_count_per_layer = [&] {
    switch (rnn_mode) {
      case CUDNN_RNN_RELU:
      case CUDNN_RNN_TANH:
        return 2;
      case CUDNN_LSTM:
        return 8;
      case CUDNN_GRU:
        return 6;
      default:
        LOG(FATAL) << "Invalid RNN Mode: " << static_cast<int>(rnn_mode);
        return 0;
    }
  }();

  FilterDescriptor region_desc_handle = CreateFilterDescriptor();
  const int layer_count =
      direction_mode == CUDNN_UNIDIRECTIONAL ? num_layers : 2 * num_layers;

  ParamsRegions weights;
  ParamsRegions biases;

  for (int layer = 0; layer < layer_count; layer++) {
    for (int region = 0; region < region_count_per_layer; region++) {
      for (int type = 0; type < 2; type++) {
        void* offset = nullptr;
        RETURN_IF_CUDNN_ERROR(
            type == 0 ? cudnnGetRNNLinLayerMatrixParams(
                            /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
                            /*layer=*/layer, /*xDesc=*/input_desc.get(),
                            /*wDesc=*/filter_desc.get(),
                            /*w=*/nullptr, /*linLayerID=*/region,
                            /*linLayerMatDesc=*/region_desc_handle.get(),
                            /*linLayerMat or linLayerBias=*/&offset)
                      : cudnnGetRNNLinLayerBiasParams(
                            /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc,
                            /*layer=*/layer, /*xDesc=*/input_desc.get(),
                            /*wDesc=*/filter_desc.get(),
                            /*w=*/nullptr, /*linLayerID=*/region,
                            /*linLayerMatDesc=*/region_desc_handle.get(),
                            /*linLayerMat or linLayerBias=*/&offset));
        int dims[] = {1, 1, 1};
        cudnnDataType_t data_type;
        cudnnTensorFormat_t tensor_format;
        int n_dims;
        RETURN_IF_CUDNN_ERROR(cudnnGetFilterNdDescriptor(
            /*filterDesc=*/region_desc_handle.get(),
            /*nbDimsRequested=*/sizeof(dims) / sizeof(dims[0]),
            /*dataType=*/&data_type, /*format=*/&tensor_format,
            /*nbDims=*/&n_dims, /*filterDimA=*/dims));
        int64_t size =
            dims[0] * dims[1] * dims[2] * CudnnDataTypeToByteSize(data_type);
        dnn::RnnDescriptor::ParamsRegion region = {
            reinterpret_cast<int64_t>(offset), size};
        (type == 0 ? weights : biases).push_back(region);
      }
    }
    TF_RETURN_IF_ERROR(CheckAndFetchProjectionWeights(
        cudnn, rnn_desc, layer, input_desc, filter_desc, region_desc_handle,
        &weights));
  }

  return CudnnRnnParamsDescriptor(std::move(filter_desc), params_size_in_bytes,
                                  weights, biases);
}

}  // namespace

class CudnnRnnSequenceTensorDescriptor
    : public dnn::RnnSequenceTensorDescriptor {
  CudnnRnnSequenceTensorDescriptor(GpuExecutor* parent, int max_seq_length,
                                   int batch_size, int data_size,
                                   cudnnDataType_t data_type,
                                   RNNDataDescriptor data_handle,
                                   TensorDescriptor handle)
      : max_seq_length_(max_seq_length),
        batch_size_(batch_size),
        data_size_(data_size),
        data_type_(data_type),
        handle_(std::move(handle)),
        rnn_data_handle_(std::move(data_handle)),
        handles_(max_seq_length, handle_.get()) {}

 public:
  CudnnRnnSequenceTensorDescriptor(CudnnRnnSequenceTensorDescriptor&&) =
      default;

  static port::StatusOr<CudnnRnnSequenceTensorDescriptor> Create(
      GpuExecutor* parent, int max_seq_length, int batch_size, int data_size,
      cudnnDataType_t data_type) {
    if (max_seq_length <= 0) {
      return port::Status(port::error::INVALID_ARGUMENT, "max_seq_length <= 0");
    }
    int dims[] = {batch_size, data_size, 1};
    int strides[] = {dims[1] * dims[2], dims[2], 1};
    TensorDescriptor tensor_desc = CreateTensorDescriptor();
    RETURN_IF_CUDNN_ERROR(cudnnSetTensorNdDescriptor(
        /*tensorDesc=*/tensor_desc.get(), /*dataType=*/data_type,
        /*nbDims=*/sizeof(dims) / sizeof(dims[0]), /*dimA=*/dims,
        /*strideA=*/strides));
    return CudnnRnnSequenceTensorDescriptor(parent, max_seq_length, batch_size,
                                            data_size, data_type, nullptr,
                                            std::move(tensor_desc));
  }

  static port::StatusOr<CudnnRnnSequenceTensorDescriptor> Create(
      GpuExecutor* parent, int max_seq_length, int batch_size, int data_size,
      const absl::Span<const int>& seq_lengths, bool time_major,
      cudnnDataType_t data_type) {
    if (max_seq_length <= 0) {
      return port::Status(port::error::INVALID_ARGUMENT, "max_seq_length <= 0");
    }
    int dims[] = {batch_size, data_size, 1};
    int strides[] = {dims[1] * dims[2], dims[2], 1};
    TensorDescriptor tensor_desc = CreateTensorDescriptor();
    RETURN_IF_CUDNN_ERROR(cudnnSetTensorNdDescriptor(
        /*tensorDesc=*/tensor_desc.get(), /*dataType=*/data_type,
        /*nbDims=*/sizeof(dims) / sizeof(dims[0]), /*dimA=*/dims,
        /*strideA=*/strides));
    const int* seq_lengths_array = seq_lengths.data();
    RNNDataDescriptor data_desc = CreateRNNDataDescriptor();
    float padding_fill = 0.0f;
    cudnnRNNDataLayout_t layout;
    if (time_major) {
      layout = CUDNN_RNN_DATA_LAYOUT_SEQ_MAJOR_UNPACKED;
    } else {
      layout = CUDNN_RNN_DATA_LAYOUT_BATCH_MAJOR_UNPACKED;
    }
    RETURN_IF_CUDNN_ERROR(cudnnSetRNNDataDescriptor(
        /*RNNDataDesc=*/data_desc.get(), /*dataType*/ data_type,
        /*layout=*/layout,
        /*maxSeqLength=*/max_seq_length,
        /*batchSize=*/batch_size, /*vectorSize=*/data_size,
        /*seqLengthArray=*/seq_lengths_array,
        /*paddingFill*/ (void*)&padding_fill));
    return CudnnRnnSequenceTensorDescriptor(
        parent, max_seq_length, batch_size, data_size, data_type,
        std::move(data_desc), std::move(tensor_desc));
  }

  const cudnnTensorDescriptor_t* handles() const { return handles_.data(); }
  const cudnnRNNDataDescriptor_t data_handle() const {
    return rnn_data_handle_.get();
  }

  int max_seq_length() const { return max_seq_length_; }
  int batch_size() const { return batch_size_; }
  int data_size() const { return data_size_; }
  bool is_var_seq_lengths() const { return rnn_data_handle_ != nullptr; }

 private:
  int max_seq_length_;
  int batch_size_;
  int data_size_;
  cudnnDataType_t data_type_;
  TensorDescriptor handle_;
  RNNDataDescriptor rnn_data_handle_;
  std::vector<cudnnTensorDescriptor_t> handles_;  // Copies of handle_.
  SE_DISALLOW_COPY_AND_ASSIGN(CudnnRnnSequenceTensorDescriptor);
};

class CudnnRnnStateTensorDescriptor : public dnn::RnnStateTensorDescriptor {
 public:
  CudnnRnnStateTensorDescriptor(GpuExecutor* parent, int num_layers,
                                int batch_size, int data_size,
                                cudnnDataType_t data_type)
      : handle_(CreateTensorDescriptor()),
        num_layers_(num_layers),
        batch_size_(batch_size),
        data_size_(data_size),
        data_type_(data_type) {
    int dims[] = {num_layers, batch_size, data_size};
    int strides[] = {dims[1] * dims[2], dims[2], 1};
    CHECK_CUDNN_OK(cudnnSetTensorNdDescriptor(
        /*tensorDesc=*/handle_.get(), /*dataType=*/data_type,
        /*nbDims=*/sizeof(dims) / sizeof(dims[0]), /*dimA=*/dims,
        /*strideA=*/strides));
  }

  cudnnTensorDescriptor_t handle() const { return handle_.get(); }

  int num_layers() const { return num_layers_; }
  int batch_size() const { return batch_size_; }
  int data_size() const { return data_size_; }

 private:
  TensorDescriptor handle_;
  int num_layers_;
  int batch_size_;
  int data_size_;
  cudnnDataType_t data_type_;
  SE_DISALLOW_COPY_AND_ASSIGN(CudnnRnnStateTensorDescriptor);
};

namespace {

struct RnnModelDims {
  int num_layers = 0;
  int batch_size = 0;
  int max_seq_length = 0;
  int hidden_size = 0;
  int input_size = 0;
  int cell_size = 0;
  int dir_count = 0;
};

template <class T>
port::StatusOr<RnnModelDims> ExtractAndCheckRnnForward(
    const CudnnRnnDescriptor& rnn_desc,
    const CudnnRnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<T>& input_data,
    const CudnnRnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<T>& input_h_data,
    const CudnnRnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<T>& input_c_data, const DeviceMemory<T>& params,
    const CudnnRnnSequenceTensorDescriptor& output_desc,
    const DeviceMemory<T>& output_data,
    const CudnnRnnStateTensorDescriptor& output_h_desc,
    const DeviceMemory<T>& output_h_data,
    const CudnnRnnStateTensorDescriptor& output_c_desc,
    const DeviceMemory<T>& output_c_data) {
  // extract model parameters
  RnnModelDims model_dims;
  model_dims.num_layers = rnn_desc.num_layers();
  model_dims.batch_size = input_desc.batch_size();
  model_dims.max_seq_length = input_desc.max_seq_length();
  model_dims.hidden_size = rnn_desc.hidden_size();
  model_dims.input_size = input_desc.data_size();
  model_dims.cell_size = rnn_desc.cell_size();
  model_dims.dir_count =
      (rnn_desc.direction_mode() == CUDNN_BIDIRECTIONAL) ? 2 : 1;

  // check parameters
  if (!(input_h_desc.num_layers() ==
            model_dims.num_layers * model_dims.dir_count &&
        input_h_desc.batch_size() == model_dims.batch_size &&
        input_h_desc.data_size() == model_dims.hidden_size)) {
    return port::Status(port::error::INVALID_ARGUMENT, "Invalid input_h shape");
  }
  // The LSTM projection will be used if input_h_desc.data_size() <
  // input_c_desc.data_size()
  if (!(input_h_desc.num_layers() == input_c_desc.num_layers() &&
        input_h_desc.batch_size() == input_c_desc.batch_size() &&
        input_h_desc.data_size() <= input_c_desc.data_size())) {
    return port::Status(port::error::INVALID_ARGUMENT, "Invalid input_c shape");
  }
  if (!(output_desc.max_seq_length() == model_dims.max_seq_length &&
        output_desc.batch_size() == model_dims.batch_size &&
        output_desc.data_size() ==
            model_dims.hidden_size * model_dims.dir_count)) {
    return port::Status(port::error::INVALID_ARGUMENT, "Invalid output shape");
  }
  if (!(input_h_desc.num_layers() == output_h_desc.num_layers() &&
        input_h_desc.batch_size() == output_h_desc.batch_size() &&
        input_h_desc.data_size() == output_h_desc.data_size())) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "Invalid output_h shape");
  }
  if (!(input_h_desc.num_layers() == output_c_desc.num_layers() &&
        input_h_desc.batch_size() == output_c_desc.batch_size() &&
        input_h_desc.data_size() <= output_c_desc.data_size())) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "Invalid output_c shape");
  }

  return model_dims;
}

port::Status CheckRNNParameterSize(
    const CudnnHandle& cudnn, const CudnnRnnDescriptor& rnn_desc,
    const CudnnRnnSequenceTensorDescriptor& input_desc) {
  size_t params_size_in_bytes = 0;
#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNWeightSpaceSize(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
      /*sizeInBytes=*/&params_size_in_bytes));
#else
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNParamsSize(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
      /*xDesc=*/input_desc.handles()[0], /*sizeInBytes=*/&params_size_in_bytes,
      /*dataType=*/rnn_desc.data_type()));
#endif
  if (static_cast<int64_t>(params_size_in_bytes) !=
      rnn_desc.ParamsSizeInBytes()) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "Mismatching RNN parameter size");
  }
  return port::Status::OK();
}

port::StatusOr<DeviceMemory<uint8>> CreateRnnWorkspace(
    Stream* stream, const CudnnHandle& cudnn,
    const CudnnRnnDescriptor& rnn_desc,
    const CudnnRnnSequenceTensorDescriptor& input_desc,
    ScratchAllocator* workspace_allocator) {
  // Query the workspace size.
  size_t workspace_size_in_bytes = 0;
  RETURN_IF_CUDNN_ERROR(cudnnGetRNNWorkspaceSize(
      /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
      /*seqLength=*/input_desc.max_seq_length(), /*xDesc=*/input_desc.handles(),
      /*sizeInBytes=*/&workspace_size_in_bytes));
  // Allocate the workspace.
  if (workspace_size_in_bytes == 0) {
    return DeviceMemory<uint8>();
  }
  return workspace_allocator->AllocateBytes(workspace_size_in_bytes);
}

#if CUDNN_VERSION >= 7402
port::StatusOr<DeviceMemory<uint8>> CreateBatchNormForwardWorkspace(
    Stream* stream, const CudnnHandle& cudnn, const cudnnBatchNormMode_t& mode,
    const cudnnBatchNormOps_t& bn_ops,
    const cudnnActivationDescriptor_t& activation_desc,
    const CudnnTensorDescriptor& x_descriptor,
    const CudnnTensorDescriptor& scale_offset_descriptor,
    ScratchAllocator* workspace_allocator) {
  // Query the workspace size.
  size_t workspace_size_in_bytes = 0;
  RETURN_IF_CUDNN_ERROR(
      cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize(
          /*handle=*/cudnn.handle(), /*mode=*/mode, /*bnOps=*/bn_ops,
          /*xDesc=*/x_descriptor.handle(), /*zDesc=*/x_descriptor.handle(),
          /*yDesc=*/x_descriptor.handle(),
          /*bnScaleBiasMeanVarDesc=*/scale_offset_descriptor.handle(),
          /*activationDesc=*/activation_desc,
          /*sizeInBytes=*/&workspace_size_in_bytes));
  // Allocate the workspace.
  if (workspace_size_in_bytes == 0) {
    return DeviceMemory<uint8>();
  }
  return workspace_allocator->AllocateBytes(workspace_size_in_bytes);
}

port::StatusOr<DeviceMemory<uint8>> CreateBatchNormBackwardWorkspace(
    Stream* stream, const CudnnHandle& cudnn, const cudnnBatchNormMode_t& mode,
    const cudnnBatchNormOps_t& bn_ops,
    const cudnnActivationDescriptor_t& activation_desc,
    const CudnnTensorDescriptor& x_descriptor,
    const CudnnTensorDescriptor& scale_offset_descriptor,
    ScratchAllocator* workspace_allocator) {
  // Query the workspace size.
  size_t workspace_size_in_bytes = 0;
  RETURN_IF_CUDNN_ERROR(cudnnGetBatchNormalizationBackwardExWorkspaceSize(
      /*handle=*/cudnn.handle(), /*mode=*/mode, /*bnOps=*/bn_ops,
      /*xDesc=*/x_descriptor.handle(),
      /*yDesc=*/x_descriptor.handle(),
      /*dyDesc=*/x_descriptor.handle(),
      /*dzDesc=*/x_descriptor.handle(),
      /*dxDesc=*/x_descriptor.handle(),
      /*dBnScaleBiasDesc=*/scale_offset_descriptor.handle(),
      /*activationDesc=*/activation_desc,
      /*sizeInBytes=*/&workspace_size_in_bytes));
  // Allocate the workspace.
  if (workspace_size_in_bytes == 0) {
    return DeviceMemory<uint8>();
  }
  return workspace_allocator->AllocateBytes(workspace_size_in_bytes);
}

#endif

}  // namespace

template <class T>
port::Status CudnnSupport::DoRnnForwardImpl(
    Stream* stream, const CudnnRnnDescriptor& rnn_desc,
    const CudnnRnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<T>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const CudnnRnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<T>& input_h_data,
    const CudnnRnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<T>& input_c_data, const DeviceMemory<T>& params,
    const CudnnRnnSequenceTensorDescriptor& output_desc,
    DeviceMemory<T>* output_data,
    const CudnnRnnStateTensorDescriptor& output_h_desc,
    DeviceMemory<T>* output_h_data,
    const CudnnRnnStateTensorDescriptor& output_c_desc,
    DeviceMemory<T>* output_c_data, bool is_training,
    ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  SE_ASSIGN_OR_RETURN(
      RnnModelDims model_dims,
      ExtractAndCheckRnnForward(
          rnn_desc, input_desc, input_data, input_h_desc, input_h_data,
          input_c_desc, input_c_data, params, output_desc, *output_data,
          output_h_desc, *output_h_data, output_c_desc, *output_c_data));

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  SE_RETURN_IF_ERROR(CheckRNNParameterSize(cudnn, rnn_desc, input_desc));

  // In CUDNN v8.0, the cudnnRNNForward*** and cudnnRNNForward***Ex have been
  // deprecated. Instead, we use the cudnnRNNForward which requires the
  // sequence_lengths parameter. For more info,
  // https://docs.nvidia.com/deeplearning/cudnn/api/index.html#release-802.
#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  if (input_desc.is_var_seq_lengths()) {
    DeviceMemory<uint8> workspace;
    DeviceMemory<uint8> reserve_space;
    cudnnForwardMode_t rnn_fwd_mode;
    if (is_training) {
      rnn_fwd_mode = CUDNN_FWD_MODE_TRAINING;
    } else {
      rnn_fwd_mode = CUDNN_FWD_MODE_INFERENCE;
    }
    size_t reserve_space_size_in_bytes = 0;
    size_t workspace_size_in_bytes = 0;
    RETURN_IF_CUDNN_ERROR(cudnnGetRNNTempSpaceSizes(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*fMode=*/rnn_fwd_mode, /*xDesc=*/input_desc.data_handle(),
        /*workSpaceSize=*/&workspace_size_in_bytes,
        /*reserveSpaceSize=*/&reserve_space_size_in_bytes));

    if (workspace_size_in_bytes > 0) {
      SE_ASSIGN_OR_RETURN(workspace, workspace_allocator->AllocateBytes(
                                         workspace_size_in_bytes));
    }
    if (reserve_space_size_in_bytes > 0) {
      SE_ASSIGN_OR_RETURN(reserve_space, reserve_space_allocator->AllocateBytes(
                                             reserve_space_size_in_bytes));
    }

    std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
    const bool is_profiling = output_profile_result != nullptr;
    if (is_profiling) {
      timer.reset(new GpuTimer(parent_));
      // The start and stop of the timer should be as close to the Cudnn call as
      // possible. It is still possible for other threads to issue workload on
      // to this stream. So it could take multiple profiling measurements.
      if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to start timer");
      }
    }

    RETURN_IF_CUDNN_ERROR(cudnnRNNForward(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*fwdMode=*/rnn_fwd_mode,
        /*devSeqLengths=*/
        reinterpret_cast<const int*>(seq_lengths_data.opaque()),
        /*xDesc=*/input_desc.data_handle(), /*x=*/input_data.opaque(),
        /*yDesc=*/output_desc.data_handle(), /*y=*/output_data->opaque(),
        /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
        /*hy=*/output_h_data->opaque(),
        /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
        /*cy=*/output_c_data->opaque(),
        /*weightSpaceSize=*/rnn_desc.ParamsSizeInBytes(),
        /*weightSpace=*/params.opaque(),
        /*workSpaceSize=*/workspace.size(), /*workspace=*/workspace.opaque(),
        /*reserveSpaceSizeInBytes=*/reserve_space.size(),
        /*reserveSpace=*/reserve_space.opaque()));

    if (is_profiling) {
      if (!timer->Stop(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to stop timer");
      }
      auto algo_desc = *rnn_desc.algorithm_config().algorithm();
      output_profile_result->set_algorithm(algo_desc);
      output_profile_result->set_elapsed_time_in_ms(
          timer->GetElapsedMilliseconds());
    }
    return port::Status::OK();
  }
#endif
  SE_ASSIGN_OR_RETURN(DeviceMemory<uint8> workspace,
                      CreateRnnWorkspace(stream, cudnn, rnn_desc, input_desc,
                                         workspace_allocator))

  // query the reserve space size
  // allocate the reserve space
  DeviceMemory<uint8> reserve_space;
  if (is_training) {
    size_t reserve_space_size_in_bytes = 0;
    RETURN_IF_CUDNN_ERROR(cudnnGetRNNTrainingReserveSize(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*seqLength=*/model_dims.max_seq_length, /*xDesc=*/input_desc.handles(),
        /*sizeInBytes=*/&reserve_space_size_in_bytes));

    if (reserve_space_size_in_bytes > 0) {
      SE_ASSIGN_OR_RETURN(reserve_space, reserve_space_allocator->AllocateBytes(
                                             reserve_space_size_in_bytes));
    }
  }

  std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
  const bool is_profiling = output_profile_result != nullptr;
  if (is_profiling) {
    timer.reset(new GpuTimer(parent_));
    // The start and stop of the timer should be as close to the Cudnn call as
    // possible. It is still possible for other threads to issue workload on
    // to this stream. So it could take multiple profiling measurements.
    if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
      return port::Status(port::error::INTERNAL, "Failed to start timer");
    }
  }

  if (!is_training) {
    if (input_desc.is_var_seq_lengths()) {
      RETURN_IF_CUDNN_ERROR(cudnnRNNForwardInferenceEx(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*xDesc=*/input_desc.data_handle(), /*x=*/input_data.opaque(),
          /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
          /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
          /*wDesc=*/rnn_desc.params_handle(), /*w=*/params.opaque(),
          /*yDesc=*/output_desc.data_handle(),
          /*y=*/output_data->opaque(),
          /*hyDesc=*/output_h_desc.handle(), /*hy=*/output_h_data->opaque(),
          /*cyDesc=*/output_c_desc.handle(), /*cy=*/output_c_data->opaque(),
          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr,
          /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size()));
    } else {
      RETURN_IF_CUDNN_ERROR(cudnnRNNForwardInference(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*seqLength=*/model_dims.max_seq_length,
          /*xDesc=*/input_desc.handles(),
          /*x=*/input_data.opaque(), /*hxDesc=*/input_h_desc.handle(),
          /*hx=*/input_h_data.opaque(), /*cxDesc=*/input_c_desc.handle(),
          /*cx=*/input_c_data.opaque(), /*wDesc=*/rnn_desc.params_handle(),
          /*w=*/params.opaque(), /*yDesc=*/output_desc.handles(),
          /*y=*/output_data->opaque(), /*hyDesc=*/output_h_desc.handle(),
          /*hy=*/output_h_data->opaque(), /*cyDesc=*/output_c_desc.handle(),
          /*cy=*/output_c_data->opaque(), /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size()));
    }
  } else {
    if (input_desc.is_var_seq_lengths()) {
      RETURN_IF_CUDNN_ERROR(cudnnRNNForwardTrainingEx(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*xDesc=*/input_desc.data_handle(), /*x=*/input_data.opaque(),
          /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
          /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
          /*wDesc=*/rnn_desc.params_handle(), /*w=*/params.opaque(),
          /*yDesc=*/output_desc.data_handle(),
          /*y=*/output_data->opaque(),
          /*hyDesc=*/output_h_desc.handle(), /*hy=*/output_h_data->opaque(),
          /*cyDesc=*/output_c_desc.handle(), /*cy=*/output_c_data->opaque(),
          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr,
          /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size(),
          /*reserveSpace=*/reserve_space.opaque(),
          /*reserveSpaceSizeInBytes=*/reserve_space.size()));
    } else {
      RETURN_IF_CUDNN_ERROR(cudnnRNNForwardTraining(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*seqLength=*/model_dims.max_seq_length,
          /*xDesc=*/input_desc.handles(),
          /*x=*/input_data.opaque(), /*hxDesc=*/input_h_desc.handle(),
          /*hx=*/input_h_data.opaque(), /*cxDesc=*/input_c_desc.handle(),
          /*cx=*/input_c_data.opaque(), /*wDesc=*/rnn_desc.params_handle(),
          /*w=*/params.opaque(), /*yDesc=*/output_desc.handles(),
          /*y=*/output_data->opaque(), /*hyDesc=*/output_h_desc.handle(),
          /*hy=*/output_h_data->opaque(), /*cyDesc=*/output_c_desc.handle(),
          /*cy=*/output_c_data->opaque(), /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size(),
          /*reserveSpace=*/reserve_space.opaque(),
          /*reserveSpaceSizeInBytes=*/reserve_space.size()));
    }
  }

  if (is_profiling) {
    if (!timer->Stop(AsGpuStream(stream))) {
      return port::Status(port::error::INTERNAL, "Failed to stop timer");
    }
    auto algo_desc = *rnn_desc.algorithm_config().algorithm();
    output_profile_result->set_algorithm(algo_desc);
    output_profile_result->set_elapsed_time_in_ms(
        timer->GetElapsedMilliseconds());
  }

  return port::Status::OK();
}

template <class T>
port::Status CudnnSupport::DoRnnBackwardImpl(
    Stream* stream, const CudnnRnnDescriptor& rnn_desc,
    const CudnnRnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<T>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const CudnnRnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<T>& input_h_data,
    const CudnnRnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<T>& input_c_data, const DeviceMemory<T>& params,
    const CudnnRnnSequenceTensorDescriptor& output_desc,
    const DeviceMemory<T>& output_data,
    const CudnnRnnStateTensorDescriptor& output_h_desc,
    const DeviceMemory<T>& output_h_data,
    const CudnnRnnStateTensorDescriptor& output_c_desc,
    const DeviceMemory<T>& output_c_data,
    const DeviceMemory<T>& output_backprop_data,
    const DeviceMemory<T>& output_h_backprop_data,
    const DeviceMemory<T>& output_c_backprop_data,
    DeviceMemory<T>* input_backprop_data,
    DeviceMemory<T>* input_h_backprop_data,
    DeviceMemory<T>* input_c_backprop_data,
    DeviceMemory<T>* params_backprop_data,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  SE_ASSIGN_OR_RETURN(
      RnnModelDims model_dims,
      ExtractAndCheckRnnForward(rnn_desc, input_desc, input_data, input_h_desc,
                                input_h_data, input_c_desc, input_c_data,
                                params, output_desc, output_data, output_h_desc,
                                output_h_data, output_c_desc, output_c_data));

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  SE_RETURN_IF_ERROR(CheckRNNParameterSize(cudnn, rnn_desc, input_desc));

  // In CUDNN v8.0, the cudnnRNNForward*** and cudnnRNNForward***Ex have been
  // deprecated. Instead, we use the cudnnRNNForward which requires the
  // sequence_lengths parameter. For more info,
  // https://docs.nvidia.com/deeplearning/cudnn/api/index.html#release-802.
#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  if (input_desc.is_var_seq_lengths()) {
    DeviceMemory<uint8> workspace;
    size_t workspace_size_in_bytes = 0;
    RETURN_IF_CUDNN_ERROR(cudnnGetRNNTempSpaceSizes(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*fMode=*/CUDNN_FWD_MODE_TRAINING, /*xDesc=*/input_desc.data_handle(),
        /*workSpaceSize=*/&workspace_size_in_bytes,
        /*reserveSpaceSize=*/NULL));
    if (workspace_size_in_bytes > 0) {
      SE_ASSIGN_OR_RETURN(workspace, workspace_allocator->AllocateBytes(
                                         workspace_size_in_bytes));
    }

    std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
    const bool is_profiling = output_profile_result != nullptr;
    if (is_profiling) {
      timer.reset(new GpuTimer(parent_));
      // The start and stop of the timer should be as close to the Cudnn call as
      // possible. It is still possible for other threads to issue workload on
      // to this stream. So it could take multiple profiling measurements.
      if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to start timer");
      }
    }

    RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardData_v8(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*devSeqLengths=*/
        reinterpret_cast<const int*>(seq_lengths_data.opaque()),
        /*yDesc=*/output_desc.data_handle(), /*y=*/output_data.opaque(),
        /*dy=*/output_backprop_data.opaque(),
        /*xDesc=*/input_desc.data_handle(),
        /*dx=*/input_backprop_data->opaque(),
        /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
        /*dhy=*/output_h_backprop_data.opaque(),
        /*dhx=*/input_h_backprop_data->opaque(),
        /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
        /*dcy=*/output_c_backprop_data.opaque(),
        /*dcx=*/input_c_backprop_data->opaque(),
        /*weightSpaceSize=*/rnn_desc.ParamsSizeInBytes(),
        /*weightSpace=*/params.opaque(),
        /*workSpaceSize=*/workspace.size(), /*workSpace=*/workspace.opaque(),
        /*reserveSpaceSize=*/reserve_space_data->size(),
        /*reserveSpace=*/reserve_space_data->opaque()));

    if (params_backprop_data != nullptr) {
      // Clear the dw to zeros.
      stream->ThenMemZero(params_backprop_data, params_backprop_data->size());
      RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardWeights_v8(
          /*handle=*/cudnn.handle(),
          /*rnnDesc=*/rnn_desc.handle(),
          /*addGrad=*/CUDNN_WGRAD_MODE_ADD,
          /*devSeqLengths=*/
          reinterpret_cast<const int*>(seq_lengths_data.opaque()),
          /*xDesc=*/input_desc.data_handle(),
          /*x=*/input_data.opaque(),
          /*hDesc=*/input_h_desc.handle(),
          /*hx=*/input_h_data.opaque(),
          /*yDesc=*/output_desc.data_handle(),
          /*y=*/output_data.opaque(),
          /*weightSpaceSize=*/rnn_desc.ParamsSizeInBytes(),
          /*dweightSpace=*/params_backprop_data->opaque(),
          /*workSpaceSize=*/workspace.size(),
          /*workSpace=*/workspace.opaque(),
          /*reserveSpaceSize=*/reserve_space_data->size(),
          /*reserveSpace=*/reserve_space_data->opaque()));
    }

    if (is_profiling) {
      if (!timer->Stop(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to stop timer");
      }
      auto algo_desc = *rnn_desc.algorithm_config().algorithm();
      output_profile_result->set_algorithm(algo_desc);
      output_profile_result->set_elapsed_time_in_ms(
          timer->GetElapsedMilliseconds());
    }
    return port::Status::OK();
  }
#endif
  SE_ASSIGN_OR_RETURN(DeviceMemory<uint8> workspace,
                      CreateRnnWorkspace(stream, cudnn, rnn_desc, input_desc,
                                         workspace_allocator));

  std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
  const bool is_profiling = output_profile_result != nullptr;
  if (is_profiling) {
    timer.reset(new GpuTimer(parent_));
    // The start and stop of the timer should be as close to the Cudnn call as
    // possible. It is still possible for other threads to issue workload on
    // to this stream. So it could take multiple profiling measurements.
    if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
      return port::Status(port::error::INTERNAL, "Failed to start timer");
    }
  }

  if (input_desc.is_var_seq_lengths()) {
    RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardDataEx(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*yDesc=*/output_desc.data_handle(), /*y=*/output_data.opaque(),
        /*dyDesc=*/output_desc.data_handle(),
        /*dy=*/output_backprop_data.opaque(), nullptr, nullptr,
        /*dhyDesc=*/output_h_desc.handle(),
        /*dhy=*/output_h_backprop_data.opaque(),
        /*dcyDesc=*/output_c_desc.handle(),
        /*dcy=*/output_c_backprop_data.opaque(),
        /*wDesc=*/rnn_desc.params_handle(), /*w=*/params.opaque(),
        /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
        /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
        /*dxDesc=*/input_desc.data_handle(),
        /*dx=*/input_backprop_data->opaque(),
        /*dhxDesc=*/input_h_desc.handle(),
        /*dhx=*/input_h_backprop_data->opaque(),
        /*dcxDesc=*/input_c_desc.handle(),
        /*dcx=*/input_c_backprop_data->opaque(), nullptr, nullptr,
        /*workspace=*/workspace.opaque(),
        /*workSpaceSizeInBytes=*/workspace.size(),
        /*reserveSpace=*/reserve_space_data->opaque(),
        /*reserveSpaceSizeInBytes=*/reserve_space_data->size()));
  } else {
    RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardData(
        /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
        /*seqLength=*/model_dims.max_seq_length,
        /*yDesc=*/output_desc.handles(),
        /*y=*/output_data.opaque(), /*dyDesc=*/output_desc.handles(),
        /*dy=*/output_backprop_data.opaque(),
        /*dhyDesc=*/output_h_desc.handle(),
        /*dhy=*/output_h_backprop_data.opaque(),
        /*dcyDesc=*/output_c_desc.handle(),
        /*dcy=*/output_c_backprop_data.opaque(),
        /*wDesc=*/rnn_desc.params_handle(), /*w=*/params.opaque(),
        /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
        /*cxDesc=*/input_c_desc.handle(), /*cx=*/input_c_data.opaque(),
        /*dxDesc=*/input_desc.handles(), /*dx=*/input_backprop_data->opaque(),
        /*dhxDesc=*/input_h_desc.handle(),
        /*dhx=*/input_h_backprop_data->opaque(),
        /*dcxDesc=*/input_c_desc.handle(),
        /*dcx=*/input_c_backprop_data->opaque(),
        /*workspace=*/workspace.opaque(),
        /*workSpaceSizeInBytes=*/workspace.size(),
        /*reserveSpace=*/reserve_space_data->opaque(),
        /*reserveSpaceSizeInBytes=*/reserve_space_data->size()));
  }

  if (params_backprop_data != nullptr) {
    // Clear the dw to zeros.
    stream->ThenMemZero(params_backprop_data, params_backprop_data->size());
    if (input_desc.is_var_seq_lengths()) {
      RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardWeightsEx(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*xDesc=*/input_desc.data_handle(), /*x=*/input_data.opaque(),
          /*hxDesc=*/input_h_desc.handle(), /*hx=*/input_h_data.opaque(),
          /*yDesc=*/output_desc.data_handle(),
          /*y=*/output_data.opaque(),
          /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size(),
          /*dwDesc=*/rnn_desc.params_handle(),
          /*dw=*/params_backprop_data->opaque(),
          /*reserveSpace=*/reserve_space_data->opaque(),
          /*reserveSpaceSizeInBytes=*/reserve_space_data->size()));
    } else {
      // make the backward weight call
      RETURN_IF_CUDNN_ERROR(cudnnRNNBackwardWeights(
          /*handle=*/cudnn.handle(), /*rnnDesc=*/rnn_desc.handle(),
          /*seqLength=*/model_dims.max_seq_length,
          /*xDesc=*/input_desc.handles(),
          /*x=*/input_data.opaque(), /*hxDesc=*/input_h_desc.handle(),
          /*hx=*/input_h_data.opaque(), /*yDesc=*/output_desc.handles(),
          /*y=*/output_data.opaque(), /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size(),
          /*dwDesc=*/rnn_desc.params_handle(),
          /*dw=*/params_backprop_data->opaque(),
          /*reserveSpace=*/reserve_space_data->opaque(),
          /*reserveSpaceSizeInBytes=*/reserve_space_data->size()));
    }
  }

  if (is_profiling) {
    if (!timer->Stop(AsGpuStream(stream))) {
      return port::Status(port::error::INTERNAL, "Failed to stop timer");
    }
    auto algo_desc = *rnn_desc.algorithm_config().algorithm();
    output_profile_result->set_algorithm(algo_desc);
    output_profile_result->set_elapsed_time_in_ms(
        timer->GetElapsedMilliseconds());
  }

  return port::Status::OK();
}

port::Status CudnnSupport::DoCtcLossImpl(
    Stream* stream, const CudnnRnnStateTensorDescriptor& probs_desc,
    const DeviceMemoryBase probs_data, absl::Span<const int> labels_data,
    absl::Span<const int> labels_lengths_data,
    absl::Span<const int> input_lengths_data, DeviceMemoryBase costs_data,
    const CudnnRnnStateTensorDescriptor& grads_desc,
    DeviceMemoryBase grads_data, const CudnnCtcLossDescriptor& ctc_loss_desc,
    DeviceMemory<uint8> scratch_memory, int ctc_loss_algo_id) {
  auto cudnn = cudnn_->GetHandle(parent_, stream);

  int kNumTimestamps = probs_desc.num_layers();
  int kBatchSize = probs_desc.batch_size();
  int kNumLabels = probs_desc.data_size();
  int total_size = kNumLabels * kNumTimestamps * kBatchSize;
  (void)total_size;

#if CUDNN_VERSION >= 7603
  cudnnCTCLossAlgo_t ctc_loss_algo =
      static_cast<cudnnCTCLossAlgo_t>(ctc_loss_algo_id);
  RETURN_IF_CUDNN_ERROR(cudnnCTCLoss(
      /*handle=*/cudnn.handle(), /*probsDesc=*/probs_desc.handle(),
      /*probs=*/probs_data.opaque(), /*labels=*/labels_data.data(),
      /*labelLengths=*/labels_lengths_data.data(),
      /*inputLengths=*/input_lengths_data.data(),
      /*costs=*/costs_data.opaque(), /*gradientsDesc=*/grads_desc.handle(),
      /*gradients=*/grads_data.opaque(),
      /*algo=*/ctc_loss_algo,
      /*ctcLossDesc=*/ctc_loss_desc.handle(),
      /*workspace=*/scratch_memory.opaque(),
      /*workSpaceSizeInBytes=*/scratch_memory.size()));
#else
  return port::Status(port::error::INVALID_ARGUMENT,
                      "No supported cudnnCTCLoss when "
                      "CUDNN_VERSION < 7.6.3");
#endif

  return port::Status::OK();
}

port::StatusOr<std::unique_ptr<dnn::RnnDescriptor>>
CudnnSupport::createRnnDescriptor(
    int num_layers, int hidden_size, int input_size, int cell_size,
    int batch_size, dnn::RnnInputMode input_mode,
    dnn::RnnDirectionMode direction_mode, dnn::RnnMode rnn_mode,
    dnn::DataType data_type, const dnn::AlgorithmConfig& algorithm_config,
    float dropout, uint64_t seed, ScratchAllocator* state_allocator,
    bool use_padded_io) {
  // Setting up a cudnnRNNDescriptor requires a cuDNN handle, but because it's
  // not enqueueing anything into a stream, we pass in the null stream.
  auto cudnn = cudnn_->GetHandle(parent_, /*stream=*/nullptr);
  SE_ASSIGN_OR_RETURN(
      CudnnRnnDescriptor rnn_desc,
      CudnnRnnDescriptor::Create(
          cudnn, num_layers, hidden_size, input_size, cell_size, batch_size,
          ToCudnnRnnInputMode(input_mode),
          ToCudnnRnnDirectionMode(direction_mode), ToCudnnRnnMode(rnn_mode),
          ToCudnnDataType(data_type), GetRnnComputeType(data_type),
          algorithm_config, dropout, seed, state_allocator, use_padded_io));
  return std::unique_ptr<dnn::RnnDescriptor>(
      new CudnnRnnDescriptor(std::move(rnn_desc)));
}

port::StatusOr<std::unique_ptr<dnn::RnnSequenceTensorDescriptor>>
CudnnSupport::createRnnSequenceTensorDescriptor(int max_seq_length,
                                                int batch_size, int data_size,
                                                dnn::DataType data_type) {
  SE_ASSIGN_OR_RETURN(CudnnRnnSequenceTensorDescriptor descriptor,
                      CudnnRnnSequenceTensorDescriptor::Create(
                          parent_, max_seq_length, batch_size, data_size,
                          ToCudnnDataType(data_type)));
  return std::unique_ptr<dnn::RnnSequenceTensorDescriptor>(
      new CudnnRnnSequenceTensorDescriptor(std::move(descriptor)));
}

port::StatusOr<std::unique_ptr<dnn::RnnSequenceTensorDescriptor>>
CudnnSupport::createRnnSequenceTensorDescriptor(
    int max_seq_length, int batch_size, int data_size,
    const absl::Span<const int>& seq_lengths, bool time_major,
    dnn::DataType data_type) {
  SE_ASSIGN_OR_RETURN(CudnnRnnSequenceTensorDescriptor descriptor,
                      CudnnRnnSequenceTensorDescriptor::Create(
                          parent_, max_seq_length, batch_size, data_size,
                          seq_lengths, time_major, ToCudnnDataType(data_type)));
  return std::unique_ptr<dnn::RnnSequenceTensorDescriptor>(
      new CudnnRnnSequenceTensorDescriptor(std::move(descriptor)));
}

port::StatusOr<std::unique_ptr<dnn::RnnStateTensorDescriptor>>
CudnnSupport::createRnnStateTensorDescriptor(int num_layer, int batch_size,
                                             int data_size,
                                             dnn::DataType data_type) {
  return std::unique_ptr<dnn::RnnStateTensorDescriptor>(
      new CudnnRnnStateTensorDescriptor(parent_, num_layer, batch_size,
                                        data_size, ToCudnnDataType(data_type)));
}

bool CudnnSupport::DoRnnForward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<Eigen::half>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<Eigen::half>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<Eigen::half>& input_c_data,
    const DeviceMemory<Eigen::half>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    DeviceMemory<Eigen::half>* output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    DeviceMemory<Eigen::half>* output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    DeviceMemory<Eigen::half>* output_c_data, bool is_training,
    ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnForwardImpl<Eigen::half>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, is_training, reserve_space_allocator,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

bool CudnnSupport::DoRnnForward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<float>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<float>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<float>& input_c_data, const DeviceMemory<float>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    DeviceMemory<float>* output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    DeviceMemory<float>* output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    DeviceMemory<float>* output_c_data, bool is_training,
    ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnForwardImpl<float>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, is_training, reserve_space_allocator,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

bool CudnnSupport::DoRnnForward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<double>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<double>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<double>& input_c_data,
    const DeviceMemory<double>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    DeviceMemory<double>* output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    DeviceMemory<double>* output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    DeviceMemory<double>* output_c_data, bool is_training,
    ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnForwardImpl<double>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, is_training, reserve_space_allocator,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

bool CudnnSupport::DoRnnBackward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<Eigen::half>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<Eigen::half>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<Eigen::half>& input_c_data,
    const DeviceMemory<Eigen::half>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    const DeviceMemory<Eigen::half>& output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    const DeviceMemory<Eigen::half>& output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    const DeviceMemory<Eigen::half>& output_c_data,
    const DeviceMemory<Eigen::half>& output_backprop_data,
    const DeviceMemory<Eigen::half>& output_h_backprop_data,
    const DeviceMemory<Eigen::half>& output_c_backprop_data,
    DeviceMemory<Eigen::half>* input_backprop_data,
    DeviceMemory<Eigen::half>* input_h_backprop_data,
    DeviceMemory<Eigen::half>* input_c_backprop_data,
    DeviceMemory<Eigen::half>* params_backprop_data,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnBackwardImpl<Eigen::half>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, output_backprop_data, output_h_backprop_data,
          output_c_backprop_data, input_backprop_data, input_h_backprop_data,
          input_c_backprop_data, params_backprop_data, reserve_space_data,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

bool CudnnSupport::DoRnnBackward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<float>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<float>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<float>& input_c_data, const DeviceMemory<float>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    const DeviceMemory<float>& output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    const DeviceMemory<float>& output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    const DeviceMemory<float>& output_c_data,
    const DeviceMemory<float>& output_backprop_data,
    const DeviceMemory<float>& output_h_backprop_data,
    const DeviceMemory<float>& output_c_backprop_data,
    DeviceMemory<float>* input_backprop_data,
    DeviceMemory<float>* input_h_backprop_data,
    DeviceMemory<float>* input_c_backprop_data,
    DeviceMemory<float>* params_backprop_data,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnBackwardImpl<float>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, output_backprop_data, output_h_backprop_data,
          output_c_backprop_data, input_backprop_data, input_h_backprop_data,
          input_c_backprop_data, params_backprop_data, reserve_space_data,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

bool CudnnSupport::DoRnnBackward(
    Stream* stream, const dnn::RnnDescriptor& rnn_desc,
    const dnn::RnnSequenceTensorDescriptor& input_desc,
    const DeviceMemory<double>& input_data,
    const DeviceMemory<int>& seq_lengths_data,
    const dnn::RnnStateTensorDescriptor& input_h_desc,
    const DeviceMemory<double>& input_h_data,
    const dnn::RnnStateTensorDescriptor& input_c_desc,
    const DeviceMemory<double>& input_c_data,
    const DeviceMemory<double>& params,
    const dnn::RnnSequenceTensorDescriptor& output_desc,
    const DeviceMemory<double>& output_data,
    const dnn::RnnStateTensorDescriptor& output_h_desc,
    const DeviceMemory<double>& output_h_data,
    const dnn::RnnStateTensorDescriptor& output_c_desc,
    const DeviceMemory<double>& output_c_data,
    const DeviceMemory<double>& output_backprop_data,
    const DeviceMemory<double>& output_h_backprop_data,
    const DeviceMemory<double>& output_c_backprop_data,
    DeviceMemory<double>* input_backprop_data,
    DeviceMemory<double>* input_h_backprop_data,
    DeviceMemory<double>* input_c_backprop_data,
    DeviceMemory<double>* params_backprop_data,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator,
    dnn::ProfileResult* output_profile_result) {
  const CudnnRnnDescriptor& cudnn_rnn_desc =
      static_cast<const CudnnRnnDescriptor&>(rnn_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_input_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(input_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_input_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(input_c_desc);
  const CudnnRnnSequenceTensorDescriptor& cudnn_output_desc =
      static_cast<const CudnnRnnSequenceTensorDescriptor&>(output_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_h_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_h_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_output_c_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(output_c_desc);
  return IsStatusOk(
      DoRnnBackwardImpl<double>(
          stream, cudnn_rnn_desc, cudnn_input_desc, input_data,
          seq_lengths_data, cudnn_input_h_desc, input_h_data,
          cudnn_input_c_desc, input_c_data, params, cudnn_output_desc,
          output_data, cudnn_output_h_desc, output_h_data, cudnn_output_c_desc,
          output_c_data, output_backprop_data, output_h_backprop_data,
          output_c_backprop_data, input_backprop_data, input_h_backprop_data,
          input_c_backprop_data, params_backprop_data, reserve_space_data,
          workspace_allocator, output_profile_result),
      /*report_error=*/!output_profile_result);
}

namespace {

// TODO(csigg): Merge a lot of duplicate code below for forward, backward data,
// and backward filter.

port::StatusOr<cudnnConvolutionFwdAlgo_t> GetCudnnConvolutionForwardAlgo(
    const CudnnHandle& cudnn, const CudnnTensorDescriptor& input_nd,
    const CudnnFilterDescriptor& filter, const CudnnConvolutionDescriptor& conv,
    const CudnnTensorDescriptor& output_nd, bool specify_workspace_limit,
    size_t memory_limit_bytes) {
#if CUDNN_VERSION >= 8000
  const int num_requested_algos = 5;
  int num_returned_algos = 0;
  cudnnConvolutionFwdAlgoPerf_t perf_results[num_requested_algos];

  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionForwardAlgorithm_v7(
      cudnn.handle(), input_nd.handle(), filter.handle(), conv.handle(),
      output_nd.handle(), num_requested_algos, &num_returned_algos,
      perf_results));

  size_t mem_limit = specify_workspace_limit ? memory_limit_bytes : 0ULL;
  for (int r = 0; r < num_returned_algos; r++) {
    if (perf_results[r].status == CUDNN_STATUS_SUCCESS &&
        perf_results[r].algo != CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD_NONFUSED &&
        perf_results[r].memory <= mem_limit) {
      return perf_results[r].algo;
    }
  }
  return port::Status(port::error::INTERNAL,
                      "cudnnGetConvolutionForwardAlgorithm_v7 returned "
                      "no suitable algorithms. This could be a cudnn bug.");
#else
  cudnnConvolutionFwdPreference_t preference =
      specify_workspace_limit ? CUDNN_CONVOLUTION_FWD_SPECIFY_WORKSPACE_LIMIT
                              : CUDNN_CONVOLUTION_FWD_NO_WORKSPACE;
  cudnnConvolutionFwdAlgo_t algo_to_use;
  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionForwardAlgorithm(
      cudnn.handle(), input_nd.handle(), filter.handle(), conv.handle(),
      output_nd.handle(), preference, memory_limit_bytes, &algo_to_use));
  return algo_to_use;
#endif
}

port::StatusOr<cudnnConvolutionBwdDataAlgo_t>
GetCudnnConvolutionBackwardDataAlgo(const CudnnHandle& cudnn,
                                    const CudnnTensorDescriptor& input_nd,
                                    const CudnnFilterDescriptor& filter,
                                    const CudnnConvolutionDescriptor& conv,
                                    const CudnnTensorDescriptor& output_nd,
                                    bool specify_workspace_limit,
                                    size_t memory_limit_bytes) {
#if CUDNN_VERSION >= 8000
  const int num_requested_algos = 5;
  int num_returned_algos = 0;
  cudnnConvolutionBwdDataAlgoPerf_t perf_results[num_requested_algos];

  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardDataAlgorithm_v7(
      cudnn.handle(), filter.handle(), output_nd.handle(), conv.handle(),
      input_nd.handle(), num_requested_algos, &num_returned_algos,
      perf_results));

  size_t mem_limit = specify_workspace_limit ? memory_limit_bytes : 0ULL;
  for (int r = 0; r < num_returned_algos; r++) {
    if (perf_results[r].status == CUDNN_STATUS_SUCCESS &&
        perf_results[r].algo !=
            CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD_NONFUSED &&
        perf_results[r].memory <= mem_limit) {
      return perf_results[r].algo;
    }
  }
  return port::Status(port::error::INTERNAL,
                      "cudnnGetConvolutionBackwardDataAlgorithm_v7 returned "
                      "no suitable algorithms. This could be a cudnn bug.");
#else
  cudnnConvolutionBwdDataPreference_t preference =
      specify_workspace_limit
          ? CUDNN_CONVOLUTION_BWD_DATA_SPECIFY_WORKSPACE_LIMIT
          : CUDNN_CONVOLUTION_BWD_DATA_NO_WORKSPACE;
  cudnnConvolutionBwdDataAlgo_t algo_to_use;
  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardDataAlgorithm(
      cudnn.handle(), filter.handle(), output_nd.handle(), conv.handle(),
      input_nd.handle(), preference, memory_limit_bytes, &algo_to_use));
  return algo_to_use;
#endif
}

port::StatusOr<cudnnConvolutionBwdFilterAlgo_t>
GetCudnnConvolutionBackwardFilterAlgo(const CudnnHandle& cudnn,
                                      const CudnnTensorDescriptor& input_nd,
                                      const CudnnFilterDescriptor& filter,
                                      const CudnnConvolutionDescriptor& conv,
                                      const CudnnTensorDescriptor& output_nd,
                                      bool specify_workspace_limit,
                                      size_t memory_limit_bytes) {
#if CUDNN_VERSION >= 8000
  const int num_requested_algos = 5;
  int num_returned_algos = 0;
  cudnnConvolutionBwdFilterAlgoPerf_t perf_results[num_requested_algos];

  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardFilterAlgorithm_v7(
      cudnn.handle(), input_nd.handle(), output_nd.handle(), conv.handle(),
      filter.handle(), num_requested_algos, &num_returned_algos, perf_results));

  size_t mem_limit = specify_workspace_limit ? memory_limit_bytes : 0ULL;
  for (int r = 0; r < num_returned_algos; r++) {
    if (perf_results[r].status == CUDNN_STATUS_SUCCESS &&
        perf_results[r].algo !=
            CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD_NONFUSED &&
        perf_results[r].memory <= mem_limit) {
      return perf_results[r].algo;
    }
  }
  return port::Status(port::error::INTERNAL,
                      "cudnnGetConvolutionBackwardFilterAlgorithm_v7 returned "
                      "no suitable algorithms. This could be a cudnn bug.");
#else
  cudnnConvolutionBwdFilterPreference_t preference =
      specify_workspace_limit
          ? CUDNN_CONVOLUTION_BWD_FILTER_SPECIFY_WORKSPACE_LIMIT
          : CUDNN_CONVOLUTION_BWD_FILTER_NO_WORKSPACE;
  cudnnConvolutionBwdFilterAlgo_t algo_to_use;
  RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardFilterAlgorithm(
      cudnn.handle(), input_nd.handle(), output_nd.handle(), conv.handle(),
      filter.handle(), preference, memory_limit_bytes, &algo_to_use));
  return algo_to_use;
#endif
}

port::StatusOr<DeviceMemory<uint8>> AllocateCudnnConvolutionForwardWorkspace(
    Stream* stream, const CudnnHandle& cudnn,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    const CudnnConvolutionDescriptor& conv,
    const CudnnTensorDescriptor& output_nd,
    const dnn::AlgorithmDesc& algorithm_desc,
    ScratchAllocator* scratch_allocator) {
  if (IsTensorMathOpSet(conv) != algorithm_desc.tensor_ops_enabled()) {
    return port::Status(
        port::error::INTERNAL,
        "Mismatch between cudnn conv and algorithm descriptors.");
  }

  // Query the size of the workspace and allocate it.
  size_t size_in_bytes;
  if (algorithm_desc.workspace_size()) {
    size_in_bytes = *algorithm_desc.workspace_size();
  } else {
    RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionForwardWorkspaceSize(
        cudnn.handle(),
        /*xDesc=*/input_nd.handle(),
        /*wDesc=*/filter.handle(), /*convDesc=*/conv.handle(),
        /*yDesc=*/output_nd.handle(),
        /*algo=*/ToConvForwardAlgo(algorithm_desc),
        /*sizeInBytes=*/&size_in_bytes));
  }

  int64_t size_in_bytes_int64_t = size_in_bytes;

  if (TF_PREDICT_FALSE(size_in_bytes_int64_t < 0)) {
    return port::Status(
        port::error::INTERNAL,
        "cudnnGetConvolutionForwardWorkspaceSize() returned "
        "negative sizeInBytes value. This could be a cudnn bug.");
  }

  if (size_in_bytes_int64_t == 0) {
    return DeviceMemory<uint8>();
  }

  if (TF_PREDICT_FALSE(!scratch_allocator)) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "No scratch allocator provided");
  }

  return scratch_allocator->AllocateBytes(size_in_bytes);
}

port::StatusOr<DeviceMemory<uint8>>
AllocateCudnnConvolutionBackwardDataWorkspace(
    Stream* stream, const CudnnHandle& cudnn,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    const CudnnConvolutionDescriptor& conv,
    const CudnnTensorDescriptor& output_nd,
    const dnn::AlgorithmDesc& algorithm_desc,
    ScratchAllocator* scratch_allocator) {
  if (IsTensorMathOpSet(conv) != algorithm_desc.tensor_ops_enabled()) {
    return port::Status(
        port::error::INTERNAL,
        "Mismatch between cudnn conv and algorithm descriptors.");
  }

  // Query the size of the workspace and allocate it.
  size_t size_in_bytes;
  if (algorithm_desc.workspace_size()) {
    size_in_bytes = *algorithm_desc.workspace_size();
  } else {
    RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardDataWorkspaceSize(
        cudnn.handle(),
        /*wDesc=*/filter.handle(),
        /*dyDesc=*/output_nd.handle(),
        /*convDesc=*/conv.handle(),
        /*dxDesc=*/input_nd.handle(),
        /*algo=*/ToConvBackwardDataAlgo(algorithm_desc),
        /*sizeInBytes=*/&size_in_bytes));
  }

  int64_t size_in_bytes_int64_t = size_in_bytes;

  if (TF_PREDICT_FALSE(size_in_bytes_int64_t < 0)) {
    return port::Status(
        port::error::INTERNAL,
        "cudnnGetConvolutionBackwardDataWorkspaceSize() returned "
        "negative sizeInBytes value. This could be a cudnn bug.");
  }

  if (size_in_bytes_int64_t == 0) {
    return DeviceMemory<uint8>();
  }

  if (TF_PREDICT_FALSE(!scratch_allocator)) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "No scratch allocator provided");
  }

  return scratch_allocator->AllocateBytes(size_in_bytes);
}

port::StatusOr<DeviceMemory<uint8>>
AllocateCudnnConvolutionBackwardFilterWorkspace(
    Stream* stream, const CudnnHandle& cudnn,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    const CudnnConvolutionDescriptor& conv,
    const CudnnTensorDescriptor& output_nd,
    const dnn::AlgorithmDesc& algorithm_desc,
    ScratchAllocator* scratch_allocator) {
  if (IsTensorMathOpSet(conv) != algorithm_desc.tensor_ops_enabled()) {
    return port::Status(
        port::error::INTERNAL,
        "Mismatch between cudnn conv and algorithm descriptors.");
  }

  // Query the size of the workspace and allocate it.
  size_t size_in_bytes;
  if (algorithm_desc.workspace_size()) {
    size_in_bytes = *algorithm_desc.workspace_size();
  } else {
    RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardFilterWorkspaceSize(
        cudnn.handle(),
        /*xDesc=*/input_nd.handle(),
        /*dyDesc=*/output_nd.handle(),
        /*convDesc=*/conv.handle(),
        /*gradDesc=*/filter.handle(),
        /*algo=*/ToConvBackwardFilterAlgo(algorithm_desc),
        /*sizeInBytes=*/&size_in_bytes));
  }

  int64_t size_in_bytes_int64_t = size_in_bytes;

  if (TF_PREDICT_FALSE(size_in_bytes_int64_t < 0)) {
    return port::Status(
        port::error::INTERNAL,
        "cudnnGetConvolutionBackwardFilterWorkspaceSize() returned "
        "negative sizeInBytes value. This could be a cudnn bug.");
  }

  if (size_in_bytes_int64_t == 0) {
    return DeviceMemory<uint8>();
  }

  if (TF_PREDICT_FALSE(!scratch_allocator)) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "No scratch allocator provided");
  }

  return scratch_allocator->AllocateBytes(size_in_bytes);
}

port::StatusOr<bool> UseTensorOps(Stream* stream, dnn::DataType type,
                                  absl::optional<dnn::AlgorithmDesc> desc) {
  bool use_tensor_ops;
  if (desc.has_value()) {
    use_tensor_ops = desc->tensor_ops_enabled();
    if (use_tensor_ops && !IsTensorMathEnabled(stream, type)) {
      return port::Status(port::error::INVALID_ARGUMENT,
                          "Algo requests disabled tensor op evaluation.");
    }
  } else {
    use_tensor_ops = IsTensorMathEnabled(stream, type);
  }
  return use_tensor_ops;
}

cudnnDataType_t GetRnnComputeType(dnn::DataType data_type);
dnn::DataType GetConvAccumulatorType(dnn::DataType data_type);

port::StatusOr<dnn::AlgorithmDesc> GetCudnnConvolutionForwardAlgorithm(
    Stream* stream, const CudnnHandle& cudnn,
    const dnn::AlgorithmConfig& algorithm_config,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    dnn::DataType element_type,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    const CudnnTensorDescriptor& output_nd, ScratchAllocator* scratch_allocator,
    DeviceMemory<uint8>* scratch) {
  absl::optional<dnn::AlgorithmDesc> algo_desc = algorithm_config.algorithm();

  CudnnConvolutionDescriptor conv(
      convolution_descriptor,
      ToCudnnDataType(GetConvAccumulatorType(element_type)));
  bool use_tensor_ops;
  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);

  if (!algo_desc.has_value()) {
    // Pick fastest algorithm within memory limit according to cuDNN's
    // heuristics.
    bool specify_workspace_limit = scratch_allocator != nullptr;
    auto memory_limit_bytes =
        specify_workspace_limit
            ? std::max(scratch_allocator->GetMemoryLimitInBytes(), int64_t{0})
            : int64_t{0};
    SE_ASSIGN_OR_RETURN(cudnnConvolutionFwdAlgo_t algo,
                        GetCudnnConvolutionForwardAlgo(
                            cudnn, input_nd, filter, conv, output_nd,
                            specify_workspace_limit, memory_limit_bytes));
    algo_desc = dnn::AlgorithmDesc(algo, use_tensor_ops);
  }

  const auto scratch_or = AllocateCudnnConvolutionForwardWorkspace(
      stream, cudnn, input_nd, filter, conv, output_nd, *algo_desc,
      scratch_allocator);

  if (scratch_or.ok()) {
    *scratch = scratch_or.ValueOrDie();
    return *algo_desc;
  }

  algo_desc = algorithm_config.algorithm_no_scratch();

  // Failed to allocate workspace for the first algorithm, fall back to the
  // no_scratch algorithm.
  if (!algo_desc.has_value()) {
    return port::Status(
        scratch_or.status().code(),
        absl::StrCat("The primary convolution algorithm failed, ",
                     "while a secondary algorithm is not provided. ",
                     "Returned status: ", scratch_or.status().ToString()));
  }

  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);
  SE_ASSIGN_OR_RETURN(*scratch, AllocateCudnnConvolutionForwardWorkspace(
                                    stream, cudnn, input_nd, filter, conv,
                                    output_nd, *algo_desc, scratch_allocator));
  return *algo_desc;
}

port::StatusOr<dnn::AlgorithmDesc> GetCudnnConvolutionBackwardDataAlgorithm(
    Stream* stream, const CudnnHandle& cudnn,
    const dnn::AlgorithmConfig& algorithm_config,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    dnn::DataType element_type,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    const CudnnTensorDescriptor& output_nd, ScratchAllocator* scratch_allocator,
    DeviceMemory<uint8>* scratch) {
  absl::optional<dnn::AlgorithmDesc> algo_desc = algorithm_config.algorithm();
  CudnnConvolutionDescriptor conv(
      convolution_descriptor,
      ToCudnnDataType(GetConvAccumulatorType(element_type)));
  bool use_tensor_ops;
  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);

  if (!algo_desc.has_value()) {
    // Pick fastest algorithm within memory limit according to cuDNN's
    // heuristics.
    bool specify_workspace_limit = scratch_allocator != nullptr;
    auto memory_limit_bytes =
        specify_workspace_limit
            ? std::max(scratch_allocator->GetMemoryLimitInBytes(), int64_t{0})
            : int64_t{0};
    SE_ASSIGN_OR_RETURN(cudnnConvolutionBwdDataAlgo_t algo,
                        GetCudnnConvolutionBackwardDataAlgo(
                            cudnn, input_nd, filter, conv, output_nd,
                            specify_workspace_limit, memory_limit_bytes));
    algo_desc = dnn::AlgorithmDesc(algo, use_tensor_ops);
  }

  const auto scratch_or = AllocateCudnnConvolutionBackwardDataWorkspace(
      stream, cudnn, input_nd, filter, conv, output_nd, *algo_desc,
      scratch_allocator);

  if (scratch_or.ok()) {
    *scratch = scratch_or.ValueOrDie();
    return *algo_desc;
  }

  algo_desc = algorithm_config.algorithm_no_scratch();

  // Failed to allocate workspace for the first algorithm, fall back to the
  // no_scratch algorithm.
  if (!algo_desc.has_value()) {
    return port::Status(
        port::error::INVALID_ARGUMENT,
        "The primary convolution algorithm failed memory allocation, "
        "while a secondary algorithm is not provided.");
  }

  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);
  SE_ASSIGN_OR_RETURN(*scratch, AllocateCudnnConvolutionBackwardDataWorkspace(
                                    stream, cudnn, input_nd, filter, conv,
                                    output_nd, *algo_desc, scratch_allocator));
  return *algo_desc;
}

port::StatusOr<dnn::AlgorithmDesc> GetCudnnConvolutionBackwardFilterAlgorithm(
    Stream* stream, const CudnnHandle& cudnn,
    const dnn::AlgorithmConfig& algorithm_config,
    const CudnnTensorDescriptor& input_nd, const CudnnFilterDescriptor& filter,
    dnn::DataType element_type,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    const CudnnTensorDescriptor& output_nd, ScratchAllocator* scratch_allocator,
    DeviceMemory<uint8>* scratch) {
  absl::optional<dnn::AlgorithmDesc> algo_desc = algorithm_config.algorithm();
  CudnnConvolutionDescriptor conv(
      convolution_descriptor,
      ToCudnnDataType(GetConvAccumulatorType(element_type)));
  bool use_tensor_ops;
  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);

  if (!algo_desc.has_value()) {
    // Pick fastest algorithm within memory limit according to cuDNN's
    // heuristics.
    bool specify_workspace_limit = scratch_allocator != nullptr;
    auto memory_limit_bytes =
        specify_workspace_limit
            ? std::max(scratch_allocator->GetMemoryLimitInBytes(), int64_t{0})
            : int64_t{0};
    SE_ASSIGN_OR_RETURN(cudnnConvolutionBwdFilterAlgo_t algo,
                        GetCudnnConvolutionBackwardFilterAlgo(
                            cudnn, input_nd, filter, conv, output_nd,
                            specify_workspace_limit, memory_limit_bytes));
    algo_desc = dnn::AlgorithmDesc(algo, use_tensor_ops);
  }

  port::StatusOr<DeviceMemory<uint8>> scratch_or =
      AllocateCudnnConvolutionBackwardFilterWorkspace(
          stream, cudnn, input_nd, filter, conv, output_nd, *algo_desc,
          scratch_allocator);

  if (scratch_or.ok()) {
    *scratch = scratch_or.ValueOrDie();
    return *algo_desc;
  }

  algo_desc = algorithm_config.algorithm_no_scratch();

  // Failed to allocate workspace for the first algorithm, fall back to the
  // no_scratch algorithm.
  if (!algo_desc.has_value()) {
    return port::Status(
        port::error::INVALID_ARGUMENT,
        absl::StrCat(
            "The primary convolution algorithm failed memory allocation, "
            "while a secondary algorithm is not provided. Actual error: ",
            scratch_or.status().ToString()));
  }

  SE_ASSIGN_OR_RETURN(use_tensor_ops,
                      UseTensorOps(stream, element_type, algo_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);
  SE_ASSIGN_OR_RETURN(*scratch, AllocateCudnnConvolutionBackwardFilterWorkspace(
                                    stream, cudnn, input_nd, filter, conv,
                                    output_nd, *algo_desc, scratch_allocator));
  return *algo_desc;
}

// A helper class to set env-vars and choose options for cudnn-related
// algorithms.
template <typename EnvVar>
class CudnnEnvVar {
 public:
  static bool IsEnabled() {
    static bool is_enabled = IsEnabledImpl();
    return is_enabled;
  }

 private:
  static bool IsEnabledImpl() {
    const char* tf_env_var_val = getenv(EnvVar::kName);
    if (tf_env_var_val != nullptr) {
      absl::string_view tf_env_var_val_str(tf_env_var_val);
      if (tf_env_var_val_str == "0") {
        return false;
      }
      return true;
    }
    return EnvVar::kDefaultFlag;
  }
};

// A helper struct to decide whether to enable the FFT_TILING algorithms for
// forward convolution. It is disabled for cuDNN < 7 due to memory corruption
// caused by some shapes with this algorithm. Users can explicitly enable the
// algorithm through an env-var "TF_ENABLE_FFT_TILING_FORWARD=1".
struct FftTilingForward {
  static constexpr const char* kName = "TF_ENABLE_FFT_TILING_FORWARD";
  static constexpr bool kDefaultFlag = true;
};

// A helper struct to decide whether to enable the WINOGRAD_NONFUSED algorithms.
// By default it is turned on, users can explicitly disable them through an
// env-var "TF_ENABLE_WINOGRAD_NONFUSED=0".
// https://github.com/tensorflow/tensorflow/pull/4901
// For CUDNN v8.1, when this env-var is turned off, both the winograd and
// winograd-non-fused engines will be ruled out.
struct WinogradNonfused {
  static constexpr const char* kName = "TF_ENABLE_WINOGRAD_NONFUSED";
  // NVIDIA has fixed winograd nonfused bug for cudnn v>=7. For older versions,
  // we have a workaround.
  static constexpr bool kDefaultFlag = true;
};

// A helper struct to decide whether to use FP32 as the internal compute type
// for convolution when the input data type is FP16. By default it is turned on,
// users can explicitly disable them (choose to use FP16 as the internal compute
// type) through an env-var "TF_FP16_CONV_USE_FP32_COMPUTE=0".
struct ConvDoFP32ComputationFP16Input {
  static constexpr const char* kName = "TF_FP16_CONV_USE_FP32_COMPUTE";
  // Using FP16 as the internal compute type for convolution when the input data
  // type is FP16 is only supported on architectures with true fp16 support
  // (compute capability 5.3 and 6.0). Setting this to false in an unsupported
  // architecture will cause internal errors.
  static constexpr bool kDefaultFlag = true;
};

// A helper struct to decide whether to use FP32 as the internal compute type
// for rnn when the input data type is FP16. At present it is turned off,
// users can explicitly control them through an env-var
// TF_FP16_RNN_USE_FP32_COMPUTE.
// After the TODO below is fixed, users should almost always use fp32 compute
// type for training. Using fp16 might suffer suboptimal accuracy due to loss
// in precision.
struct RnnDoFP32ComputationFP16Input {
  static constexpr const char* kName = "TF_FP16_RNN_USE_FP32_COMPUTE";
  // TODO(jamesqin): b/78182362 flip to true when cudnn 7.1.4 fixes the bug.
  // Before cudnn 7.1.4 RNN are always done in fp32, no matter what math
  // precision is set.
  // Set it temporary to false s.t. no error is raised when using fp16 inputs,
  // fp32 math precision.
  //
  // cuDNN == 7.5.0 is verified to have this fixed.
  static constexpr bool kDefaultFlag = CUDNN_VERSION >= 7500;
};

namespace {

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

bool GenericEngineFilter(cudnnBackendDescriptor_t engine_config,
                         bool disable_winograd, bool disable_nondeterminism,
                         bool disable_tensor_core) {
  bool ret = cudnn_frontend::hasNumericalNote<
      CUDNN_NUMERICAL_NOTE_DOWN_CONVERT_INPUTS>(engine_config);

  if (disable_winograd) {
    ret |= cudnn_frontend::hasNumericalNote<CUDNN_NUMERICAL_NOTE_WINOGRAD>(
        engine_config);
  }

  if (disable_nondeterminism) {
    ret |=
        cudnn_frontend::hasNumericalNote<CUDNN_NUMERICAL_NOTE_NONDETERMINISTIC>(
            engine_config);
  }

  if (disable_tensor_core) {
    ret |= cudnn_frontend::hasNumericalNote<CUDNN_NUMERICAL_NOTE_TENSOR_CORE>(
        engine_config);
  }

  return ret;
}

#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

}  // namespace

cudnnDataType_t GetRnnComputeType(dnn::DataType data_type) {
  switch (data_type) {
    case dnn::DataType::kFloat:
      return CUDNN_DATA_FLOAT;
    case dnn::DataType::kDouble:
      return CUDNN_DATA_DOUBLE;
    case dnn::DataType::kHalf:
      if (CudnnEnvVar<RnnDoFP32ComputationFP16Input>::IsEnabled()) {
        return CUDNN_DATA_FLOAT;
      } else {
        return CUDNN_DATA_HALF;
      }
    default:
      LOG(FATAL) << "Invalid RNN data type: " << static_cast<int>(data_type);
  }
}

dnn::DataType GetConvActivationType(dnn::DataType data_type) {
  switch (data_type) {
    case dnn::DataType::kFloat:
    case dnn::DataType::kDouble:
      return data_type;
    // TODO(awpr): it's not clear whether float-precision activations on
    // half-precision convs are supported; double-check.
    case dnn::DataType::kHalf:
      return CudnnEnvVar<ConvDoFP32ComputationFP16Input>::IsEnabled()
                 ? dnn::DataType::kFloat
                 : dnn::DataType::kHalf;
    case dnn::DataType::kInt8:
    case dnn::DataType::kInt32:  // TODO(awpr): does int32 do blending in float?
      return dnn::DataType::kFloat;
#if CUDNN_VERSION >= 8200
    // TODO(awpr): as with kHalf, this is not clear.
    case dnn::DataType::kBF16:
      return CudnnEnvVar<ConvDoFP32ComputationFP16Input>::IsEnabled()
                 ? dnn::DataType::kFloat
                 : dnn::DataType::kBF16;
#endif
    default:
      LOG(FATAL) << "Invalid DNN data type: " << static_cast<int>(data_type);
  }
}

dnn::DataType GetConvAccumulatorType(dnn::DataType data_type) {
  switch (data_type) {
    case dnn::DataType::kFloat:
    case dnn::DataType::kDouble:
      return data_type;
    case dnn::DataType::kHalf:
      return CudnnEnvVar<ConvDoFP32ComputationFP16Input>::IsEnabled()
                 ? dnn::DataType::kFloat
                 : dnn::DataType::kHalf;
    case dnn::DataType::kInt8:
    case dnn::DataType::kInt32:
      return dnn::DataType::kInt32;
#if CUDNN_VERSION >= 8200
    case dnn::DataType::kBF16:
      return CudnnEnvVar<ConvDoFP32ComputationFP16Input>::IsEnabled()
                 ? dnn::DataType::kFloat
                 : dnn::DataType::kBF16;
#endif
    default:
      LOG(FATAL) << "Invalid DNN data type: " << static_cast<int>(data_type);
  }
}

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

namespace {
cudnnBackendHeurMode_t GetCudnnFrontendHeurMode() {
#if CUDNN_VERSION >= 8300
  return CUDNN_HEUR_MODE_B;
#else
  return CUDNN_HEUR_MODE_INSTANT;
#endif  // CUDNN_VERSION >= 8300
}

cudnnBackendDescriptorType_t GetCudnnConvolutionType(
    dnn::ConvolutionKind kind) {
  cudnnBackendDescriptorType_t conv_mode;
  switch (kind) {
    case dnn::ConvolutionKind::FORWARD: {
      conv_mode = CUDNN_BACKEND_OPERATION_CONVOLUTION_FORWARD_DESCRIPTOR;
      break;
    }
    case dnn::ConvolutionKind::BACKWARD_DATA: {
      conv_mode = CUDNN_BACKEND_OPERATION_CONVOLUTION_BACKWARD_DATA_DESCRIPTOR;
      break;
    }
    case dnn::ConvolutionKind::BACKWARD_FILTER: {
      conv_mode =
          CUDNN_BACKEND_OPERATION_CONVOLUTION_BACKWARD_FILTER_DESCRIPTOR;
      break;
    }
    default:
      LOG(FATAL) << "Unexpected convolution kind " << static_cast<int>(kind);
      break;
  }
  return conv_mode;
}

// Cudnn only supports vectorization over the channel dimension (e.g., int8x4,
// or int8x32).
std::tuple<int, int> GetTensorVectorSizeAndDim(
    const dnn::BatchDescriptor& tensor, dnn::DataType element_type) {
  int vector_size = 1;
  int vector_dim = -1;
  if (element_type == dnn::DataType::kInt8) {
    if (tensor.layout() == dnn::DataLayout::kBatchDepthYX4) {
      vector_size = 4;
      vector_dim = 1;
    } else if (tensor.layout() == dnn::DataLayout::kBatchDepthYX32) {
      vector_size = 32;
      vector_dim = 1;
    }
  }
  return std::make_tuple(vector_size, vector_dim);
}

std::tuple<int, int> GetTensorVectorSizeAndDim(
    const dnn::FilterDescriptor& filter, dnn::DataType element_type) {
  int vector_size = 1;
  int vector_dim = -1;
  if (element_type == dnn::DataType::kInt8) {
    if (filter.layout() == dnn::FilterLayout::kOutputInputYX4) {
      vector_size = 4;
      vector_dim = 1;
    } else if (filter.layout() == dnn::FilterLayout::kOutputInputYX32) {
      vector_size = 32;
      vector_dim = 1;
    }
  }
  return std::make_tuple(vector_size, vector_dim);
}

port::StatusOr<cudnn_frontend::Tensor> CreateCudnnTensor(
    absl::Span<const int64_t> dims, absl::Span<const int64_t> strides,
    int64_t uid, dnn::DataType dtype, int64_t vec_count, int64_t vec_dim,
    bool is_virtual = false) {
  auto tensor = cudnn_frontend::TensorBuilder()
                    .setDim(dims.size(), dims.data())
                    .setStrides(strides.size(), strides.data())
                    .setId(uid)
                    .setAlignment(32)
                    .setDataType(ToCudnnDataType(dtype))
                    .setVectorCountAndDimension(vec_count, vec_dim)
                    .setVirtual(is_virtual)
                    .build();
  RETURN_MSG_IF_CUDNN_ERROR(tensor);
  return tensor;
}

port::StatusOr<std::unique_ptr<cudnn_frontend::OperationGraph>>
GetCudnnOperationGraph(dnn::ConvolutionKind kind, dnn::DataType input_type,
                       dnn::DataType output_type,
                       const dnn::BatchDescriptor& input_descriptor,
                       const dnn::FilterDescriptor& filter_descriptor,
                       const dnn::BatchDescriptor& output_descriptor,
                       const dnn::ConvolutionDescriptor& convolution_descriptor,
                       CudnnHandle& cudnn) {
  cudnnBackendDescriptorType_t conv_mode = GetCudnnConvolutionType(kind);

  // x tensor.
  int vector_size, vector_dim;
  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(input_descriptor, input_type);
  std::vector<int64_t> input_dims = input_descriptor.vectorized_dims(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  std::vector<int64_t> input_strides = input_descriptor.vectorized_strides(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);

  if (vector_size == 32) {
    return port::InternalError(
        "cuDNN frontend doesn't support Tx32 at the moment.");
  }

  TF_ASSIGN_OR_RETURN(auto tensor_x,
                      CreateCudnnTensor(input_dims, input_strides, 'x',
                                        input_type, vector_size, vector_dim));

  // y tensor.
  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(output_descriptor, output_type);
  std::vector<int64_t> output_dims = output_descriptor.vectorized_dims(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  std::vector<int64_t> output_strides = output_descriptor.vectorized_strides(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);

  TF_ASSIGN_OR_RETURN(auto tensor_y,
                      CreateCudnnTensor(output_dims, output_strides, 'y',
                                        output_type, vector_size, vector_dim));

  // w tensor.
  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(filter_descriptor, input_type);
  std::vector<int64_t> filter_dims = filter_descriptor.vectorized_dims(
      dnn::FilterLayout::kOutputInputYX, vector_size, vector_dim);
  std::vector<int64_t> filter_strides = filter_descriptor.vectorized_strides(
      dnn::FilterLayout::kOutputInputYX, vector_size, vector_dim);

  TF_ASSIGN_OR_RETURN(auto tensor_w,
                      CreateCudnnTensor(filter_dims, filter_strides, 'w',
                                        input_type, vector_size, vector_dim));

  // conv_desc.
  auto mode = convolution_descriptor.convolution_not_crosscorr()
                  ? CUDNN_CONVOLUTION
                  : CUDNN_CROSS_CORRELATION;

  int conv_dim = convolution_descriptor.ndims();

  auto accumulator_type = ToCudnnDataType(GetConvAccumulatorType(input_type));
  CHECK_NE(convolution_descriptor.pad_alignment(),
           dnn::PadAlignment::kTensorFlowPadding)
      << "TensorFlow padding alignment is not supported.";

  auto conv_desc =
      cudnn_frontend::ConvDescBuilder()
          .setComputePrecision(accumulator_type)
          .setMathMode(mode)
          .setNDims(conv_dim)
          .setStrides(conv_dim, convolution_descriptor.strides().data())
          .setPrePadding(conv_dim, convolution_descriptor.padding().data())
          .setPostPadding(conv_dim, convolution_descriptor.padding().data())
          .setDilation(conv_dim, convolution_descriptor.dilations().data())
          .build();
  RETURN_MSG_IF_CUDNN_ERROR(conv_desc);

  double alpha = 1.0;
  double beta = 0.0;

  // CUDNN Operation
  auto op = cudnn_frontend::OperationBuilder(conv_mode)
                .setxDesc(tensor_x)
                .setyDesc(tensor_y)
                .setwDesc(tensor_w)
                .setcDesc(conv_desc)
                .setAlpha(alpha)
                .setBeta(beta)
                .build();
  RETURN_MSG_IF_CUDNN_ERROR(op);

  // CUDNN OperationGraph
  std::array<cudnn_frontend::Operation const*, 1> ops = {&op};
  auto opGraph = cudnn_frontend::OperationGraphBuilder()
                     .setHandle(cudnn.handle())
                     .setOperationGraph(ops.size(), ops.data())
                     .build();
  RETURN_MSG_IF_CUDNN_ERROR(opGraph);

  VLOG(4) << "\nTensor_x: " << tensor_x.describe()
          << "\nTensor_y: " << tensor_y.describe()
          << "\nTensor_w: " << tensor_w.describe()
          << "\nConv: " << conv_desc.describe() << "\nOp: " << op.describe()
          << "\nOpGraph: " << opGraph.describe();

  return std::unique_ptr<cudnn_frontend::OperationGraph>(
      new cudnn_frontend::OperationGraph(std::move(opGraph)));
}

port::StatusOr<std::unique_ptr<cudnn_frontend::OperationGraph>>
GetCudnnFusedOperationGraph(
    dnn::ConvolutionKind kind, dnn::DataType input_type,
    dnn::DataType bias_type, dnn::DataType output_type, double alpha,
    double alpha2, const dnn::BatchDescriptor& input_descriptor,
    const dnn::FilterDescriptor& filter_descriptor,
    dnn::BatchDescriptor bias_descriptor,
    const dnn::BatchDescriptor& output_descriptor,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    const dnn::ActivationMode activation_mode, CudnnHandle& cudnn) {
  cudnnBackendDescriptorType_t conv_mode = GetCudnnConvolutionType(kind);
  dnn::DataType accumulator_type = GetConvAccumulatorType(input_type);
  dnn::DataType activation_type = GetConvActivationType(input_type);

  // CUDNN fused operation supports the pattern in the form of
  // Conv + Add + BiasAdd + Act. Therefore, we need to build a graph of the
  // four ops with their input/output tensor edges:
  // Conv   : input: tensor_x, tensor_w;    output: tensor_conv (virtual)
  // Add    : input: tensor_conv, tensor_z; output: tensor_add (virtual)
  // BiasAdd: input: tensor_add, tensor_b;  output: tensor_bias (virtual)
  // Act    : input: tensor_bias;           output: tensor_y
  int vector_size, vector_dim;
  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(input_descriptor, input_type);
  std::vector<int64_t> input_dims = input_descriptor.vectorized_dims(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  std::vector<int64_t> input_strides = input_descriptor.vectorized_strides(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);

  if (vector_size == 32) {
    return port::InternalError(
        "cuDNN frontend doesn't support Tx32 at the moment.");
  }

  TF_ASSIGN_OR_RETURN(auto tensor_x,
                      CreateCudnnTensor(input_dims, input_strides, 'x',
                                        input_type, vector_size, vector_dim));

  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(output_descriptor, output_type);
  std::vector<int64_t> output_dims = output_descriptor.vectorized_dims(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  std::vector<int64_t> output_strides = output_descriptor.vectorized_strides(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  TF_ASSIGN_OR_RETURN(auto tensor_y,
                      CreateCudnnTensor(output_dims, output_strides, 'y',
                                        output_type, vector_size, vector_dim));

  TF_ASSIGN_OR_RETURN(auto tensor_z,
                      CreateCudnnTensor(output_dims, output_strides, 'z',
                                        output_type, vector_size, vector_dim));

  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(filter_descriptor, input_type);
  std::vector<int64_t> filter_dims = filter_descriptor.vectorized_dims(
      dnn::FilterLayout::kOutputInputYX, vector_size, vector_dim);
  std::vector<int64_t> filter_strides = filter_descriptor.vectorized_strides(
      dnn::FilterLayout::kOutputInputYX, vector_size, vector_dim);
  TF_ASSIGN_OR_RETURN(auto tensor_w,
                      CreateCudnnTensor(filter_dims, filter_strides, 'w',
                                        input_type, vector_size, vector_dim));

  // For the purposes of the cudnn graph, say that the bias tensor has the same
  // layout as the output tensor.  It doesn't actually matter, because bias is a
  // 1D array.  But we need to get the correct vectorization, otherwise the
  // cudnn graph API rejects this tensor, even though vectorized float tensors
  // aren't even a thing in cuDNN.
  bias_descriptor.set_layout(output_descriptor.layout());

  // Even more unnecessarily subtle: since vectorized float tensors don't exist,
  // `GetVectorSizeAndDim` ignores vectorized layouts for floating-point types,
  // so we have to ask it for vector sizes as if the type were `input_type`, as
  // opposed to `bias_type`.  For non-int8 types, these are the same anyway, so
  // this only affects int8 convolutions.
  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(bias_descriptor, input_type);
  std::vector<int64_t> bias_dims = bias_descriptor.vectorized_dims(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  std::vector<int64_t> bias_strides = bias_descriptor.vectorized_strides(
      dnn::DataLayout::kBatchDepthYX, vector_size, vector_dim);
  TF_ASSIGN_OR_RETURN(auto tensor_b,
                      CreateCudnnTensor(bias_dims, bias_strides, 'b', bias_type,
                                        vector_size, vector_dim));

  std::tie(vector_size, vector_dim) =
      GetTensorVectorSizeAndDim(output_descriptor, output_type);
  TF_ASSIGN_OR_RETURN(
      auto tensor_conv,
      CreateCudnnTensor(output_dims, output_strides, 'C', accumulator_type,
                        vector_size, vector_dim, /*is_virtual=*/true));

  TF_ASSIGN_OR_RETURN(
      auto tensor_add,
      CreateCudnnTensor(output_dims, output_strides, 'A', activation_type,
                        vector_size, vector_dim, /*is_virtual=*/true));

  TF_ASSIGN_OR_RETURN(
      auto tensor_bias,
      CreateCudnnTensor(output_dims, output_strides, 'B', activation_type,
                        vector_size, vector_dim, /*is_virtual=*/true));

  // conv_desc.
  auto mode = convolution_descriptor.convolution_not_crosscorr()
                  ? CUDNN_CONVOLUTION
                  : CUDNN_CROSS_CORRELATION;

  int conv_dim = convolution_descriptor.ndims();

  CHECK_NE(convolution_descriptor.pad_alignment(),
           dnn::PadAlignment::kTensorFlowPadding)
      << "TensorFlow padding alignment is not supported.";

  cudnnDataType_t cudnn_convolution_type = ToCudnnDataType(accumulator_type);
  cudnnDataType_t cudnn_activation_type = ToCudnnDataType(activation_type);
  auto conv_desc =
      cudnn_frontend::ConvDescBuilder()
          .setComputePrecision(cudnn_convolution_type)
          .setMathMode(mode)
          .setNDims(conv_dim)
          .setStrides(conv_dim, convolution_descriptor.strides().data())
          .setPrePadding(conv_dim, convolution_descriptor.padding().data())
          .setPostPadding(conv_dim, convolution_descriptor.padding().data())
          .setDilation(conv_dim, convolution_descriptor.dilations().data())
          .build();
  RETURN_MSG_IF_CUDNN_ERROR(conv_desc);

  // CUDNN Operation
  auto conv_op = cudnn_frontend::OperationBuilder(conv_mode)
                     .setxDesc(tensor_x)
                     .setyDesc(tensor_conv)
                     .setwDesc(tensor_w)
                     .setcDesc(conv_desc)
                     .setAlpha(1.0f)
                     .setBeta(0.0f)
                     .build();
  RETURN_MSG_IF_CUDNN_ERROR(conv_op);

  auto add_desc = cudnn_frontend::PointWiseDescBuilder()
                      .setMode(CUDNN_POINTWISE_ADD)
                      .setMathPrecision(cudnn_activation_type)
                      .build();
  auto add_op = cudnn_frontend::OperationBuilder(
                    CUDNN_BACKEND_OPERATION_POINTWISE_DESCRIPTOR)
                    .setxDesc(conv_op.getOutputTensor())
                    .setbDesc(tensor_z)
                    .setyDesc(tensor_add)
                    .setpwDesc(add_desc)
                    .setAlpha(alpha)
                    .setAlpha2(alpha2)
                    .build();
  RETURN_MSG_IF_CUDNN_ERROR(add_op);

  auto bias_add_desc = cudnn_frontend::PointWiseDescBuilder()
                           .setMode(CUDNN_POINTWISE_ADD)
                           .setMathPrecision(cudnn_activation_type)
                           .build();

  // If the activation is the identity function, then the bias-add is the last
  // op, and it writes to the output, tensor_y.  Otherwise, it writes to the
  // "virtual tensor" (temp buffer) tensor_bias, to which we apply the
  // activation.
  auto& bias_out_desc =
      activation_mode == dnn::ActivationMode::kNone ? tensor_y : tensor_bias;
  auto bias_add_op = cudnn_frontend::OperationBuilder(
                         CUDNN_BACKEND_OPERATION_POINTWISE_DESCRIPTOR)
                         .setxDesc(add_op.getOutputTensor())
                         .setbDesc(tensor_b)
                         .setyDesc(bias_out_desc)
                         .setpwDesc(bias_add_desc)
                         .build();
  RETURN_MSG_IF_CUDNN_ERROR(bias_add_op);

  // CUDNN OperationGraph
  absl::InlinedVector<cudnn_frontend::Operation const*, 4> ops = {
      &conv_op, &add_op, &bias_add_op};

  absl::optional<cudnn_frontend::PointWiseDesc_v8> act_desc;
  absl::optional<cudnn_frontend::Operation_v8> act_op;
  switch (activation_mode) {
    case dnn::ActivationMode::kNone:
      break;
    case dnn::ActivationMode::kRelu:
      act_desc.emplace(cudnn_frontend::PointWiseDescBuilder()
                           .setMode(CUDNN_POINTWISE_RELU_FWD)
                           .setMathPrecision(cudnn_activation_type)
                           .build());
      RETURN_MSG_IF_CUDNN_ERROR(*act_desc);
      act_op.emplace(cudnn_frontend::OperationBuilder(
                         CUDNN_BACKEND_OPERATION_POINTWISE_DESCRIPTOR)
                         .setxDesc(bias_add_op.getOutputTensor())
                         .setyDesc(tensor_y)
                         .setpwDesc(*act_desc)
                         .build());
      RETURN_MSG_IF_CUDNN_ERROR(*act_op);
      ops.push_back(&*act_op);
      break;
    default:
      return port::InternalError(
          absl::StrCat("Unimplemented activation mode ",
                       dnn::ActivationModeString(activation_mode)));
  }

  auto op_graph = cudnn_frontend::OperationGraphBuilder()
                      .setHandle(cudnn.handle())
                      .setOperationGraph(ops.size(), ops.data())
                      .build();
  RETURN_MSG_IF_CUDNN_ERROR(op_graph);

  VLOG(4) << "\nTensor_x: " << tensor_x.describe()
          << "\nTensor_y: " << tensor_y.describe()
          << "\nTensor_z: " << tensor_z.describe()
          << "\nTensor_w: " << tensor_w.describe()
          << "\nTensor_b: " << tensor_b.describe()
          << "\nTensor_conv: " << tensor_conv.describe()
          << "\nTensor_add: " << tensor_add.describe()
          << "\nTensor_bias: " << tensor_bias.describe()
          << "\nConv: " << conv_desc.describe()
          << "\nAdd: " << add_desc.describe()
          << "\nBiasAdd: " << bias_add_desc.describe()  //
          << "\nAct: "
          << (act_desc.has_value() ? act_desc->describe() : "(identity)")
          << "\nConvOp: " << conv_op.describe()
          << "\nAddOp: " << add_op.describe()
          << "\nBiasAddOp: " << bias_add_op.describe()  //
          << "\nActOp: "
          << (act_op.has_value() ? act_op->describe() : "(identity)")
          << "\nOpGraph: " << op_graph.describe();

  return std::unique_ptr<cudnn_frontend::OperationGraph>(
      new cudnn_frontend::OperationGraph(std::move(op_graph)));
}

}  // namespace

static port::StatusOr<cudnn_frontend::ExecutionPlan> RebuildExecutionPlan(
    const CudnnHandle& cudnn, const dnn::AlgorithmDesc& desc,
    const cudnn_frontend::OperationGraph& op_graph) {
  if (!desc.is_cudnn_frontend()) {
    return port::InternalError(
        "Got legacy cuDNN algorithm enum in RebuildExecutionPlan.");
  }

  // Errors encountered when building a cuDNN operation graph are surfaced in an
  // unprecedented and innovative way: they're written into a field of the
  // contained engine object, but then clobbered by the object's move
  // constructor which makes more cuDNN API calls and encounters further errors.
  // The only way to get the actual errors is to peek at them via the returned
  // rvalue reference before actually moving the object to finish its
  // initialization.
  cudnn_frontend::EngineBuilder engine_builder;
  engine_builder.setOperationGraph(op_graph).setGlobalEngineIdx(desc.algo_id());
  auto&& unmoved = engine_builder.build();
  RETURN_MSG_IF_CUDNN_ERROR(unmoved);
  cudnn_frontend::Engine engine = std::move(unmoved);
  RETURN_MSG_IF_CUDNN_ERROR(engine);

  // Miscellaneous compiler bugs and linker issues conspired to make it
  // impossible for AlgorithmDesc to just give us a map initially.  Get the
  // vector of tuning knobs and build the map locally.
  auto tuning_knobs_vec = desc.TuningKnobs();
  absl::flat_hash_map<int64_t, int64_t> tuning_knobs;
  tuning_knobs.reserve(tuning_knobs_vec.size());
  for (const auto& pair : tuning_knobs_vec) {
    tuning_knobs[pair.first] = pair.second;
  }

  for (auto& knob : engine.getSupportedKnobs()) {
    const auto it = tuning_knobs.find(static_cast<int64_t>(knob.getKnobType()));
    if (it != tuning_knobs.end()) {
      knob.setChoice(it->second);
    }
  }

  auto engine_config =
      cudnn_frontend::EngineConfigBuilder().setEngine(engine).build();
  RETURN_MSG_IF_CUDNN_ERROR(engine_config);

  auto plan = cudnn_frontend::ExecutionPlanBuilder()
                  .setHandle(cudnn.handle())
                  .setEngineConfig(engine_config)
                  .build();
  RETURN_MSG_IF_CUDNN_ERROR(plan);

  return {std::move(plan)};
}

#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

}  // namespace

port::Status CudnnSupport::DoPrepareForConvolution(
    dnn::ConvolutionKind kind, dnn::DataType element_type, Stream* stream,
    const dnn::BatchDescriptor& input_descriptor, DeviceMemoryBase input_data,
    const dnn::FilterDescriptor& filter_descriptor,
    DeviceMemoryBase filter_data, const dnn::BatchDescriptor& output_descriptor,
    DeviceMemoryBase output_data,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    const dnn::AlgorithmConfig& algorithm_config,
    ScratchAllocator* scratch_allocator, dnn::AlgorithmDesc* algorithm_desc,
    DeviceMemory<uint8>* scratch_memory) {
  CudnnTensorDescriptor input_nd(
      input_descriptor,
      ToCudnnDataType(element_type, input_descriptor.layout()));
  CudnnFilterDescriptor filter_nd(
      filter_descriptor,
      ToCudnnDataType(element_type, filter_descriptor.layout()));
  CudnnTensorDescriptor output_nd(
      output_descriptor,
      ToCudnnDataType(element_type, output_descriptor.layout()));

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  switch (kind) {
    case dnn::ConvolutionKind::FORWARD: {
      SE_ASSIGN_OR_RETURN(*algorithm_desc,
                          GetCudnnConvolutionForwardAlgorithm(
                              stream, cudnn, algorithm_config, input_nd,
                              filter_nd, element_type, convolution_descriptor,
                              output_nd, scratch_allocator, scratch_memory));
      break;
    }
    case dnn::ConvolutionKind::BACKWARD_DATA: {
      SE_ASSIGN_OR_RETURN(*algorithm_desc,
                          GetCudnnConvolutionBackwardDataAlgorithm(
                              stream, cudnn, algorithm_config, input_nd,
                              filter_nd, element_type, convolution_descriptor,
                              output_nd, scratch_allocator, scratch_memory));
      break;
    }
    case dnn::ConvolutionKind::BACKWARD_FILTER: {
      SE_ASSIGN_OR_RETURN(*algorithm_desc,
                          GetCudnnConvolutionBackwardFilterAlgorithm(
                              stream, cudnn, algorithm_config, input_nd,
                              filter_nd, element_type, convolution_descriptor,
                              output_nd, scratch_allocator, scratch_memory));
      break;
    }
    default:
      return port::InternalError(
          absl::StrCat("Unexpected convolution kind ", static_cast<int>(kind)));
  }

  return port::Status::OK();
}

class CudnnLegacyConvRunner : public dnn::ConvRunner {
 public:
  // Queries the workspace size and constructs a 'CudnnLegacyConvRunner'.
  static port::StatusOr<CudnnLegacyConvRunner> Create(
      GpuExecutor* parent, Stream* stream, CudnnAccess* cudnn,
      const dnn::AlgorithmDesc& algo, dnn::DataType input_type,
      dnn::DataType output_type, dnn::ConvolutionKind kind,
      CudnnTensorDescriptor input_nd, CudnnTensorDescriptor output_nd,
      CudnnFilterDescriptor filter, CudnnConvolutionDescriptor conv) {
    size_t workspace_size;
    if (algo.workspace_size()) {
      workspace_size = *algo.workspace_size();
    } else {
      // For old AlgorithmProtos loaded from serialized autotune maps and for
      // AlgorithmDescs constructed by manually specifying an algorithm ID, we
      // need to compute the workspace size here.
      auto handle = cudnn->GetHandle(parent, stream);

      switch (kind) {
        case dnn::ConvolutionKind::FORWARD:
          RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionForwardWorkspaceSize(
              handle.handle(),
              /*xDesc=*/input_nd.handle(),
              /*wDesc=*/filter.handle(), /*convDesc=*/conv.handle(),
              /*yDesc=*/output_nd.handle(),
              /*algo=*/ToConvForwardAlgo(algo),
              /*sizeInBytes=*/&workspace_size));
          break;
        case dnn::ConvolutionKind::BACKWARD_FILTER:
          RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardFilterWorkspaceSize(
              handle.handle(),
              /*xDesc=*/input_nd.handle(),
              /*dyDesc=*/output_nd.handle(),
              /*convDesc=*/conv.handle(),
              /*gradDesc=*/filter.handle(),
              /*algo=*/ToConvBackwardFilterAlgo(algo),
              /*sizeInBytes=*/&workspace_size));
          break;
        case dnn::ConvolutionKind::BACKWARD_DATA:
          RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionBackwardDataWorkspaceSize(
              handle.handle(),
              /*wDesc=*/filter.handle(),
              /*dyDesc=*/output_nd.handle(),
              /*convDesc=*/conv.handle(),
              /*dxDesc=*/input_nd.handle(),
              /*algo=*/ToConvBackwardDataAlgo(algo),
              /*sizeInBytes=*/&workspace_size));
          break;
        default:
          return port::InternalError(
              "Invalid ConvolutionKind for CudnnLegacyConvRunner.");
      }
    }

    return {{parent, cudnn, algo.algo_id(), algo.tensor_ops_enabled(),
             workspace_size, input_type, output_type, kind, std::move(input_nd),
             std::move(output_nd), std::move(filter), std::move(conv)}};
  }

  std::string ToString() const override {
    return MakeAlgorithmDesc().ToString();
  }

  size_t GetWorkspaceSize() const override { return workspace_size_; }

  port::StatusOr<dnn::AlgorithmDesc> ToAlgorithmDesc() const override {
    return MakeAlgorithmDesc();
  }

  port::Status operator()(Stream* stream, dnn::ProfileResult* profile_result,
                          DeviceMemoryBase scratch_memory,
                          DeviceMemoryBase input_data,
                          DeviceMemoryBase filter_data,
                          DeviceMemoryBase output_data) const override {
    auto algo = MakeAlgorithmDesc();

    // Check that the current stream supports tensor ops if they're requested.
    SE_RETURN_IF_ERROR(UseTensorOps(stream, input_type_, algo).status());

    if (static_cast<internal::StreamExecutorInterface*>(parent_) !=
        stream->parent()->implementation()) {
      return port::InternalError(
          "CudnnLegacyConvRunner cached across multiple StreamExecutors.");
    }

    auto cudnn = cudnn_->GetHandle(parent_, stream);
    // Alpha is the scaling factor for input.
    float falpha = 1.0;
    double dalpha = 1.0;
    void* alpha = input_type_ == dnn::DataType::kDouble
                      ? static_cast<void*>(&dalpha)
                      : static_cast<void*>(&falpha);
    // Beta is the scaling factor for output.
    float fbeta = 0.0;
    double dbeta = 0.0;
    void* beta = input_type_ == dnn::DataType::kDouble
                     ? static_cast<void*>(&dbeta)
                     : static_cast<void*>(&fbeta);

    const bool is_profiling = profile_result != nullptr;

    std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
    if (is_profiling) {
      timer.reset(new GpuTimer(parent_));  // NOLINT
      // The start and stop of the timer should be as close to the Cudnn call as
      // possible. It is still possible for other threads to issue workload on
      // to this stream. So it could take multiple profiling measurements.
      if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to start timer");
      }
    }

    const auto get_fwd_bugs = [&]() -> port::Status {
#if CUDNN_VERSION < 8000
      if (algo_id_ == CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM &&
          ToCudnnDataType(input_type_) == CUDNN_DATA_INT8 &&
          ToCudnnDataType(output_type_) == CUDNN_DATA_FLOAT) {
        return port::Status(
            port::error::FAILED_PRECONDITION,
            "This configuration potentially produces incorrect results.");
      }
#else
      (void)output_type_;  // To stop clang-tidy saying it's unused.
#endif
      return port::Status::OK();
    };

    auto get_bwd_data_bugs = [&]() -> port::Status {
      return port::Status::OK();
    };

    const auto get_bwd_filter_bugs = [&]() -> port::Status {
      return port::Status::OK();
    };

    switch (kind_) {
      case dnn::ConvolutionKind::FORWARD: {
        SE_RETURN_IF_ERROR(get_fwd_bugs());
        RETURN_IF_CUDNN_ERROR(cudnnConvolutionForward(
            cudnn.handle(),
            /*alpha=*/alpha, /*srcDesc=*/input_nd_.handle(),
            /*srcData=*/input_data.opaque(), /*filterDesc=*/filter_.handle(),
            /*filterData=*/filter_data.opaque(), /*convDesc=*/conv_.handle(),
            /*algo=*/ToConvForwardAlgo(algo),
            /*workSpace=*/scratch_memory.opaque(),
            /*workSpaceSizeInBytes=*/scratch_memory.size(), /*beta=*/beta,
            /*yDesc=*/output_nd_.handle(), /*y=*/output_data.opaque()));
        break;
      }
      case dnn::ConvolutionKind::BACKWARD_DATA: {
        SE_RETURN_IF_ERROR(get_bwd_data_bugs());
        RETURN_IF_CUDNN_ERROR(cudnnConvolutionBackwardData(
            cudnn.handle(),
            /*alpha=*/alpha,
            /*wDesc=*/filter_.handle(),
            /*w=*/filter_data.opaque(),
            /*dyDesc=*/output_nd_.handle(),
            /*dy=*/output_data.opaque(),
            /*convDesc=*/conv_.handle(),
            /*algo=*/ToConvBackwardDataAlgo(algo),
            /*workSpace=*/scratch_memory.opaque(),
            /*workSpaceSizeInBytes=*/scratch_memory.size(),
            /*beta=*/beta,
            /*dxDesc=*/input_nd_.handle(),
            /*dx=*/input_data.opaque()));
        break;
      }
      case dnn::ConvolutionKind::BACKWARD_FILTER: {
        SE_RETURN_IF_ERROR(get_bwd_filter_bugs());
        RETURN_IF_CUDNN_ERROR(cudnnConvolutionBackwardFilter(
            cudnn.handle(),
            /*alpha=*/alpha,
            /*srcDesc=*/input_nd_.handle(),
            /*srcData=*/input_data.opaque(),
            /*diffDesc=*/output_nd_.handle(),
            /*diffData=*/output_data.opaque(),
            /*convDesc=*/conv_.handle(),
            /*algo=*/ToConvBackwardFilterAlgo(algo),
            /*workSpace=*/scratch_memory.opaque(),
            /*workSpaceSizeInBytes=*/scratch_memory.size(),
            /*beta=*/beta,
            /*gradDesc=*/filter_.handle(),
            /*dw=*/filter_data.opaque()));
        break;
      }
      default:
        return port::InternalError(absl::StrCat("Unexpected convolution kind ",
                                                static_cast<int>(kind_)));
    }

    if (is_profiling) {
      if (!timer->Stop(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to stop timer");
      }
      profile_result->set_algorithm(algo);
      profile_result->set_elapsed_time_in_ms(timer->GetElapsedMilliseconds());
      profile_result->set_scratch_size(scratch_memory.size());
    }

    return port::Status::OK();
  }

 private:
  // Private to prevent passing in the wrong workspace_size.
  CudnnLegacyConvRunner(GpuExecutor* parent, CudnnAccess* cudnn,
                        int64_t algo_id, bool tensor_ops_enabled,
                        size_t workspace_size, dnn::DataType input_type,
                        dnn::DataType output_type, dnn::ConvolutionKind kind,
                        CudnnTensorDescriptor input_nd,
                        CudnnTensorDescriptor output_nd,
                        CudnnFilterDescriptor filter,
                        CudnnConvolutionDescriptor conv)
      : parent_(parent),
        cudnn_(cudnn),
        algo_id_(algo_id),
        tensor_ops_enabled_(tensor_ops_enabled),
        workspace_size_(workspace_size),
        kind_(kind),
        input_type_(input_type),
        output_type_(output_type),
        input_nd_(std::move(input_nd)),
        output_nd_(std::move(output_nd)),
        filter_(std::move(filter)),
        conv_(std::move(conv)) {}

  // Internal form of ToAlgorithmDesc without the StatusOr.
  dnn::AlgorithmDesc MakeAlgorithmDesc() const {
    return {algo_id_, tensor_ops_enabled_, workspace_size_};
  }

  GpuExecutor* parent_;
  CudnnAccess* cudnn_;
  int64_t algo_id_;
  bool tensor_ops_enabled_;
  size_t workspace_size_;
  dnn::ConvolutionKind kind_;
  dnn::DataType input_type_;
  dnn::DataType output_type_;

  CudnnTensorDescriptor input_nd_;
  CudnnTensorDescriptor output_nd_;
  CudnnFilterDescriptor filter_;
  CudnnConvolutionDescriptor conv_;
};

port::Status CudnnSupport::DoConvolve(
    dnn::ConvolutionKind kind, dnn::DataType element_type,
    dnn::DataType output_type, Stream* stream,
    const dnn::BatchDescriptor& input_descriptor, DeviceMemoryBase input_data,
    const dnn::FilterDescriptor& filter_descriptor,
    DeviceMemoryBase filter_data, const dnn::BatchDescriptor& output_descriptor,
    DeviceMemoryBase output_data,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    dnn::AlgorithmDesc algorithm_desc, DeviceMemory<uint8> scratch_memory,
    dnn::ProfileResult* profile_result) {
  cudnnDataType_t cudnn_type =
      ToCudnnDataType(element_type, input_descriptor.layout());
  CudnnTensorDescriptor input_nd(input_descriptor, cudnn_type);
  CudnnTensorDescriptor output_nd(
      output_descriptor,
      ToCudnnDataType(output_type, output_descriptor.layout()));
  CudnnFilterDescriptor filter_nd(
      filter_descriptor,
      ToCudnnDataType(element_type, filter_descriptor.layout()));

  auto accumulator_type = GetConvAccumulatorType(element_type);
  CudnnConvolutionDescriptor conv(convolution_descriptor,
                                  ToCudnnDataType(accumulator_type));
  SE_ASSIGN_OR_RETURN(bool use_tensor_ops,
                      UseTensorOps(stream, element_type, algorithm_desc));
  conv.set_use_tensor_op_math(use_tensor_ops);

  SE_ASSIGN_OR_RETURN(
      auto runner,
      CudnnLegacyConvRunner::Create(
          parent_, stream, cudnn_.get(), algorithm_desc, element_type,
          output_type, kind, std::move(input_nd), std::move(output_nd),
          std::move(filter_nd), std::move(conv)));
  return runner(stream, profile_result, scratch_memory, input_data, filter_data,
                output_data);
}

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

struct BackendDescriptorDeleter {
  void operator()(cudnnBackendDescriptor_t desc) {
    cudnnBackendDestroyDescriptor(desc);
  }
};

using BackendDescriptor = std::unique_ptr<void, BackendDescriptorDeleter>;

port::StatusOr<BackendDescriptor> CreateBackendDesc(
    cudnnBackendDescriptorType_t type) {
  void* result;
  RETURN_IF_CUDNN_ERROR(cudnnBackendCreateDescriptor(type, &result));
  return BackendDescriptor(result);
}

// Get the values of a CUDNN_TYPE_BACKEND_DESCRIPTOR attribute as a vector.
//
// This is fetching the entirety of a single sequence-valued attribute, as
// opposed to a sequence of multiple attributes.  The distinction is a bit
// meaningless, but this is the presentation the cuDNN docs use, so it may as
// well be consistent.
port::StatusOr<std::vector<BackendDescriptor>> GetDescriptorAttribute(
    cudnnBackendDescriptor_t desc, cudnnBackendAttributeName_t name,
    cudnnBackendDescriptorType_t type) {
  int64_t n;
  RETURN_IF_CUDNN_ERROR(cudnnBackendGetAttribute(
      desc, name, CUDNN_TYPE_BACKEND_DESCRIPTOR, 0, &n, nullptr));

  std::vector<BackendDescriptor> result(n);
  for (int i = 0; i < n; ++i) {
    SE_ASSIGN_OR_RETURN(result[i], CreateBackendDesc(type));
  }

  std::vector<cudnnBackendDescriptor_t> raw_ptrs;
  raw_ptrs.reserve(result.size());
  absl::c_transform(result, std::back_inserter(raw_ptrs),
                    [](const BackendDescriptor& ptr) { return ptr.get(); });

  // This API evidently does a deep copy of the descriptors into the pointers in
  // the output array, rather than writing pointers to the descriptors into the
  // output array.  So, this writes the memory behind each BackendDescriptor in
  // result, rather than writing the contents of raw_ptrs.
  RETURN_IF_CUDNN_ERROR(cudnnBackendGetAttribute(
      desc, name, CUDNN_TYPE_BACKEND_DESCRIPTOR, n, &n, raw_ptrs.data()));

  return result;
}

// Extract the engine ID and tuning knobs from the ExecutionPlan, and return
// them in the form of an AlgorithmDesc for use with RebuildExecutionPlan.
port::StatusOr<dnn::AlgorithmDesc> ExecutionPlanToAlgorithmDesc(
    const cudnn_frontend::ExecutionPlan& plan, size_t workspace_size) {
  SE_ASSIGN_OR_RETURN(
      auto engine_cfgs,
      GetDescriptorAttribute(plan.get_raw_desc(),
                             CUDNN_ATTR_EXECUTION_PLAN_ENGINE_CONFIG,
                             CUDNN_BACKEND_ENGINECFG_DESCRIPTOR));
  if (engine_cfgs.size() != 1) {
    return port::InternalError(
        "CUDNN_ATTR_EXECUTION_PLAN_ENGINE_CONFIG had more than one element.");
  }

  SE_ASSIGN_OR_RETURN(
      auto engines,
      GetDescriptorAttribute(engine_cfgs[0].get(), CUDNN_ATTR_ENGINECFG_ENGINE,
                             CUDNN_BACKEND_ENGINE_DESCRIPTOR));
  if (engines.size() != 1) {
    return port::InternalError(
        "CUDNN_ATTR_ENGINECFG_ENGINE had more than one element.");
  }

  int64_t n;
  int64_t engine_id;
  RETURN_IF_CUDNN_ERROR(
      cudnnBackendGetAttribute(engines[0].get(), CUDNN_ATTR_ENGINE_GLOBAL_INDEX,
                               CUDNN_TYPE_INT64, 1, &n, &engine_id));

  // Apparently for CUDNN_ATTR_ENGINECFG_KNOB_CHOICES only, trying to query the
  // number of elements in the attribute by using an output limit value of 0
  // just returns 0; the only way to find out how many there are is to
  // pre-allocate space for every existing knob type (as an upper bound on the
  // number of knob choices a config can have), and then look back at how many
  // were filled.
  std::vector<BackendDescriptor> knobs(CUDNN_KNOB_TYPE_COUNTS);
  for (int i = 0; i < knobs.size(); ++i) {
    SE_ASSIGN_OR_RETURN(
        knobs[i], CreateBackendDesc(CUDNN_BACKEND_KNOB_CHOICE_DESCRIPTOR));
  }
  std::vector<cudnnBackendDescriptor_t> raw_knob_ptrs;
  raw_knob_ptrs.reserve(knobs.size());
  absl::c_transform(knobs, std::back_inserter(raw_knob_ptrs),
                    [](const BackendDescriptor& ptr) { return ptr.get(); });
  RETURN_IF_CUDNN_ERROR(cudnnBackendGetAttribute(
      engine_cfgs[0].get(), CUDNN_ATTR_ENGINECFG_KNOB_CHOICES,
      CUDNN_TYPE_BACKEND_DESCRIPTOR, raw_knob_ptrs.size(), &n,
      raw_knob_ptrs.data()));
  knobs.resize(n);

  absl::flat_hash_map<int64_t, int64_t> tuning_knobs;
  for (const auto& knob : knobs) {
    cudnnBackendKnobType_t knob_type;
    int64_t knob_value;

    RETURN_IF_CUDNN_ERROR(
        cudnnBackendGetAttribute(knob.get(), CUDNN_ATTR_KNOB_CHOICE_KNOB_TYPE,
                                 CUDNN_TYPE_KNOB_TYPE, 1, &n, &knob_type));
    if (n != 1) {
      return port::InternalError(
          absl::StrCat("Knob should have exactly one KNOB_TYPE; had ", n));
    }

    RETURN_IF_CUDNN_ERROR(
        cudnnBackendGetAttribute(knob.get(), CUDNN_ATTR_KNOB_CHOICE_KNOB_VALUE,
                                 CUDNN_TYPE_INT64, 1, &n, &knob_value));
    if (n != 1) {
      return port::InternalError(
          absl::StrCat("Knob should have exactly one KNOB_VALUE; had ", n));
    }

    auto emplaced = tuning_knobs.try_emplace(knob_type, knob_value).second;
    if (!emplaced) {
      return port::InternalError(absl::StrFormat(
          "cuDNN gave multiple knob values for the same knob type.\n"
          "  KNOB_TYPE: %d\n"
          "  new KNOB_VALUE: %d\n"
          "  old KNOB_VALUE: %d",
          knob_type, knob_value, tuning_knobs.at(knob_type)));
    }
  }

  std::vector<std::pair<int64_t, int64_t>> tuning_knobs_vec;
  tuning_knobs_vec.reserve(tuning_knobs.size());
  absl::c_copy(tuning_knobs, std::back_inserter(tuning_knobs_vec));

  return dnn::AlgorithmDesc(engine_id, tuning_knobs_vec, workspace_size);
}

template <typename Sig>
class CudnnExecutionPlanRunner;
// An OpRunner implemented by an ExecutionPlan.
//
// This is the class holding the implementation of ToString, GetWorkspaceSize,
// and operator() for use by the cudnn frontend op runners.
template <typename... Args>
class CudnnExecutionPlanRunner<void(Args...)>
    : public dnn::OpRunner<void(Args...)> {
 public:
  std::string ToString() const override { return plan_.getTag(); }

  size_t GetWorkspaceSize() const override { return workspace_size_; }

  port::StatusOr<dnn::AlgorithmDesc> ToAlgorithmDesc() const override {
    return ExecutionPlanToAlgorithmDesc(plan_, workspace_size_);
  }

  port::Status operator()(Stream* stream, dnn::ProfileResult* profile_result,
                          DeviceMemoryBase scratch_memory,
                          Args... inputs) const override {
    if (static_cast<internal::StreamExecutorInterface*>(parent_) !=
        stream->parent()->implementation()) {
      return port::InternalError(
          "CudnnExecutionPlanRunner cached across multiple StreamExecutors.");
    }

    auto cudnn = cudnn_->GetHandle(parent_, stream);

    size_t workspace_size = plan_.getWorkspaceSize();
    RETURN_MSG_IF_CUDNN_ERROR(plan_);

    std::array<void*, sizeof...(Args)> data_ptrs = {inputs.opaque()...};

    // TODO(kaixih@nvidia): Remove the const_cast after cudnn frontend can take
    // in const int64 pointer.
    auto variantPack =
        cudnn_frontend::VariantPackBuilder()
            .setWorkspacePointer(scratch_memory.opaque())
            .setDataPointers(data_ptrs.size(), data_ptrs.data())
            .setUids(data_uids_.size(), const_cast<int64_t*>(data_uids_.data()))
            .build();
    RETURN_MSG_IF_CUDNN_ERROR(variantPack);

    VLOG(4) << "\nDo cudnn execution plan with plan tag: " << plan_.getTag()
            << "\nWorkspace size in bytes: " << workspace_size
            << "\nVariantPack: " << variantPack.describe();

    const bool is_profiling = profile_result != nullptr;

    std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
    if (is_profiling) {
      timer.reset(new GpuTimer(parent_));  // NOLINT
      // The start and stop of the timer should be as close to the Cudnn call as
      // possible. It is still possible for other threads to issue workload on
      // to this stream. So it could take multiple profiling measurements.
      if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to start timer");
      }
    }

    cudnnStatus_t status = cudnnBackendExecute(
        cudnn.handle(), plan_.get_raw_desc(), variantPack.get_raw_desc());
    RETURN_IF_CUDNN_ERROR(status);

    if (is_profiling) {
      if (!timer->Stop(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to stop timer");
      }
      SE_ASSIGN_OR_RETURN(auto desc, ToAlgorithmDesc());
      profile_result->set_algorithm(desc);
      profile_result->set_elapsed_time_in_ms(timer->GetElapsedMilliseconds());
      profile_result->set_scratch_size(scratch_memory.size());

      VLOG(4) << "cudnn op with plan " << plan_.getTag()
              << ", workspace_size=" << workspace_size << " -> "
              << CudnnStatusToString(status) << " in "
              << timer->GetElapsedMilliseconds() << "ms";
    }

    return port::Status::OK();
  }

  static port::StatusOr<CudnnExecutionPlanRunner> Create(
      GpuExecutor* parent, CudnnAccess* cudnn,
      cudnn_frontend::ExecutionPlan plan, absl::Span<const int64_t> uids) {
    auto workspace_size = static_cast<uint64_t>(plan.getWorkspaceSize());
    RETURN_MSG_IF_CUDNN_ERROR(plan);
    return {{parent, cudnn, std::move(plan), workspace_size, uids}};
  }

 private:
  CudnnExecutionPlanRunner(GpuExecutor* parent, CudnnAccess* cudnn,
                           cudnn_frontend::ExecutionPlan plan,
                           size_t workspace_size,
                           absl::Span<const int64_t> uids)
      : parent_(parent),
        cudnn_(cudnn),
        plan_(std::move(plan)),
        workspace_size_(workspace_size),
        data_uids_(uids.begin(), uids.end()) {}
  GpuExecutor* parent_;
  CudnnAccess* cudnn_;
  cudnn_frontend::ExecutionPlan plan_;
  size_t workspace_size_;
  absl::InlinedVector<int64_t, sizeof...(Args)> data_uids_;
};
#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

// Utility for dealing with CUDA's type-erased scaling parameters, where some
// sets of parameters expect a void* pointing at a float while others expect it
// to point at a double.
//
// This is rather ugly, but its purpose is to quarantine the corresponding
// ugliness that already exists in the CUDA API.
class ScalingParam {
 public:
  explicit ScalingParam(double value) : as_double_(value), as_float_(value) {}

  // Return a pointer to the appropriate representation type for the given
  // element type.
  //
  // See
  // https://docs.nvidia.com/deeplearning/cudnn/developer-guide/index.html#scaling-parameters
  // for more info; the behavior for int8 result tensors is not described there,
  // but is maintained from the existing behavior (namely, using a float scaling
  // parameter).
  void* ToVoidPointer(dnn::DataType element_type) {
    if (element_type == dnn::DataType::kDouble) {
      return &as_double_;
    } else {
      return &as_float_;
    }
  }

 private:
  double as_double_;
  float as_float_;
};

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
namespace {

template <typename Sig>
port::Status CreateOpRunners(
    Stream* stream, CudnnHandle& cudnn, GpuExecutor* gpu_executor,
    CudnnAccess* cudnn_access,
    std::unique_ptr<cudnn_frontend::OperationGraph> op_graph,
    dnn::ConvolutionKind kind, dnn::DataType input_type,
    absl::Span<const int64_t> input_uids, bool use_fallback,
    std::vector<std::unique_ptr<const dnn::OpRunner<Sig>>>* out_runners) {
  cudnn_frontend::EngineConfigList filtered_configs;
  auto generic_filter_fn = [=](cudnnBackendDescriptor_t engine_config) -> bool {
    return GenericEngineFilter(
        engine_config,
        /*disable_winograd*/ !CudnnEnvVar<WinogradNonfused>::IsEnabled(),
        /*disable_nondeterminism*/ RequireCudnnDeterminism(),
        /*disable_tensor_core*/ !IsTensorMathEnabled(stream, input_type));
  };

  if (!use_fallback) {
    auto heuristics = cudnn_frontend::EngineHeuristicsBuilder()
                          .setOperationGraph(*op_graph)
                          .setHeurMode(GetCudnnFrontendHeurMode())
                          .build();
    RETURN_MSG_IF_CUDNN_ERROR(heuristics);

    // cuDNN frontend sneakily puts error messages on the object and returns
    // partially-initialized results when there's an error; make sure to check
    // them.
    int64_t engine_count = heuristics.getEngineConfigCount();
    RETURN_MSG_IF_CUDNN_ERROR(heuristics);
    auto& heuristics_configs = heuristics.getEngineConfig(engine_count);
    RETURN_MSG_IF_CUDNN_ERROR(heuristics);
    VLOG(4) << "\nHeuristics engine configs size: "
            << heuristics_configs.size();

    cudnn_frontend::filter(heuristics_configs, filtered_configs,
                           generic_filter_fn);
  } else {
    auto fallback = cudnn_frontend::EngineFallbackListBuilder()
                        .setOperationGraph(*op_graph)
                        .setOperation(GetCudnnConvolutionType(kind))
                        .build();
    RETURN_MSG_IF_CUDNN_ERROR(fallback);

    auto& fallback_configs = fallback.getFallbackList();
    VLOG(4) << "\nFallback engine configs size: " << fallback_configs.size();

    cudnn_frontend::filter(fallback_configs, filtered_configs,
                           generic_filter_fn);
  }
  VLOG(4) << "\nFiltered engine configs size: " << filtered_configs.size();

  auto fn = []() { return true; };
  auto maybe_json_handle_static = CudnnExecutionPlanEngineFilterStatic();
  auto maybe_json_handle_runtime = CudnnExecutionPlanEngineFilterRuntime();

  out_runners->clear();
  for (int i = 0; i < filtered_configs.size(); i++) {
    auto plan = cudnn_frontend::ExecutionPlanBuilder()
                    .setHandle(cudnn.handle())
                    .setEngineConfig(filtered_configs[i], op_graph->getTag())
                    .build();
    if (plan.get_status() != CUDNN_STATUS_SUCCESS) {
      continue;
    }

    if (maybe_json_handle_static &&
        cudnn_frontend::check_errata(*maybe_json_handle_static, plan.getTag(),
                                     cudnn.handle(), fn)) {
      VLOG(4) << "Exclude engine (static): " << plan.getTag();
      continue;
    }
    if (maybe_json_handle_runtime &&
        cudnn_frontend::check_errata(*maybe_json_handle_runtime, plan.getTag(),
                                     cudnn.handle(), fn)) {
      VLOG(4) << "Exclude engine (runtime): " << plan.getTag();
      continue;
    }

    auto runner_or = CudnnExecutionPlanRunner<Sig>::Create(
        gpu_executor, cudnn_access, std::move(plan), input_uids);
    if (!runner_or.ok()) {
      // Note this can happen if cuDNN Frontend gives us partially-initialized
      // ExecutionPlans because its error handling is broken in non-exception
      // builds; those were meant to be filtered out earlier inside cuDNN
      // Frontend, but instead they get filtered out here.
      VLOG(4) << "Failed building runner from ExecutionPlan (i.e. failed "
                 "getting its workspace size): "
              << runner_or.status().ToString();
      continue;
    }

    out_runners->push_back(std::make_unique<CudnnExecutionPlanRunner<Sig>>(
        runner_or.ConsumeValueOrDie()));

    // We will use the first working plan when determinism is required.
    if (RequireCudnnDeterminism()) {
      break;
    }
  }

  VLOG(4) << "\nReturned execution plans size: " << out_runners->size();

  return port::Status::OK();
}

}  // namespace
#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND

port::Status CudnnSupport::GetConvolveRunners(
    bool use_cudnn_frontend, dnn::ConvolutionKind kind,
    dnn::DataType input_type, dnn::DataType output_type, Stream* stream,
    const dnn::BatchDescriptor& input_descriptor,
    DeviceMemoryBase /*input_data*/,
    const dnn::FilterDescriptor& filter_descriptor,
    DeviceMemoryBase /*filter_data*/,
    const dnn::BatchDescriptor& output_descriptor,
    DeviceMemoryBase /*output_data*/,
    const dnn::ConvolutionDescriptor& convolution_descriptor, bool use_fallback,
    ScratchAllocator* /*scratch_allocator*/,
    std::vector<std::unique_ptr<const dnn::ConvRunner>>* out_exec_plans) {
  // All current versions of the frontend API lack support for Tx32
  // convolutions.
  const bool is_unsupported_x32 =
      input_descriptor.layout() == dnn::kBatchDepthYX32;

  // cuDNN frontend support became sufficiently stable to use in 8.1.
  // TODO(awpr): remove this condition once support for cuDNN 8.0 is dropped.
  const bool is_pre_frontend_cudnn = CUDNN_VERSION < 8100;

  const bool actually_use_cudnn_frontend =
      use_cudnn_frontend && !is_pre_frontend_cudnn && !is_unsupported_x32;

  if (use_cudnn_frontend && !actually_use_cudnn_frontend) {
    // This will happen once per unique conv configuration/shape that gets
    // affected (and not, for example, on every conv launch).  Confusion over
    // whether this has happened or not has repeatedly wasted a lot of time
    // debugging, so be sure it shows up in the logs.
    LOG(INFO) << "Disabling cuDNN frontend for the following convolution:\n"
              << "  input: " << input_descriptor.ToString() << "\n"
              << "  filter: " << filter_descriptor.ToString() << "\n"
              << "  " << convolution_descriptor.ToString() << "\n"
              << "  ... because "
              << (is_unsupported_x32
                      ? "Tx32 convolutions are unsupported."
                      : "the current cuDNN version does not support it.");
  }

  if (!actually_use_cudnn_frontend) {
    auto cuda_compute_capability = stream->GetCudaComputeCapability();
    std::vector<dnn::AlgorithmDesc> algorithms;
    bool got_algos = false;
    switch (kind) {
      default:
        return port::InternalError(absl::StrFormat(
            "Unknown ConvolutionKind for unfused conv: %d", kind));
      case dnn::ConvolutionKind::FORWARD:
        got_algos = GetConvolveAlgorithms(cuda_compute_capability, &algorithms);
        break;
      case dnn::ConvolutionKind::BACKWARD_FILTER:
        got_algos = GetConvolveBackwardFilterAlgorithms(cuda_compute_capability,
                                                        &algorithms);
        break;
      case dnn::ConvolutionKind::BACKWARD_DATA:
        got_algos = GetConvolveBackwardDataAlgorithms(cuda_compute_capability,
                                                      &algorithms);
        break;
    }
    if (!got_algos) {
      return port::Status(
          port::error::UNKNOWN,
          absl::StrFormat("Listing algorithms failed for kind %d", kind));
    }

    for (const auto& algo : algorithms) {
      auto runner_or = ConvolveRunnerFromDesc(
          stream, algo, kind, input_type, output_type, input_descriptor,
          filter_descriptor, output_descriptor, convolution_descriptor);
      if (!runner_or.ok()) {
        // Failures here can result from trying to query the workspace size for
        // algorithms that aren't supported for the present configuration.  This
        // means we'll now return only supported algorithms, unlike the
        // predecessor 'GetConvolveAlgorithms', which returned all existing
        // algorithms regardless of any particular configuration.
        //
        // TODO(awpr): can we arrange for the expected errors here to have a
        // particular error code (e.g. UNIMPLEMENTED or INVALID_ARGUMENT) and
        // log errors for anything unexpected?
        continue;
      }
      out_exec_plans->push_back(runner_or.ConsumeValueOrDie());
    }

    return port::Status::OK();
  }

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  auto cudnn = cudnn_->GetHandle(parent_, stream);
  SE_ASSIGN_OR_RETURN(
      auto op_graph,
      GetCudnnOperationGraph(kind, input_type, output_type, input_descriptor,
                             filter_descriptor, output_descriptor,
                             convolution_descriptor, cudnn));

  return CreateOpRunners<dnn::ConvSignature>(
      stream, cudnn, parent_, cudnn_.get(), std::move(op_graph), kind,
      input_type, {'x', 'w', 'y'}, use_fallback, out_exec_plans);
#else
  return port::UnimplementedError(
      "Cudnn execution plans are only supported with Cudnn >= 8.1.");
#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
}

port::StatusOr<std::unique_ptr<const dnn::ConvRunner>>
CudnnSupport::ConvolveRunnerFromDesc(
    Stream* stream, const dnn::AlgorithmDesc& algorithm_desc,
    dnn::ConvolutionKind kind, dnn::DataType input_type,
    dnn::DataType output_type, const dnn::BatchDescriptor& input_descriptor,
    const dnn::FilterDescriptor& filter_descriptor,
    const dnn::BatchDescriptor& output_descriptor,
    const dnn::ConvolutionDescriptor& convolution_descriptor) {
  if (!algorithm_desc.is_cudnn_frontend()) {
    CudnnConvolutionDescriptor conv(
        convolution_descriptor,
        ToCudnnDataType(GetConvAccumulatorType(input_type)));
    conv.set_use_tensor_op_math(algorithm_desc.tensor_ops_enabled());

    SE_ASSIGN_OR_RETURN(
        auto runner,
        CudnnLegacyConvRunner::Create(
            parent_, stream, cudnn_.get(), algorithm_desc, input_type,
            output_type, kind,
            /* input_nd = */
            CudnnTensorDescriptor(
                input_descriptor,
                ToCudnnDataType(input_type, input_descriptor.layout())),
            /* output_nd = */
            CudnnTensorDescriptor(
                output_descriptor,
                ToCudnnDataType(output_type, output_descriptor.layout())),
            /* filter = */
            CudnnFilterDescriptor(
                filter_descriptor,
                ToCudnnDataType(input_type, filter_descriptor.layout())),
            std::move(conv)));

    return {std::make_unique<CudnnLegacyConvRunner>(std::move(runner))};
  }

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  auto cudnn = cudnn_->GetHandle(parent_, stream);

  SE_ASSIGN_OR_RETURN(
      auto op_graph,
      GetCudnnOperationGraph(kind, input_type, output_type, input_descriptor,
                             filter_descriptor, output_descriptor,
                             convolution_descriptor, cudnn));

  SE_ASSIGN_OR_RETURN(auto execution_plan,
                      RebuildExecutionPlan(cudnn, algorithm_desc, *op_graph));

  SE_ASSIGN_OR_RETURN(
      auto runner,
      CudnnExecutionPlanRunner<dnn::ConvSignature>::Create(
          parent_, cudnn_.get(), std::move(execution_plan), {'x', 'w', 'y'}));
  return {std::make_unique<CudnnExecutionPlanRunner<dnn::ConvSignature>>(
      std::move(runner))};
#else
  return port::UnimplementedError(
      "Cudnn execution plans are only supported with Cudnn >= 8.1.");
#endif
}

class CudnnLegacyFusedConvRunner : public dnn::FusedConvRunner {
 public:
  // Queries the workspace size and constructs a 'CudnnLegacyFusedConvRunner'.
  static port::StatusOr<CudnnLegacyFusedConvRunner> Create(
      GpuExecutor* parent, Stream* stream, CudnnAccess* cudnn,
      const dnn::AlgorithmDesc& algo, dnn::DataType input_type,
      double conv_scale, double side_input_scale,
      CudnnTensorDescriptor input_nd, CudnnTensorDescriptor output_nd,
      CudnnFilterDescriptor filter, CudnnTensorDescriptor bias_nd,
      CudnnConvolutionDescriptor conv,
      CudnnActivationDescriptor activation_desc) {
    size_t workspace_size;
    if (algo.workspace_size()) {
      workspace_size = *algo.workspace_size();
    } else {
      auto handle = cudnn->GetHandle(parent, stream);

      RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionForwardWorkspaceSize(
          handle.handle(),
          /*xDesc=*/input_nd.handle(),
          /*wDesc=*/filter.handle(), /*convDesc=*/conv.handle(),
          /*yDesc=*/output_nd.handle(),
          /*algo=*/ToConvForwardAlgo(algo),
          /*sizeInBytes=*/&workspace_size));
    }

    return {{parent, cudnn, algo.algo_id(), algo.tensor_ops_enabled(),
             workspace_size, input_type, conv_scale, side_input_scale,
             std::move(input_nd), std::move(output_nd), std::move(filter),
             std::move(bias_nd), std::move(conv), std::move(activation_desc)}};
  }

  std::string ToString() const override {
    return MakeAlgorithmDesc().ToString();
  }

  uint64_t GetWorkspaceSize() const override { return workspace_size_; }

  port::StatusOr<dnn::AlgorithmDesc> ToAlgorithmDesc() const override {
    return MakeAlgorithmDesc();
  }

  port::Status operator()(Stream* stream, dnn::ProfileResult* profile_result,
                          DeviceMemoryBase scratch_memory,
                          DeviceMemoryBase input_data,
                          DeviceMemoryBase filter_data,
                          DeviceMemoryBase side_input_data,
                          DeviceMemoryBase bias_data,
                          DeviceMemoryBase output_data) const override {
    if (static_cast<internal::StreamExecutorInterface*>(parent_) !=
        stream->parent()->implementation()) {
      return port::InternalError(
          "CudnnLegacyFusedConvRunner cached across multiple StreamExecutors.");
    }

    auto algo = MakeAlgorithmDesc();

    std::unique_ptr<GpuTimer, GpuTimerDeleter> timer;
    if (profile_result) {
      timer.reset(new GpuTimer(parent_));  // NOLINT
      // The start and stop of the timer should be as close to the Cudnn call as
      // possible. It is still possible for other threads to issue workload on
      // to this stream. So it could take multiple profiling measurements.
      if (!timer->Init() || !timer->Start(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to start timer");
      }
    }
    auto side_input_data_ptr = (side_input_scale_ == 0)
                                   ? output_data.opaque()
                                   : side_input_data.opaque();

    auto cudnn = cudnn_->GetHandle(parent_, stream);

    VLOG(2) << "\nconv_scale = " << conv_scale_
            << "\nconv_input_nd.handle() = " << input_nd_.handle()
            << "\nconv_input_data.opaque() = " << input_data.opaque()
            << "\nfilter.handle() = " << filter_.handle()
            << "\nfilter_data.opaque() = " << filter_data.opaque()
            << "\nconv.handle() = " << conv_.handle() << "\nalgo = " << algo_id_
            << ", tensor_ops_enabled=" << tensor_ops_enabled_
            << "\nscratch.opaque() = " << scratch_memory.opaque()
            << "\nscratch.size() = " << scratch_memory.size()
            << "\nside_input_scale = " << side_input_scale_
            << "\noutput_nd.handle() = " << output_nd_.handle()
            << "\nside_input_data_ptr = " << side_input_data_ptr
            << "\nbias_nd.handle() = " << bias_nd_.handle()
            << "\nbiases.opaque() = " << bias_data.opaque()
            << "\nactivation_desc.handle() = " << activation_desc_.handle()
            << "\noutput_nd.handle() = " << output_nd_.handle()
            << "\noutput_data.opaque() = " << output_data.opaque();

    if (IsTensorMathOpSet(conv_) != tensor_ops_enabled_) {
      return port::Status(port::error::FAILED_PRECONDITION,
                          "Tensor op math type in dnn::AlgorithmDesc does not "
                          "match that of the CudnnConvolutionDescriptor");
    }

    // N.B. the scaling parameters alpha1 and alpha2 are pointers to
    // temporaries; this API doesn't persist the pointers beyond its own stack
    // frame.
    auto status = cudnnConvolutionBiasActivationForward(
        cudnn.handle(),
        /*alpha1=*/ScalingParam(conv_scale_).ToVoidPointer(input_type_),
        /*xDesc=*/input_nd_.handle(), /*x=*/input_data.opaque(),
        /*wDesc=*/filter_.handle(), /*w=*/filter_data.opaque(),
        /*convDesc=*/conv_.handle(), ToConvForwardAlgo(algo),
        /*workSpace=*/scratch_memory.opaque(),
        /*workSpaceSizeInBytes=*/scratch_memory.size(),
        /*alpha2=*/ScalingParam(side_input_scale_).ToVoidPointer(input_type_),
        /*zDesc=*/output_nd_.handle(), /*z=*/side_input_data_ptr,
        /*biasDesc=*/bias_nd_.handle(), /*bias=*/bias_data.opaque(),
        /*activationDesc=*/activation_desc_.handle(),
        /*yDesc=*/output_nd_.handle(), /*y=*/output_data.opaque());
    if (status != CUDNN_STATUS_SUCCESS || !profile_result) {
      VLOG(4) << "conv with algorithm " << ToConvForwardAlgo(algo)
              << ", workspace_size=" << scratch_memory.size() << " -> "
              << CudnnStatusToString(status);
    }
    RETURN_IF_CUDNN_ERROR(status);

    if (profile_result) {
      if (!timer->Stop(AsGpuStream(stream))) {
        return port::Status(port::error::INTERNAL, "Failed to stop timer");
      }
      profile_result->set_algorithm(algo);
      profile_result->set_elapsed_time_in_ms(timer->GetElapsedMilliseconds());
      profile_result->set_scratch_size(scratch_memory.size());
      VLOG(4) << "conv with algorithm " << ToConvForwardAlgo(algo)
              << ", tensor_ops_enabled=" << tensor_ops_enabled_
              << ", workspace_size=" << scratch_memory.size() << " -> "
              << CudnnStatusToString(status) << " in "
              << timer->GetElapsedMilliseconds() << "ms";
    }

    return port::Status::OK();
  }

 private:
  // Private to prevent passing in the wrong workspace_size.
  CudnnLegacyFusedConvRunner(GpuExecutor* parent, CudnnAccess* cudnn,
                             int64_t algo_id, bool tensor_ops_enabled,
                             size_t workspace_size, dnn::DataType input_type,
                             double conv_scale, double side_input_scale,
                             CudnnTensorDescriptor input_nd,
                             CudnnTensorDescriptor output_nd,
                             CudnnFilterDescriptor filter,
                             CudnnTensorDescriptor bias_nd,
                             CudnnConvolutionDescriptor conv,
                             CudnnActivationDescriptor activation_desc)
      : parent_(parent),
        cudnn_(cudnn),
        algo_id_(algo_id),
        tensor_ops_enabled_(tensor_ops_enabled),
        workspace_size_(workspace_size),
        input_type_(input_type),
        conv_scale_(conv_scale),
        side_input_scale_(side_input_scale),
        input_nd_(std::move(input_nd)),
        output_nd_(std::move(output_nd)),
        filter_(std::move(filter)),
        bias_nd_(std::move(bias_nd)),
        conv_(std::move(conv)),
        activation_desc_(std::move(activation_desc)) {}

  // Internal form of ToAlgorithmDesc without the StatusOr.
  dnn::AlgorithmDesc MakeAlgorithmDesc() const {
    return {algo_id_, tensor_ops_enabled_, workspace_size_};
  }

  GpuExecutor* parent_;
  CudnnAccess* cudnn_;
  int64_t algo_id_;
  bool tensor_ops_enabled_;
  size_t workspace_size_;
  dnn::DataType input_type_;
  double conv_scale_, side_input_scale_;

  CudnnTensorDescriptor input_nd_;
  CudnnTensorDescriptor output_nd_;
  CudnnFilterDescriptor filter_;
  CudnnTensorDescriptor bias_nd_;
  CudnnConvolutionDescriptor conv_;
  CudnnActivationDescriptor activation_desc_;
};

port::StatusOr<std::unique_ptr<const dnn::FusedConvRunner>>
CudnnSupport::FusedConvolveRunnerFromDesc(
    Stream* stream, const dnn::AlgorithmDesc& algorithm_desc,
    dnn::ConvolutionKind kind, dnn::DataType input_type,
    dnn::DataType bias_type, dnn::DataType output_type, double conv_scale,
    double side_input_scale, const dnn::BatchDescriptor& input_descriptor,
    const dnn::FilterDescriptor& filter_descriptor,
    const dnn::BatchDescriptor& bias_descriptor,
    const dnn::BatchDescriptor& output_descriptor,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    dnn::ActivationMode activation_mode) {
  if (!algorithm_desc.is_cudnn_frontend()) {
    CudnnTensorDescriptor conv_input_nd(
        input_descriptor,
        ToCudnnDataType(input_type, input_descriptor.layout()));
    CudnnTensorDescriptor output_nd(
        output_descriptor,
        ToCudnnDataType(output_type, input_descriptor.layout()));
    CudnnFilterDescriptor filter(
        filter_descriptor,
        ToCudnnDataType(input_type, filter_descriptor.layout()));
    CudnnTensorDescriptor bias_nd(bias_descriptor, ToCudnnDataType(bias_type));

    CudnnConvolutionDescriptor conv(
        convolution_descriptor,
        ToCudnnDataType(GetConvAccumulatorType(input_type)));
    conv.set_use_tensor_op_math(algorithm_desc.tensor_ops_enabled());

    // CUDNN v6 only supports CUDNN_NOT_PROPAGATE_NAN as the reluNanOpt for
    // activation descriptor. Note that this will change the nan propagation
    // behavior from separate conv, bias, and relu (which by default is
    // CUDNN_PROPAGATE_NAN).
    //
    // TODO(awpr): reevaluate this for newer cuDNN versions.
    CudnnActivationDescriptor activation_desc(activation_mode,
                                              CUDNN_NOT_PROPAGATE_NAN,
                                              output_descriptor.value_max());

    SE_ASSIGN_OR_RETURN(
        auto runner,
        CudnnLegacyFusedConvRunner::Create(
            parent_, stream, cudnn_.get(), algorithm_desc, input_type,
            conv_scale, side_input_scale, std::move(conv_input_nd),
            std::move(output_nd), std::move(filter), std::move(bias_nd),
            std::move(conv), std::move(activation_desc)));
    return {std::make_unique<CudnnLegacyFusedConvRunner>(std::move(runner))};
  }

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  auto cudnn = cudnn_->GetHandle(parent_, stream);

  SE_ASSIGN_OR_RETURN(auto op_graph,
                      GetCudnnFusedOperationGraph(
                          kind, input_type, bias_type, output_type, conv_scale,
                          side_input_scale, input_descriptor, filter_descriptor,
                          bias_descriptor, output_descriptor,
                          convolution_descriptor, activation_mode, cudnn));

  SE_ASSIGN_OR_RETURN(auto execution_plan,
                      RebuildExecutionPlan(cudnn, algorithm_desc, *op_graph));

  SE_ASSIGN_OR_RETURN(auto runner,
                      CudnnExecutionPlanRunner<dnn::FusedConvSignature>::Create(
                          parent_, cudnn_.get(), std::move(execution_plan),
                          {'x', 'w', 'z', 'b', 'y'}));
  return {std::make_unique<CudnnExecutionPlanRunner<dnn::FusedConvSignature>>(
      std::move(runner))};
#else
  return port::UnimplementedError(
      "Cudnn execution plans are only supported with Cudnn >= 8.1.");
#endif
}

port::Status CudnnSupport::GetFusedConvolveRunners(
    bool use_cudnn_frontend, dnn::ConvolutionKind kind,
    dnn::DataType input_type, dnn::DataType bias_type,
    dnn::DataType output_type, double conv_scale, double side_input_scale,
    Stream* stream, const dnn::BatchDescriptor& input_descriptor,
    const dnn::FilterDescriptor& filter_descriptor,
    const dnn::BatchDescriptor& bias_descriptor,
    const dnn::BatchDescriptor& output_descriptor,
    const dnn::ConvolutionDescriptor& convolution_descriptor, bool use_fallback,
    const dnn::ActivationMode activation_mode,
    std::vector<std::unique_ptr<const dnn::FusedConvRunner>>* out_exec_plans) {
  // Fused convolutions with identity activations are broken in that they
  // implicitly do ReLU on some engines, and we can't reliably detect which
  // ones.
  const bool is_broken_identity_fused_conv =
#if CUDNN_VERSION < 8205
      activation_mode == dnn::ActivationMode::kNone;
#else
      false;
#endif

  // All current versions of the frontend API lack support for Tx32
  // convolutions.
  const bool is_unsupported_x32 =
      input_descriptor.layout() == dnn::kBatchDepthYX32;

  // cuDNN frontend support became sufficiently stable to use in 8.1.
  // TODO(awpr): remove this condition once support for cuDNN 8.0 is dropped.
  const bool is_pre_frontend_cudnn = CUDNN_VERSION < 8100;

  const bool actually_use_cudnn_frontend =
      use_cudnn_frontend && !is_pre_frontend_cudnn &&
      !is_broken_identity_fused_conv && !is_unsupported_x32;

  if (use_cudnn_frontend && !actually_use_cudnn_frontend) {
    const char* reason = "the current cuDNN version does not support it.";
    if (is_unsupported_x32) {
      reason = "Tx32 convolutions are unsupported.";
    } else if (is_broken_identity_fused_conv) {
      reason = "it uses an identity activation.";
    }

    // This will happen once per unique conv configuration/shape that gets
    // affected (and not, for example, on every conv launch).  Confusion over
    // whether this has happened or not has repeatedly wasted a lot of time
    // debugging, so be sure it shows up in the logs.
    LOG(INFO) << "Disabling cuDNN frontend for the following convolution:\n"
              << "  input: " << input_descriptor.ToString() << "\n"
              << "  filter: " << filter_descriptor.ToString() << "\n"
              << "  " << convolution_descriptor.ToString() << "\n"
              << "  ... because " << reason;
  }

  if (input_type == dnn::DataType::kInt8 &&
      !stream->GetCudaComputeCapability().IsAtLeast(6, 1)) {
    return port::UnimplementedError(
        "cudnnConvolutionBiasActivationForward() for int8 is only supported "
        "on GPUs with compute capability 6.1 or later.");
  }

  if (input_type == dnn::DataType::kInt8 &&
      output_type == dnn::DataType::kFloat &&
      (CUDNN_VERSION >= 8000 && CUDNN_VERSION <= 8200)) {
    return port::UnimplementedError(
        "int8 -> float fused conv is disabled for this cuDNN version. See "
        "go/nvbugs/3326122");
  }

  if (activation_mode != dnn::ActivationMode::kRelu &&
      activation_mode != dnn::ActivationMode::kNone) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "cudnnConvolutionBiasActivationForward() only supports "
                        "Relu or None activation.");
  }

  if (!actually_use_cudnn_frontend) {
    std::vector<dnn::AlgorithmDesc> algorithms;

    auto cuda_compute_capability = stream->GetCudaComputeCapability();
    if (!GetConvolveAlgorithms(cuda_compute_capability, &algorithms)) {
      return port::Status(port::error::UNKNOWN,
                          "Listing fused convolve algorithms failed.");
    }

    for (const auto& algo : algorithms) {
      // Only CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM is supported
      // for identity activation, other algs seem to quietly do Relu. See
      // https://docs.nvidia.com/deeplearning/sdk/cudnn-developer-guide/index.html#cudnnConvolutionBiasActivationForward
      if (activation_mode == dnn::ActivationMode::kNone &&
          algo.algo_id() != CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM) {
        continue;
      }
      auto runner_or = FusedConvolveRunnerFromDesc(
          stream, algo, kind, input_type, bias_type, output_type, conv_scale,
          side_input_scale, input_descriptor, filter_descriptor,
          bias_descriptor, output_descriptor, convolution_descriptor,
          activation_mode);
      if (!runner_or.ok()) {
        // See the corresponding error handling in
        // CudnnSupport::GetConvolveRunners: this filters out algorithms that
        // don't support this conv.
        continue;
      }
      out_exec_plans->push_back(runner_or.ConsumeValueOrDie());
    }
    return port::Status::OK();
  }

#if CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
  auto cudnn = cudnn_->GetHandle(parent_, stream);
  auto op_graph_status = GetCudnnFusedOperationGraph(
      kind, input_type, bias_type, output_type, conv_scale, side_input_scale,
      input_descriptor, filter_descriptor, bias_descriptor, output_descriptor,
      convolution_descriptor, activation_mode, cudnn);
  if (!op_graph_status.status().ok()) {
    return port::Status(port::error::INTERNAL,
                        absl::StrCat("Cudnn graph failed to build: ",
                                     op_graph_status.status().ToString()));
  }
  auto op_graph = op_graph_status.ConsumeValueOrDie();

  return CreateOpRunners<dnn::FusedConvSignature>(
      stream, cudnn, parent_, cudnn_.get(), std::move(op_graph), kind,
      input_type, {'x', 'w', 'z', 'b', 'y'}, use_fallback, out_exec_plans);
#else
  return port::UnimplementedError(
      "Cudnn execution plans are only supported with Cudnn >= 8.1.");
#endif  // CUDNN_VERSION >= 8100 && TF_ENABLE_CUDNN_FRONTEND
}

bool CudnnSupport::GetConvolveAlgorithms(
    CudaComputeCapability cuda_compute_capability,
    std::vector<dnn::AlgorithmDesc>* out_algorithms) {
  // Preload sub libs for cudnn 8.0.4+
#if CUDNN_MAJOR >= 8 && (CUDNN_MINOR > 0 || CUDNN_PATCHLEVEL >= 4)
  cudnnOpsInferVersionCheck();
  cudnnCnnInferVersionCheck();
#endif
  bool tensor_op_math_available =
      TensorOpMathAvailable(cuda_compute_capability);
  out_algorithms->clear();

  std::vector<dnn::AlgorithmDesc::Index> algo_types;
  if (ConvUseDefaultAlgorithm()) {
    // Force a fallback algorithm.
    algo_types = {CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM};
  } else {
    algo_types = {CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM,
                  CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM,
                  CUDNN_CONVOLUTION_FWD_ALGO_GEMM,
                  CUDNN_CONVOLUTION_FWD_ALGO_DIRECT,
                  CUDNN_CONVOLUTION_FWD_ALGO_FFT,
                  CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD};
    if (CudnnEnvVar<FftTilingForward>::IsEnabled()) {
      algo_types.push_back(CUDNN_CONVOLUTION_FWD_ALGO_FFT_TILING);
    }
    if (CudnnEnvVar<WinogradNonfused>::IsEnabled()) {
      algo_types.push_back(CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD_NONFUSED);
    }
  }

  // The algorithms are intentionally ordered for deterministic operation
  for (auto i : algo_types) {
    if (tensor_op_math_available) {
      out_algorithms->push_back({i, /*use_tensor_ops=*/true});
    }
    out_algorithms->push_back({i, /*use_tensor_ops=*/false});
  }

  return true;
}

bool CudnnSupport::GetRnnAlgorithms(
    std::vector<dnn::AlgorithmDesc>* out_algorithms) {
  // Preload sub libs for cudnn 8.0.4+
#if CUDNN_MAJOR >= 8 && (CUDNN_MINOR > 0 || CUDNN_PATCHLEVEL >= 4)
  cudnnOpsInferVersionCheck();
  cudnnOpsTrainVersionCheck();
  cudnnAdvInferVersionCheck();
  cudnnAdvTrainVersionCheck();
#endif
  std::vector<dnn::AlgorithmDesc::Index> algo_types = {
      // clang-format off
    CUDNN_RNN_ALGO_STANDARD,
    CUDNN_RNN_ALGO_PERSIST_STATIC,
    CUDNN_RNN_ALGO_PERSIST_DYNAMIC,
      // clang-format on
  };

  out_algorithms->clear();
  for (auto i : algo_types) {
    out_algorithms->push_back({i, /*use_tensor_ops=*/false});
    out_algorithms->push_back({i, /*use_tensor_ops=*/true});
  }
  return true;
}

bool CudnnSupport::GetConvolveBackwardDataAlgorithms(
    CudaComputeCapability cuda_compute_capability,
    std::vector<dnn::AlgorithmDesc>* out_algorithms) {
  // Preload sub libs for cudnn 8.0.4+
#if CUDNN_MAJOR >= 8 && (CUDNN_MINOR > 0 || CUDNN_PATCHLEVEL >= 4)
  cudnnOpsInferVersionCheck();
  cudnnOpsTrainVersionCheck();
  cudnnCnnInferVersionCheck();
  cudnnCnnTrainVersionCheck();
#endif
  bool tensor_op_math_available =
      TensorOpMathAvailable(cuda_compute_capability);
  out_algorithms->clear();

  std::vector<dnn::AlgorithmDesc::Index> algo_types = {
      // clang-format off
    CUDNN_CONVOLUTION_BWD_DATA_ALGO_1,
    CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT,
    CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT_TILING,
    CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD,
      // clang-format on
  };
  if (CudnnEnvVar<WinogradNonfused>::IsEnabled()) {
    algo_types.push_back(CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD_NONFUSED);
  }
  if (!RequireCudnnDeterminism()) {
    algo_types.push_back(CUDNN_CONVOLUTION_BWD_DATA_ALGO_0);
  }

  // The algorithms are intentionally ordered for deterministic operation
  for (auto i : algo_types) {
    if (tensor_op_math_available) {
      out_algorithms->push_back({i, /*use_tensor_ops=*/true});
    }
    out_algorithms->push_back({i, /*use_tensor_ops=*/false});
  }

  return true;
}

bool CudnnSupport::GetConvolveBackwardFilterAlgorithms(
    CudaComputeCapability cuda_compute_capability,
    std::vector<dnn::AlgorithmDesc>* out_algorithms) {
  // Preload sub libs for cudnn 8.0.4+
#if CUDNN_MAJOR >= 8 && (CUDNN_MINOR > 0 || CUDNN_PATCHLEVEL >= 4)
  cudnnOpsInferVersionCheck();
  cudnnOpsTrainVersionCheck();
  cudnnCnnInferVersionCheck();
  cudnnCnnTrainVersionCheck();
#endif
  bool tensor_op_math_available =
      TensorOpMathAvailable(cuda_compute_capability);
  out_algorithms->clear();

  std::vector<dnn::AlgorithmDesc::Index> algo_types = {
      // clang-format off
      CUDNN_CONVOLUTION_BWD_FILTER_ALGO_1,
      CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT,
      // Based on cudnn.h, the following is not implemented.
      // CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD,

      // Produces incorrect results for some shapes. Disabled for now, see
      // NVIDIA bug 2072856. TODO(csigg): Only disable for subset of shapes.
      // CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT_TILING,
      // clang-format on
  };
  if (CudnnEnvVar<WinogradNonfused>::IsEnabled()) {
    algo_types.push_back(CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD_NONFUSED);
  }
  if (!RequireCudnnDeterminism()) {
    algo_types.push_back(CUDNN_CONVOLUTION_BWD_FILTER_ALGO_0);
    algo_types.push_back(CUDNN_CONVOLUTION_BWD_FILTER_ALGO_3);
  }

  // The algorithms are intentionally ordered for deterministic operation
  for (auto i : algo_types) {
    if (tensor_op_math_available) {
      out_algorithms->push_back({i, /*use_tensor_ops=*/true});
    }
    out_algorithms->push_back({i, /*use_tensor_ops=*/false});
  }

  return true;
}

bool CudnnSupport::DoBatchNormalizationForward(
    Stream* stream, const DeviceMemory<float>& x,
    const DeviceMemory<float>& scale, const DeviceMemory<float>& offset,
    const DeviceMemory<float>& estimated_mean,
    const DeviceMemory<float>& estimated_variance,
    const DeviceMemory<float>& side_input, const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    const double exponential_average_factor,
    dnn::ActivationMode activation_mode, DeviceMemory<float>* y,
    DeviceMemory<float>* batch_mean, DeviceMemory<float>* batch_var,
    DeviceMemory<float>* saved_mean, DeviceMemory<float>* saved_inv_var,
    bool is_training, ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator) {
  return IsStatusOk(
      DoBatchNormalizationForwardImpl<float, float>(
          stream, dnn::DataType::kFloat, dnn::DataType::kFloat, x, scale,
          offset, estimated_mean, estimated_variance, side_input, x_desc,
          scale_offset_desc, epsilon, exponential_average_factor,
          activation_mode, y, batch_mean, batch_var, saved_mean, saved_inv_var,
          is_training, reserve_space_allocator, workspace_allocator),
      /*report_error=*/true);
}

bool CudnnSupport::DoBatchNormalizationForward(
    Stream* stream, const DeviceMemory<Eigen::half>& x,
    const DeviceMemory<float>& scale, const DeviceMemory<float>& offset,
    const DeviceMemory<float>& estimated_mean,
    const DeviceMemory<float>& estimated_variance,
    const DeviceMemory<Eigen::half>& side_input,
    const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    const double exponential_average_factor,
    dnn::ActivationMode activation_mode, DeviceMemory<Eigen::half>* y,
    DeviceMemory<float>* batch_mean, DeviceMemory<float>* batch_var,
    DeviceMemory<float>* saved_mean, DeviceMemory<float>* saved_inv_var,
    bool is_training, ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator) {
  return IsStatusOk(
      DoBatchNormalizationForwardImpl<Eigen::half, float>(
          stream, dnn::DataType::kHalf, dnn::DataType::kFloat, x, scale, offset,
          estimated_mean, estimated_variance, side_input, x_desc,
          scale_offset_desc, epsilon, exponential_average_factor,
          activation_mode, y, batch_mean, batch_var, saved_mean, saved_inv_var,
          is_training, reserve_space_allocator, workspace_allocator),
      /*report_error=*/true);
}

template <class T, class U>
port::Status CudnnSupport::DoBatchNormalizationForwardImpl(
    Stream* stream, dnn::DataType input_data_type,
    dnn::DataType scale_data_type, const DeviceMemory<T>& x,
    const DeviceMemory<U>& scale, const DeviceMemory<U>& offset,
    const DeviceMemory<U>& estimated_mean,
    const DeviceMemory<U>& estimated_variance,
    const DeviceMemory<T>& side_input, const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    const double exponential_average_factor,
    dnn::ActivationMode activation_mode, DeviceMemory<T>* y,
    DeviceMemory<U>* batch_mean, DeviceMemory<U>* batch_var,
    DeviceMemory<U>* saved_mean, DeviceMemory<U>* saved_inv_var,
    bool is_training, ScratchAllocator* reserve_space_allocator,
    ScratchAllocator* workspace_allocator) {
  CudnnTensorDescriptor x_descriptor(x_desc, ToCudnnDataType(input_data_type));
  CudnnTensorDescriptor scale_offset_descriptor(
      scale_offset_desc, ToCudnnDataType(scale_data_type));
  cudnnBatchNormMode_t mode = CUDNN_BATCHNORM_SPATIAL;
  if (BatchnormSpatialPersistentEnabled() && is_training) {
    mode = CUDNN_BATCHNORM_SPATIAL_PERSISTENT;
  }
  float one = 1.0;
  float zero = 0.0;
  auto cudnn = cudnn_->GetHandle(parent_, stream);

  DeviceMemory<uint8> workspace;
  DeviceMemory<uint8> reserve_space;

#if CUDNN_VERSION >= 7402
  const auto get_bn_ops = [&]() -> cudnnBatchNormOps_t {
    if (side_input.is_null()) {
      return activation_mode == dnn::ActivationMode::kNone
                 ? CUDNN_BATCHNORM_OPS_BN
                 : CUDNN_BATCHNORM_OPS_BN_ACTIVATION;
    } else {
      return CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION;
    }
  };
  const cudnnBatchNormOps_t bn_ops = get_bn_ops();

  // We use Nan propagation to be consistent with CudnnSupport::DoActivate(...).
  CudnnActivationDescriptor activation_desc(
      activation_mode, CUDNN_PROPAGATE_NAN, x_desc.value_max());

  if (reserve_space_allocator != nullptr && workspace_allocator != nullptr) {
    SE_ASSIGN_OR_RETURN(
        workspace,
        CreateBatchNormForwardWorkspace(
            stream, cudnn, mode, bn_ops, activation_desc.handle(), x_descriptor,
            scale_offset_descriptor, workspace_allocator))
    if (is_training) {
      size_t reserve_space_size_in_bytes = 0;
      RETURN_IF_CUDNN_ERROR(
          cudnnGetBatchNormalizationTrainingExReserveSpaceSize(
              /*handle=*/cudnn.handle(), /*mode=*/mode, /*bnOps=*/bn_ops,
              /*activationDesc=*/activation_desc.handle(),
              /*xDesc=*/x_descriptor.handle(),
              /*sizeInBytes=*/&reserve_space_size_in_bytes));
      SE_ASSIGN_OR_RETURN(reserve_space, reserve_space_allocator->AllocateBytes(
                                             reserve_space_size_in_bytes));
    }
  }
#endif

  auto check_no_side_input_or_activation = [&]() -> port::Status {
    if (activation_mode != dnn::ActivationMode::kNone ||
        !side_input.is_null()) {
      return port::Status(
          port::error::INTERNAL,
          absl::StrCat(
              "Side input and activation are not supported by cuDNN version: ",
              CUDNN_VERSION));
    } else {
      return port::Status::OK();
    }
  };

  if (is_training) {
    CHECK_EQ(batch_mean->is_null(), batch_var->is_null())
        << "batch_mean and batch_var must both be null or both be non-null";

    void* batch_mean_opaque;
    void* batch_var_opaque;
    if (!batch_mean->is_null() && !batch_var->is_null()) {
      if (exponential_average_factor == 1.0) {
        stream->ThenMemZero(batch_mean, batch_mean->size());
        stream->ThenMemZero(batch_var, batch_var->size());
      }
      batch_mean_opaque = batch_mean->opaque();
      batch_var_opaque = batch_var->opaque();
    } else {
      batch_mean_opaque = nullptr;
      batch_var_opaque = nullptr;
    }

    bool called = false;
#if CUDNN_VERSION >= 7402
    if (reserve_space_allocator != nullptr && workspace_allocator != nullptr) {
      called = true;
      RETURN_IF_CUDNN_ERROR(cudnnBatchNormalizationForwardTrainingEx(
          /*handle=*/cudnn.handle(),
          /*mode=*/mode,
          /*bnOps=*/bn_ops,
          /*alpha=*/&one,
          /*beta=*/&zero,
          /*xDesc=*/x_descriptor.handle(),
          /*xData=*/x.opaque(),
          /*zDesc=*/x_descriptor.handle(),
          /*zData=*/side_input.opaque(),
          /*yDesc=*/x_descriptor.handle(),
          /*yData=*/y->opaque(),
          /*bnScaleBiasMeanVarDesc=*/scale_offset_descriptor.handle(),
          /*bnScale=*/scale.opaque(),
          /*bnBias=*/offset.opaque(),
          /*exponentialAverageFactor=*/exponential_average_factor,
          /*resultRunningMean=*/batch_mean_opaque,
          /*resultRunningVariance=*/batch_var_opaque,
          /*epsilon=*/epsilon,
          /*resultSaveMean=*/saved_mean->opaque(),
          /*resultSaveInvVariance=*/saved_inv_var->opaque(),
          /*activationDesc=*/activation_desc.handle(),
          /*workspace=*/workspace.opaque(),
          /*workSpaceSizeInBytes=*/workspace.size(),
          /*reserveSpace=*/reserve_space.opaque(),
          /*reserveSpaceSizeInBytes=*/reserve_space.size()));
    }
#endif
    if (!called) {
      SE_RETURN_IF_ERROR(check_no_side_input_or_activation());
      RETURN_IF_CUDNN_ERROR(cudnnBatchNormalizationForwardTraining(
          cudnn.handle(), mode, &one, &zero, x_descriptor.handle(), x.opaque(),
          x_descriptor.handle(), y->opaque(), scale_offset_descriptor.handle(),
          scale.opaque(), offset.opaque(), exponential_average_factor,
          batch_mean_opaque, batch_var_opaque, epsilon, saved_mean->opaque(),
          saved_inv_var->opaque()));
    }
  } else {
    const void* maybe_inv_var = estimated_variance.opaque();
    SE_RETURN_IF_ERROR(check_no_side_input_or_activation());
    RETURN_IF_CUDNN_ERROR(cudnnBatchNormalizationForwardInference(
        cudnn.handle(), mode, &one, &zero, x_descriptor.handle(), x.opaque(),
        x_descriptor.handle(), y->opaque(), scale_offset_descriptor.handle(),
        scale.opaque(), offset.opaque(), estimated_mean.opaque(), maybe_inv_var,
        epsilon));
  }
  return port::Status::OK();
}

bool CudnnSupport::DoBatchNormalizationBackward(
    Stream* stream, const DeviceMemory<float>& y_backprop,
    const DeviceMemory<float>& x, const DeviceMemory<float>& scale,
    const DeviceMemory<float>& offset, const DeviceMemory<float>& mean,
    const DeviceMemory<float>& inv_var, const DeviceMemory<float>& y,
    const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    dnn::ActivationMode activation_mode, DeviceMemory<float>* x_backprop,
    DeviceMemory<float>* scale_backprop, DeviceMemory<float>* offset_backprop,
    DeviceMemory<float>* side_input_backprop,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator) {
  return IsStatusOk(
      DoBatchNormalizationBackwardImpl(
          stream, CUDNN_DATA_FLOAT, CUDNN_DATA_FLOAT, y_backprop, x, scale,
          offset, mean, inv_var, y, x_desc, scale_offset_desc, epsilon,
          activation_mode, x_backprop, scale_backprop, offset_backprop,
          side_input_backprop, reserve_space_data, workspace_allocator),
      /*report_error=*/true);
}

bool CudnnSupport::DoBatchNormalizationBackward(
    Stream* stream, const DeviceMemory<Eigen::half>& y_backprop,
    const DeviceMemory<Eigen::half>& x, const DeviceMemory<float>& scale,
    const DeviceMemory<float>& offset, const DeviceMemory<float>& mean,
    const DeviceMemory<float>& inv_var, const DeviceMemory<Eigen::half>& y,
    const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    dnn::ActivationMode activation_mode, DeviceMemory<Eigen::half>* x_backprop,
    DeviceMemory<float>* scale_backprop, DeviceMemory<float>* offset_backprop,
    DeviceMemory<Eigen::half>* side_input_backprop,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator) {
  return IsStatusOk(
      DoBatchNormalizationBackwardImpl(
          stream, CUDNN_DATA_HALF, CUDNN_DATA_FLOAT, y_backprop, x, scale,
          offset, mean, inv_var, y, x_desc, scale_offset_desc, epsilon,
          activation_mode, x_backprop, scale_backprop, offset_backprop,
          side_input_backprop, reserve_space_data, workspace_allocator),
      /*report_error=*/true);
}

template <class T, class U>
port::Status CudnnSupport::DoBatchNormalizationBackwardImpl(
    Stream* stream, int cudnn_input_type, int cudnn_scale_type,
    const DeviceMemory<T>& y_backprop, const DeviceMemory<T>& x,
    const DeviceMemory<U>& scale, const DeviceMemory<U>& offset,
    const DeviceMemory<U>& mean, const DeviceMemory<U>& inv_var,
    const DeviceMemory<T>& y, const dnn::BatchDescriptor& x_desc,
    const dnn::BatchDescriptor& scale_offset_desc, const double epsilon,
    dnn::ActivationMode activation_mode, DeviceMemory<T>* x_backprop,
    DeviceMemory<U>* scale_backprop, DeviceMemory<U>* offset_backprop,
    DeviceMemory<T>* side_input_backprop,
    DeviceMemory<uint8>* reserve_space_data,
    ScratchAllocator* workspace_allocator) {
  CudnnTensorDescriptor x_descriptor(
      x_desc, static_cast<cudnnDataType_t>(cudnn_input_type));
  CudnnTensorDescriptor scale_offset_descriptor(
      scale_offset_desc, static_cast<cudnnDataType_t>(cudnn_scale_type));
  cudnnBatchNormMode_t mode = CUDNN_BATCHNORM_SPATIAL;
  if (BatchnormSpatialPersistentEnabled()) {
    mode = CUDNN_BATCHNORM_SPATIAL_PERSISTENT;
  }
  float one = 1.0;
  float zero = 0.0;

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  bool called = false;
#if CUDNN_VERSION >= 7402
  if (reserve_space_data != nullptr && workspace_allocator != nullptr) {
    called = true;
    const cudnnBatchNormOps_t bn_ops = [&]() {
      if (side_input_backprop->is_null()) {
        return activation_mode == dnn::ActivationMode::kNone
                   ? CUDNN_BATCHNORM_OPS_BN
                   : CUDNN_BATCHNORM_OPS_BN_ACTIVATION;
      } else {
        return CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION;
      }
    }();

    // We use Nan propagation to be consistent with
    // CudnnSupport::DoActivate(...).
    CudnnActivationDescriptor activation_desc(
        activation_mode, CUDNN_PROPAGATE_NAN, x_desc.value_max());

    SE_ASSIGN_OR_RETURN(
        DeviceMemory<uint8> workspace,
        CreateBatchNormBackwardWorkspace(
            stream, cudnn, mode, bn_ops, activation_desc.handle(), x_descriptor,
            scale_offset_descriptor, workspace_allocator))
    RETURN_IF_CUDNN_ERROR(cudnnBatchNormalizationBackwardEx(
        /*handle=*/cudnn.handle(),
        /*mode=*/mode,
        /*bnOps=*/bn_ops,
        /*alphaDataDiff=*/&one,
        /*betaDataDiff=*/&zero,
        /*alphaParamDiff=*/&one,
        /*betaParamDiff=*/&zero,
        /*xDesc=*/x_descriptor.handle(),
        /*xData=*/x.opaque(),
        /*yDesc=*/x_descriptor.handle(),
        /*yData=*/y.opaque(),
        /*dyDesc=*/x_descriptor.handle(),
        /*dyData=*/y_backprop.opaque(),
        /*dzDesc=*/x_descriptor.handle(),
        /*dzData=*/side_input_backprop->opaque(),
        /*dxDesc=*/x_descriptor.handle(),
        /*dxData=*/x_backprop->opaque(),
        /*dBnScaleBiasDesc=*/scale_offset_descriptor.handle(),
        /*bnScaleData=*/scale.opaque(),
        /*bnBiasData=*/offset.opaque(),
        /*dBnScaleData=*/scale_backprop->opaque(),
        /*dBnBiasData=*/offset_backprop->opaque(),
        /*epsilon=*/epsilon,
        /*savedMean=*/mean.opaque(),
        /*savedInvVariance=*/inv_var.opaque(),
        /*activationDesc=*/activation_desc.handle(),
        /*workspace=*/workspace.opaque(),
        /*workSpaceSizeInBytes=*/workspace.size(),
        /*reserveSpace=*/reserve_space_data->opaque(),
        /*reserveSpaceSizeInBytes=*/reserve_space_data->size()));
  }
#endif
  auto check_no_side_input_or_activation = [&]() -> port::Status {
    if (activation_mode != dnn::ActivationMode::kNone ||
        !side_input_backprop->is_null()) {
      return port::InternalError(absl::StrCat(
          "Side input and activation are not supported by cuDNN version: ",
          CUDNN_VERSION));
    } else {
      return port::Status::OK();
    }
  };

  if (!called && check_no_side_input_or_activation().ok()) {
    RETURN_IF_CUDNN_ERROR(cudnnBatchNormalizationBackward(
        cudnn.handle(), mode, &one, &zero, &one, &zero, x_descriptor.handle(),
        x.opaque(), x_descriptor.handle(), y_backprop.opaque(),
        x_descriptor.handle(), x_backprop->opaque(),
        scale_offset_descriptor.handle(), scale.opaque(),
        scale_backprop->opaque(), offset_backprop->opaque(), epsilon,
        mean.opaque(), inv_var.opaque()));
  }

  return port::Status::OK();
}

port::Status CudnnSupport::DoFusedConvolve(
    Stream* stream, dnn::DataType input_type, dnn::DataType side_input_type,
    dnn::DataType bias_type, dnn::DataType output_type,
    const dnn::BatchDescriptor& conv_input_descriptor,
    DeviceMemoryBase conv_input_data, double conv_scale,
    const dnn::FilterDescriptor& filter_descriptor,
    DeviceMemoryBase filter_data,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    DeviceMemoryBase side_input_data, double side_input_scale,
    const dnn::BatchDescriptor& bias_descriptor, DeviceMemoryBase biases,
    dnn::ActivationMode activation_mode,
    const dnn::BatchDescriptor& output_descriptor, DeviceMemoryBase output_data,
    ScratchAllocator* scratch_allocator,
    const dnn::AlgorithmConfig& algorithm_config,
    dnn::ProfileResult* output_profile_result) {
  if (input_type == dnn::DataType::kInt8 &&
      !stream->GetCudaComputeCapability().IsAtLeast(6, 1)) {
    return port::UnimplementedError(
        "cudnnConvolutionBiasActivationForward() for int8 is only supported "
        "on GPUs with compute capability 6.1 or later.");
  }

  if (input_type == dnn::DataType::kInt8 &&
      output_type == dnn::DataType::kFloat &&
      (CUDNN_VERSION >= 8000 && CUDNN_VERSION <= 8200)) {
    return port::UnimplementedError(
        "int8 -> float fused conv is disabled for this cuDNN version. See "
        "go/nvbugs/3326122");
  }

  if (activation_mode != dnn::ActivationMode::kRelu &&
      activation_mode != dnn::ActivationMode::kNone) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "cudnnConvolutionBiasActivationForward() only supports "
                        "Relu or None activation.");
  }

  CudnnTensorDescriptor conv_input_nd(
      conv_input_descriptor,
      ToCudnnDataType(input_type, conv_input_descriptor.layout()));
  CudnnTensorDescriptor output_nd(
      output_descriptor,
      ToCudnnDataType(output_type, conv_input_descriptor.layout()));
  CudnnFilterDescriptor filter(
      filter_descriptor,
      ToCudnnDataType(input_type, filter_descriptor.layout()));
  CudnnTensorDescriptor bias_nd(bias_descriptor, ToCudnnDataType(bias_type));

  DeviceMemory<uint8> scratch;
  dnn::AlgorithmDesc algo_desc;
  {
    auto cudnn = cudnn_->GetHandle(parent_, stream);
    SE_ASSIGN_OR_RETURN(
        algo_desc,
        GetCudnnConvolutionForwardAlgorithm(
            stream, cudnn, algorithm_config, conv_input_nd, filter, input_type,
            convolution_descriptor, output_nd, scratch_allocator, &scratch));
  }  // Explicitly release cuDNN handle.

  CudnnConvolutionDescriptor conv(
      convolution_descriptor,
      ToCudnnDataType(GetConvAccumulatorType(input_type)));
  conv.set_use_tensor_op_math(algo_desc.tensor_ops_enabled());

  // CUDNN v6 only supports CUDNN_NOT_PROPAGATE_NAN as the reluNanOpt for
  // activation descriptor. Note that this will change the nan propagation
  // behavior from separate conv, bias, and relu (which by default is
  // CUDNN_PROPAGATE_NAN).
  CudnnActivationDescriptor activation_desc(
      activation_mode, CUDNN_NOT_PROPAGATE_NAN, output_descriptor.value_max());

  SE_ASSIGN_OR_RETURN(
      auto runner,
      CudnnLegacyFusedConvRunner::Create(
          parent_, stream, cudnn_.get(), std::move(algo_desc), input_type,
          conv_scale, side_input_scale, std::move(conv_input_nd),
          std::move(output_nd), std::move(filter), std::move(bias_nd),
          std::move(conv), std::move(activation_desc)));

  return runner(stream, output_profile_result, scratch, conv_input_data,
                filter_data, side_input_data, biases, output_data);
}

port::Status CudnnSupport::DoPrepareForCtcLoss(
    Stream* stream, dnn::DataType element_type,
    const dnn::RnnStateTensorDescriptor& probs_desc,
    const dnn::RnnStateTensorDescriptor& grads_desc,
    absl::Span<const int> labels_data,
    absl::Span<const int> labels_lengths_data,
    absl::Span<const int> input_lengths_data,
    ScratchAllocator* scratch_allocator, DeviceMemory<uint8>* scratch_memory,
    int* ctc_loss_algo_id) {
  auto cudnn = cudnn_->GetHandle(parent_, stream);
  // Query the workspace size.
  size_t workspace_size_in_bytes = 0;
#if CUDNN_VERSION >= 7603
  CudnnCtcLossDescriptor cudnn_ctc_loss_desc(ToCudnnDataType(element_type));
  const CudnnRnnStateTensorDescriptor& cudnn_probs_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(probs_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_grads_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(grads_desc);

  // Try running with `algo`, if successful then pick it. The
  // non-deterministic algorithm is first and thus preferentially picked when
  // determinism is not required.
  auto algo = RequireCudnnDeterminism() ? CUDNN_CTC_LOSS_ALGO_DETERMINISTIC
                                        : CUDNN_CTC_LOSS_ALGO_NON_DETERMINISTIC;
  cudnnStatus_t status = cudnnGetCTCLossWorkspaceSize(
      /*handle=*/cudnn.handle(), /*probsDesc=*/cudnn_probs_desc.handle(),
      /*gradientsDesc=*/cudnn_grads_desc.handle(),
      /*labels=*/labels_data.data(),
      /*labelLengths=*/labels_lengths_data.data(),
      /*inputLengths=*/input_lengths_data.data(),
      /*algo=*/algo,
      /*ctcLossDesc=*/cudnn_ctc_loss_desc.handle(),
      /*sizeInBytes=*/&workspace_size_in_bytes);
  if (RequireCudnnDeterminism()) {
    RETURN_IF_CUDNN_ERROR(status);
  }

  if (status != CUDNN_STATUS_SUCCESS) {
    algo = CUDNN_CTC_LOSS_ALGO_DETERMINISTIC;
    RETURN_IF_CUDNN_ERROR(cudnnGetCTCLossWorkspaceSize(
        /*handle=*/cudnn.handle(), /*probsDesc=*/cudnn_probs_desc.handle(),
        /*gradientsDesc=*/cudnn_grads_desc.handle(),
        /*labels=*/labels_data.data(),
        /*labelLengths=*/labels_lengths_data.data(),
        /*inputLengths=*/input_lengths_data.data(),
        /*algo=*/algo,
        /*ctcLossDesc=*/cudnn_ctc_loss_desc.handle(),
        /*sizeInBytes=*/&workspace_size_in_bytes));
  }
  *ctc_loss_algo_id = algo;
#else
  return port::Status(port::error::INVALID_ARGUMENT,
                      "No supported cudnnGetCTCLossWorkspaceSize when "
                      "CUDNN_VERSION < 7.6.3");
#endif
  // Allocate the workspace.
  if (workspace_size_in_bytes == 0) {
    *scratch_memory = DeviceMemory<uint8>();
    return port::Status::OK();
  }
  const auto scratch_or =
      scratch_allocator->AllocateBytes(workspace_size_in_bytes);
  if (scratch_or.ok()) {
    *scratch_memory = scratch_or.ValueOrDie();
    return port::Status::OK();
  }
  return port::InternalError(
      "Failed to allocate scratch memory for the CuDNN CTC Loss");
}

port::Status CudnnSupport::DoCtcLoss(
    Stream* stream, dnn::DataType element_type,
    const dnn::RnnStateTensorDescriptor& probs_desc,
    const DeviceMemoryBase probs_data, absl::Span<const int> labels_data,
    absl::Span<const int> labels_lengths_data,
    absl::Span<const int> input_lengths_data, DeviceMemoryBase costs_data,
    const dnn::RnnStateTensorDescriptor& grads_desc,
    DeviceMemoryBase grads_data, DeviceMemory<uint8> scratch_memory,
    int ctc_loss_algo_id) {
  // Current cuDNN CTC Loss only supports the float datatype
  if (CUDNN_VERSION < 7603 || element_type != dnn::DataType::kFloat) {
    return port::Status(port::error::INVALID_ARGUMENT,
                        "CudnnCtcLossDescriptor is supported only when the "
                        "CUDNN_VERSION >= 7.6.3 and DataType is float");
  }
  CudnnCtcLossDescriptor cudnn_ctc_loss_desc(ToCudnnDataType(element_type));
  const CudnnRnnStateTensorDescriptor& cudnn_probs_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(probs_desc);
  const CudnnRnnStateTensorDescriptor& cudnn_grads_desc =
      static_cast<const CudnnRnnStateTensorDescriptor&>(grads_desc);
  return DoCtcLossImpl(stream, cudnn_probs_desc, probs_data, labels_data,
                       labels_lengths_data, input_lengths_data, costs_data,
                       cudnn_grads_desc, grads_data, cudnn_ctc_loss_desc,
                       scratch_memory, ctc_loss_algo_id);
}

bool CudnnSupport::DoTransformTensor(Stream* stream,
                                     const dnn::BatchDescriptor& input_desc,
                                     dnn::DataType input_type,
                                     const DeviceMemoryBase& input_data,
                                     const dnn::BatchDescriptor& output_desc,
                                     dnn::DataType output_type, float scale,
                                     DeviceMemoryBase* output_data) {
  float beta = 0.0f;
  CudnnTensorDescriptor input_tensor_desc(
      input_desc, ToCudnnDataType(input_type, input_desc.layout()));
  CudnnTensorDescriptor output_tensor_desc(
      output_desc, ToCudnnDataType(output_type, output_desc.layout()));
  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnTransformTensor(
        cudnn.handle(), &scale, input_tensor_desc.handle(), input_data.opaque(),
        &beta, output_tensor_desc.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoMatMul(Stream* stream,
                            const DeviceMemory<float>& input_data,
                            const DeviceMemory<float>& weights,
                            const dnn::BatchDescriptor& input_dimensions,
                            const dnn::BatchDescriptor& output_dimensions,
                            DeviceMemory<float>* output_data) {
  if (input_dimensions.count() != output_dimensions.count()) {
    LOG(ERROR) << "MatMul input and output dimensions are not compatible.";
    return false;
  }

  // We do not permute the input or output, instead we just
  // reinterpret the layout. We are working with row-major matrices
  // and the rows of the input and output correspond to batch, so
  // batch has to be outermost in both the input and output.
  //
  // By adding transposes to the BLAS gemm call we could perhaps make
  // the kYXDepthBatch layout work as well, but there has been no need
  // for that so far.
  if (input_dimensions.layout() != dnn::DataLayout::kBatchYXDepth &&
      input_dimensions.layout() != dnn::DataLayout::kBatchDepthYX) {
    LOG(ERROR) << "Unsupported MatMul input layout.";
    return false;
  }
  if (output_dimensions.layout() != dnn::DataLayout::kBatchYXDepth &&
      output_dimensions.layout() != dnn::DataLayout::kBatchDepthYX) {
    LOG(ERROR) << "Unsupported MatMul output layout.";
    return false;
  }

  if (output_dimensions.width() == 1 && output_dimensions.height() == 1) {
    // This is a fast path that also supports the kBatchYXDepth layout.

    // The matrices here are in row-major format while BLAS expects
    // column-major, i.e. our matrices are transposed as far as BLAS
    // is concerned. So we need to compute output^T =
    // input^T*weights^T. There is no parameter for transposing the
    // output in BLAS gemm, but instead we can transpose both sides of
    // the equality to see that this is equivalent to
    // output=weights*input. So we only need to swap the order of
    // weights and input in the matrix product to correct for the
    // row-major versus column-major difference.
    const int64_t m = output_dimensions.NodesAcrossFeatureMaps();
    const int64_t n = input_dimensions.count();
    const int64_t k = input_dimensions.NodesAcrossFeatureMaps();
    if (!stream
             ->ThenBlasGemm(blas::Transpose::kNoTranspose,
                            blas::Transpose::kNoTranspose, m, n, k, weights, m,
                            input_data, k, output_data, m)
             .ok()) {
      return false;
    }
  } else {
    // This is a slower and more complex path that supports output
    // width() * height() > 1, though it only supports the
    // kBatchYXDepth layout. Does support kBatchDepthYX if output
    // feature_map_count() == 1, as then there is no difference
    // between the two layouts.
    //
    // The operation here is the same as above, except that we have to
    // do the matrix multiplication for each (y,x) output coordinate
    // separately. We then interpret weights as containing K = width()
    // * height() different matrices, which we all multiply onto the
    // matrix from input_data, yielding K matrix products. We then
    // combine these together into one matrix by concatenating all the
    // first rows of these matrices, then all the seconds rows and so
    // on. We can do this with a batched matrix multiplication, where
    // the result is written to a different submatrix of the output
    // for each matrix multiplication.
    //
    // The reason that we only support the kBatchYXDepth output layout
    // is that we have to do something in the depth for each (y,x)
    // coordinate. The kBatchYXDepth layout has the depth information
    // for each point (y,x) in contiguous memory while the
    // kBatchDepthYX layout does not.
    //
    // TODO(broune): Consider a special case for when output depth ==
    // 1, as then possibly this could all be done as one matrix
    // multiplication instead of a batched one, which should be
    // faster. Another possibility would be to add a weights layout
    // parameter and then support kBatchDepthYX for a different
    // weights layout.
    if (output_dimensions.layout() != dnn::DataLayout::kBatchYXDepth &&
        !(output_dimensions.layout() == dnn::DataLayout::kBatchDepthYX &&
          output_dimensions.feature_map_count() == 1)) {
      LOG(ERROR) << "Unsupported MatMul output layout.";
      return false;
    }

    const float alpha = 1.0f;  // Take the matrix product without scaling it.
    const float beta = 0.0f;   // Ignore the original values in output_data.
    const uint64_t m = output_dimensions.feature_map_count();
    const uint64_t n = input_dimensions.count();
    const uint64_t k = input_dimensions.NodesAcrossFeatureMaps();
    const int lda = m;
    const int ldb = k;
    const int ldc = output_dimensions.NodesAcrossFeatureMaps();
    const int batch_count = output_dimensions.NodesPerFeatureMap();

    std::vector<DeviceMemory<float>> a(batch_count);
    std::vector<DeviceMemory<float>> b(batch_count);
    std::vector<DeviceMemory<float>> c(batch_count);
    for (int i = 0; i < batch_count; ++i) {
      const int weights_offset = i * input_dimensions.NodesAcrossFeatureMaps() *
                                 output_dimensions.feature_map_count();
      a[i] = DeviceMemory<float>::MakeFromByteSize(
          const_cast<float*>(reinterpret_cast<const float*>(weights.opaque())) +
              weights_offset,
          weights.ElementCount() - weights_offset);

      b[i] = input_data;

      const int output_offset = i * output_dimensions.feature_map_count();
      c[i] = DeviceMemory<float>::MakeFromByteSize(
          const_cast<float*>(
              reinterpret_cast<const float*>(output_data->opaque())) +
              output_offset,
          output_data->ElementCount() - output_offset);
    }
    const auto toPtrs = [](std::vector<DeviceMemory<float>>& v) {
      std::vector<DeviceMemory<float>*> ptrs;
      ptrs.reserve(v.size());
      for (auto& mem : v) {
        ptrs.push_back(&mem);
      }
      return ptrs;
    };

    stream->ThenBlasGemmBatched(blas::Transpose::kNoTranspose,
                                blas::Transpose::kNoTranspose, m, n, k, alpha,
                                toPtrs(a), lda, toPtrs(b), ldb, beta, toPtrs(c),
                                ldc, batch_count);
  }

  return stream->ok();
}

bool CudnnSupport::DoBiasAdd(Stream* stream,
                             const DeviceMemory<float>& input_data,
                             const DeviceMemory<float>& biases,
                             const dnn::BatchDescriptor& dimensions,
                             DeviceMemory<float>* output_data) {
  CudnnTensorDescriptor input_descriptor(dimensions, CUDNN_DATA_FLOAT);

  dnn::BatchDescriptor bias_dimensions;
  bias_dimensions.set_count(1)
      .set_feature_map_count(dimensions.feature_map_count())
      .set_height(1)
      .set_width(1)
      .set_layout(dnn::DataLayout::kBatchYXDepth);
  CudnnTensorDescriptor bias_descriptor(bias_dimensions, CUDNN_DATA_FLOAT);

  // cudnnAddTensor after R3 is in-place, so we need to copy input_data to
  // output_data before doing the addition, unless the input and
  // output are at the same address.
  if (input_data.opaque() != output_data->opaque()) {
    stream->ThenMemcpy(output_data, input_data,
                       dimensions.ElementCount() * sizeof(float));
    if (!stream->ok()) {
      LOG(ERROR)
          << "stream " << stream
          << " could not enqueue a tensor copy as part of bias addition.";
      return false;
    }
  }

  const float alpha = 1.0f;
  const float beta = 1.0f;

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnAddTensor(
        cudnn.handle(), &alpha, bias_descriptor.handle(), biases.opaque(),
        &beta, input_descriptor.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoActivate(Stream* stream,
                              dnn::ActivationMode activation_mode,
                              const dnn::BatchDescriptor& dimensions,
                              const DeviceMemory<float>& input_data,
                              DeviceMemory<float>* output_data,
                              uint64_t options) {
  CudnnActivationDescriptor activation_desc(
      activation_mode, CUDNN_PROPAGATE_NAN, dimensions.value_max());

  CudnnTensorDescriptor input_nd(dimensions, CUDNN_DATA_FLOAT);
  // Alpha is the input scaling factor.
  float alpha = 1.0;
  // Beta is the output scaling factor.
  float beta = 0.0;

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnActivationForward(
        cudnn.handle(), activation_desc.handle(), &alpha, input_nd.handle(),
        input_data.opaque(), &beta, input_nd.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolForward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<double>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    DeviceMemory<double>* output_data, ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  double alpha = 1.0;
  // Beta is the scaling factor for output.
  double beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_DOUBLE);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_DOUBLE);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingForward(
        cudnn.handle(), pooling_desc.handle(), &alpha, src_desc.handle(),
        input_data.opaque(), &beta, dest_desc.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolForward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<float>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    DeviceMemory<float>* output_data, ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  float alpha = 1.0;
  // Beta is the scaling factor for output.
  float beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_FLOAT);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_FLOAT);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingForward(
        cudnn.handle(), pooling_desc.handle(), &alpha, src_desc.handle(),
        input_data.opaque(), &beta, dest_desc.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolForward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<Eigen::half>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    DeviceMemory<Eigen::half>* output_data,
    ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  float alpha = 1.0;
  // Beta is the scaling factor for output.
  float beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_HALF);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_HALF);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);
  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingForward(
        cudnn.handle(), pooling_desc.handle(), &alpha, src_desc.handle(),
        input_data.opaque(), &beta, dest_desc.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolForward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<int8>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    DeviceMemory<int8>* output_data, ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  float alpha = 1.0;
  // Beta is the scaling factor for output.
  float beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_INT8);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_INT8);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingForward(
        cudnn.handle(), pooling_desc.handle(), &alpha, src_desc.handle(),
        input_data.opaque(), &beta, dest_desc.handle(), output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolBackward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<double>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    const DeviceMemory<double>& output_data,
    const DeviceMemory<double>& input_diff_data,
    DeviceMemory<double>* output_diff_data,
    ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  double alpha = 1.0;
  // Beta is the scaling factor for output.
  double beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_DOUBLE);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_DOUBLE);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingBackward(
        cudnn.handle(), pooling_desc.handle(), &alpha, dest_desc.handle(),
        output_data.opaque(), dest_desc.handle(), input_diff_data.opaque(),
        src_desc.handle(), input_data.opaque(), &beta, src_desc.handle(),
        output_diff_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolBackward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<float>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    const DeviceMemory<float>& output_data,
    const DeviceMemory<float>& input_diff_data,
    DeviceMemory<float>* output_diff_data,
    ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  float alpha = 1.0;
  // Beta is the scaling factor for output.
  float beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_FLOAT);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_FLOAT);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingBackward(
        cudnn.handle(), pooling_desc.handle(), &alpha, dest_desc.handle(),
        output_data.opaque(), dest_desc.handle(), input_diff_data.opaque(),
        src_desc.handle(), input_data.opaque(), &beta, src_desc.handle(),
        output_diff_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoPoolBackward(
    Stream* stream, const dnn::PoolingDescriptor& pooling_dimensions,
    const dnn::BatchDescriptor& input_dimensions,
    const DeviceMemory<Eigen::half>& input_data,
    const dnn::BatchDescriptor& output_dimensions,
    const DeviceMemory<Eigen::half>& output_data,
    const DeviceMemory<Eigen::half>& input_diff_data,
    DeviceMemory<Eigen::half>* output_diff_data,
    ScratchAllocator* workspace_allocator) {
  // Alpha is the scaling factor for input.
  float alpha = 1.0;
  // Beta is the scaling factor for output.
  float beta = 0.0;

  CudnnTensorDescriptor src_desc(input_dimensions, CUDNN_DATA_HALF);
  CudnnTensorDescriptor dest_desc(output_dimensions, CUDNN_DATA_HALF);
  CudnnPoolingDescriptor pooling_desc(pooling_dimensions);

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnPoolingBackward(
        cudnn.handle(), pooling_desc.handle(), &alpha, dest_desc.handle(),
        output_data.opaque(), dest_desc.handle(), input_diff_data.opaque(),
        src_desc.handle(), input_data.opaque(), &beta, src_desc.handle(),
        output_diff_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoNormalizeWithDimensions(
    Stream* stream, const dnn::NormalizeDescriptor& normalize_descriptor,
    const dnn::BatchDescriptor& dimensions,
    const DeviceMemory<float>& input_data, DeviceMemory<float>* output_data) {
  // Check for unsupported modes.
  if (normalize_descriptor.wrap_around()) {
    LOG(ERROR) << "CUDA LRN does not support cudnn-around mode";
    return false;
  }
  if (normalize_descriptor.segment_size()) {
    LOG(ERROR) << "CUDA LRN does not support segmentation";
    return false;
  }

  CudnnTensorDescriptor dims(dimensions, CUDNN_DATA_FLOAT);
  CudnnNormalizeDescriptor normalize(normalize_descriptor);

  // Alpha is the scaling factor for input.
  float alpha = 1.0f;
  // Beta is the scaling factor for output.
  float beta = 0.0f;

  auto cudnn = cudnn_->GetHandle(parent_, stream);

  // Launch the normalization.
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnLRNCrossChannelForward(
        cudnn.handle(), normalize.handle(), CUDNN_LRN_CROSS_CHANNEL_DIM1,
        &alpha, dims.handle(), input_data.opaque(), &beta, dims.handle(),
        output_data->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoNormalizeBackwardWithDimensions(
    Stream* stream, const dnn::NormalizeDescriptor& normalize_descriptor,
    const dnn::BatchDescriptor& dimensions, const DeviceMemory<float>& raw_data,
    const DeviceMemory<float>& normalized_data,
    const DeviceMemory<float>& normalized_variable_gradient,
    DeviceMemory<float>* raw_variable_gradient,
    ScratchAllocator* workspace_allocator) {
  // Check for unsupported modes.
  if (normalize_descriptor.wrap_around()) {
    LOG(ERROR) << "CUDA LRN does not support cudnn-around mode";
    return false;
  }
  if (normalize_descriptor.segment_size()) {
    LOG(ERROR) << "CUDA LRN does not support segmentation";
    return false;
  }

  CudnnTensorDescriptor dims(dimensions, CUDNN_DATA_FLOAT);
  CudnnNormalizeDescriptor normalize(normalize_descriptor);

  float alpha = 1.0f;
  float beta = 0.0f;

  auto cudnn = cudnn_->GetHandle(parent_, stream);
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnLRNCrossChannelBackward(
        cudnn.handle(), normalize.handle(), CUDNN_LRN_CROSS_CHANNEL_DIM1,
        &alpha, dims.handle(), normalized_data.opaque(), dims.handle(),
        normalized_variable_gradient.opaque(), dims.handle(), raw_data.opaque(),
        &beta, dims.handle(), raw_variable_gradient->opaque()));
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

bool CudnnSupport::DoDepthConcatenate(
    Stream* stream, port::ArraySlice<dnn::BatchDescriptor> input_dimensions,
    port::ArraySlice<const DeviceMemory<float>*> input_data,
    DeviceMemory<float>* output_data) {
  CHECK_EQ(input_dimensions.size(), input_data.size());

  for (const auto& dimensions : input_dimensions) {
    if (dimensions.layout() != dnn::DataLayout::kBatchDepthYX) {
      LOG(ERROR) << "CudnnSupport::DoDepthConcatenate currently only "
                    "supports the kBatchDepthYX layout.";
      return false;
    }
  }

  if (input_dimensions.empty()) {
    return true;  // Nothing to do.
  }

  dnn::BatchDescriptor output_dimensions =
      dnn::BatchDescriptor::DepthConcatenateOutputDescriptor(input_dimensions);

  const int64_t area = output_dimensions.width() * output_dimensions.height();
  const auto index = [area](int64_t batch, int64_t depth, int64_t yx,
                            int64_t max_depth) {
    return (batch * max_depth + depth) * area + yx;
  };

  std::vector<float> output_host(output_dimensions.ElementCount());
  std::vector<float> tmp;
  int64_t depth_sum = 0;
  for (size_t i = 0; i < input_data.size(); ++i) {
    const auto& dimensions = input_dimensions[i];
    tmp.resize(dimensions.ElementCount());
    stream->ThenMemcpyD2H<float>(*input_data[i], absl::MakeSpan(tmp));
    port::Status block_status = stream->BlockHostUntilDone();
    if (!block_status.ok()) {
      LOG(ERROR) << "BlockHostUntilDone failed: " << block_status;
      return false;
    }

    for (int64_t batch = 0; batch < output_dimensions.count(); ++batch) {
      for (int64_t yx = 0; yx < area; ++yx) {
        for (int64_t depth = 0; depth < dimensions.feature_map_count();
             ++depth) {
          LOG(INFO) << output_dimensions.ElementCount() << ' ' << batch << ' '
                    << yx << ' ' << depth;
          output_host[index(batch, depth + depth_sum, yx,
                            output_dimensions.feature_map_count())] =
              tmp[index(batch, depth, yx, dimensions.feature_map_count())];
        }
      }
    }
    depth_sum += dimensions.feature_map_count();
  }
  stream->ThenMemcpyH2D<float>(output_host, output_data);
  return true;
}

bool CudnnSupport::DoElementwiseOperate(
    Stream* stream, dnn::ElementwiseOperation operation,
    port::ArraySlice<dnn::BatchDescriptor> input_dimensions,
    port::ArraySlice<const DeviceMemory<float>*> input_data,
    const dnn::BatchDescriptor& output_dimensions,
    DeviceMemory<float>* output_data) {
  LOG(FATAL) << "not yet implemented";  // TODO(leary)
  return false;
}

bool CudnnSupport::DoXYPad(Stream* stream,
                           const dnn::BatchDescriptor& dimensions,
                           const DeviceMemory<float>& input_data,
                           int64_t left_pad, int64_t right_pad, int64_t top_pad,
                           int64_t bottom_pad,
                           DeviceMemory<float>* output_data) {
  LOG(FATAL) << "not yet implemented";  // TODO(leary)
  return false;
}

bool CudnnSupport::DoXYSlice(Stream* stream,
                             const dnn::BatchDescriptor& dimensions,
                             const DeviceMemory<float>& input_data,
                             int64_t left_trim, int64_t right_trim,
                             int64_t top_trim, int64_t bottom_trim,
                             DeviceMemory<float>* output_data) {
  LOG(FATAL) << "not yet implemented";  // TODO(leary)
  return false;
}

bool CudnnSupport::DoMemcpyD2HQuantized(
    Stream* stream, const DeviceMemory<float>& gpu_unquantized_src,
    dnn::QuantizedActivationMode mode, void* host_dst, int64_t size) {
  LOG(ERROR) << "quantized memcpy not supported by cuDNN";
  return false;
}

bool CudnnSupport::DoMemcpyH2DQuantized(
    Stream* stream, const void* host_src, int64_t size,
    dnn::QuantizedActivationMode mode,
    DeviceMemory<float>* gpu_unquantized_dst) {
  LOG(ERROR) << "quantized memcpy not supported by cuDNN";
  return false;
}

bool CudnnSupport::DeriveOutputBatchDescriptor(
    const dnn::BatchDescriptor& batch_descriptor,
    const dnn::FilterDescriptor& filter_descriptor,
    const dnn::ConvolutionDescriptor& convolution_descriptor,
    dnn::BatchDescriptor* output_batch_descriptor) {
  CudnnTensorDescriptor input_nd(batch_descriptor, CUDNN_DATA_FLOAT);
  CudnnFilterDescriptor filter(filter_descriptor, CUDNN_DATA_FLOAT);
  CudnnConvolutionDescriptor conv(convolution_descriptor, CUDNN_DATA_FLOAT);

  int dn = batch_descriptor.ndims() + 2;
  std::vector<int> dims(dn);  // in BDYX
  const auto status = [&] {
    RETURN_IF_CUDNN_ERROR(cudnnGetConvolutionNdForwardOutputDim(
        conv.handle(), input_nd.handle(), filter.handle(), dn, dims.data()));
    output_batch_descriptor->set_count(dims[0])
        .set_feature_map_count(dims[1])
        .set_layout(batch_descriptor.layout());

    for (int i = 0; i < batch_descriptor.ndims(); i++) {
      output_batch_descriptor->set_spatial_dim(static_cast<dnn::DimIndex>(i),
                                               dims.rbegin()[i]);
    }
    return port::Status::OK();
  }();
  return IsStatusOk(status, /*report_error=*/true);
}

}  // namespace gpu

void initialize_cudnn() {
  port::Status status =
      PluginRegistry::Instance()->RegisterFactory<PluginRegistry::DnnFactory>(
          cuda::kCudaPlatformId, gpu::kCuDnnPlugin, "cuDNN",
          [](internal::StreamExecutorInterface* parent) -> dnn::DnnSupport* {
            gpu::GpuExecutor* cuda_executor =
                dynamic_cast<gpu::GpuExecutor*>(parent);
            if (cuda_executor == nullptr) {
              LOG(ERROR) << "Attempting to initialize an instance of the cuDNN "
                         << "support library with a non-CUDA StreamExecutor";
              return nullptr;
            }

            gpu::CudnnSupport* dnn = new gpu::CudnnSupport(cuda_executor);
            if (!dnn->Init().ok()) {
              // Note: Init() will log a more specific error.
              delete dnn;
              return nullptr;
            }
            return dnn;
          });

  if (!status.ok()) {
    LOG(ERROR) << "Unable to register cuDNN factory: "
               << status.error_message();
  }

  PluginRegistry::Instance()->SetDefaultFactory(
      cuda::kCudaPlatformId, PluginKind::kDnn, gpu::kCuDnnPlugin);
}

}  // namespace stream_executor

#pragma clang diagnostic pop

REGISTER_MODULE_INITIALIZER(register_cudnn,
                            { stream_executor::initialize_cudnn(); });
