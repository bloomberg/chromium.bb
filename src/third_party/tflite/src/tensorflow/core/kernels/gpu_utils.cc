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

#include "tensorflow/core/kernels/gpu_utils.h"

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#include <iterator>

#include "google/protobuf/any.pb.h"
#include "absl/algorithm/container.h"
#include "absl/base/call_once.h"
#include "tensorflow/core/platform/logger.h"
#include "tensorflow/core/protobuf/autotuning.pb.h"
#include "tensorflow/core/protobuf/conv_autotuning.pb.h"
#include "tensorflow/core/util/env_var.h"
#include "tensorflow/core/util/proto/proto_utils.h"
#include "tensorflow/stream_executor/gpu/asm_compiler.h"
#include "tensorflow/stream_executor/gpu/redzone_allocator.h"

namespace tensorflow {

bool RedzoneCheckDisabled() {
  const char* disable_rz_str = std::getenv("TF_DISABLE_RZ_CHECK");
  return disable_rz_str != nullptr && std::strcmp(disable_rz_str, "1") == 0;
}

se::DeviceMemoryBase WrapRedzoneBestEffort(se::RedzoneAllocator* rz_allocator,
                                           se::DeviceMemoryBase buffer) {
  if (RedzoneCheckDisabled()) {
    return buffer;
  }
  auto output_rz_or = rz_allocator->AllocateBytes(buffer.size());
  if (!output_rz_or.ok()) {
    static absl::once_flag rz_allocation_failure_logged;
    absl::call_once(rz_allocation_failure_logged, []() {
      LOG(WARNING) << "Failed to allocate memory for convolution redzone "
                   << "checking; skipping this check. This is benign and only "
                   << "means that we won't check cudnn for out-of-bounds reads "
                   << "and writes. This message will only be printed once.";
    });
    return buffer;
  }
  return se::DeviceMemoryBase(output_rz_or.ValueOrDie());
}

void CheckRedzones(const se::RedzoneAllocator& rz_allocator,
                   tensorflow::AutotuneResult* autotune_result) {
  if (RedzoneCheckDisabled()) {
    return;
  }
  se::port::StatusOr<se::RedzoneAllocator::RedzoneCheckStatus> rz_status =
      rz_allocator.CheckRedzones();
  if (!rz_status.ok()) {
    static absl::once_flag failure_logged;
    absl::call_once(failure_logged, [&]() {
      LOG(WARNING) << "Failed to check cudnn convolutions for out-of-bounds "
                   << "reads and writes with an error message: '"
                   << rz_status.status().error_message()
                   << "'; skipping this check. This only means that we won't "
                   << "check cudnn for out-of-bounds reads and writes. This "
                   << "message will only be printed once.";
    });
    return;
  }
  auto rz_check_status = rz_status.ValueOrDie();
  if (!rz_check_status.ok()) {
    auto* fail = autotune_result->mutable_failure();
    fail->set_msg(rz_check_status.RedzoneFailureMsg());
    fail->set_kind(AutotuneResult::REDZONE_MODIFIED);
    fail->set_buffer_address(
        reinterpret_cast<uint64>(rz_check_status.user_buffer_address));
    LOG(ERROR)
        << "Detected cudnn out-of-bounds write in convolution buffer! This is "
           "likely a cudnn bug. We will skip this algorithm in the future, but "
           "your GPU state may already be corrupted, leading to incorrect "
           "results. Within Google, no action is needed on your part. Outside "
           "of Google, please ensure you're running the latest version of "
           "cudnn. If that doesn't fix the problem, please file a bug with "
           "this full error message and we'll contact nvidia.";
    LOG(ERROR) << rz_check_status.RedzoneFailureMsg();
  }
}

namespace {

tensorflow::CudnnVersion GetCudnnVersion(se::StreamExecutor* stream_executor) {
  tensorflow::CudnnVersion cudnn_version;
  if (auto* dnn = stream_executor->AsDnn()) {
    se::port::StatusOr<se::dnn::VersionInfo> version_or = dnn->GetVersion();
    if (version_or.ok()) {
      const auto& version = version_or.ValueOrDie();
      cudnn_version.set_major(version.major_version());
      cudnn_version.set_minor(version.minor_version());
      cudnn_version.set_patch(version.patch());
    }
  }
  return cudnn_version;
}

tensorflow::ComputeCapability GetComputeCapability(
    se::StreamExecutor* stream_executor) {
  tensorflow::ComputeCapability cc_proto;
  se::CudaComputeCapability cc =
      stream_executor->GetDeviceDescription().cuda_compute_capability();
  cc_proto.set_major(cc.major);
  cc_proto.set_minor(cc.minor);
  return cc_proto;
}

}  // namespace

void LogConvAutotuneResults(se::dnn::ConvolutionKind kind,
                            se::dnn::DataType element_type,
                            se::DeviceMemoryBase input_buffer,
                            se::DeviceMemoryBase filter_buffer,
                            se::DeviceMemoryBase output_buffer,
                            const se::dnn::BatchDescriptor& input_desc,
                            const se::dnn::FilterDescriptor& filter_desc,
                            const se::dnn::BatchDescriptor& output_desc,
                            const se::dnn::ConvolutionDescriptor& conv_desc,
                            se::StreamExecutor* stream_exec,
                            absl::Span<const AutotuneResult> results) {
  AutotuningLog log;
  {
    ConvolutionProto instr;
    instr.set_kind(kind);
    *instr.mutable_input() = input_desc.ToProto(element_type);
    *instr.mutable_filter() = filter_desc.ToProto(element_type);
    *instr.mutable_output() = output_desc.ToProto(element_type);
    *instr.mutable_conv_desc() = conv_desc.ToProto();
    instr.set_conv_scale(1);
    instr.set_side_value_scale(0);
    instr.set_input_address(reinterpret_cast<uint64>(input_buffer.opaque()));
    instr.set_filter_address(reinterpret_cast<uint64>(filter_buffer.opaque()));
    instr.set_output_address(reinterpret_cast<uint64>(output_buffer.opaque()));
    log.mutable_instr()->PackFrom(std::move(instr));
  }
  *log.mutable_cudnn_version() = GetCudnnVersion(stream_exec);
  *log.mutable_compute_capability() = GetComputeCapability(stream_exec);
  log.set_device_pci_bus_id(stream_exec->GetDeviceDescription().pci_bus_id());
  {
    string blas_version;
    if (auto* blas = stream_exec->AsBlas()) {
      if (blas->GetVersion(&blas_version).ok()) {
        log.set_blas_version(blas_version);
      }
    }
  }
  for (const auto& result : results) {
    *log.add_results() = result;
  }
  VLOG(2) << log.DebugString();
  Logger::GetSingleton()->LogProto(log);
}

void LogFusedConvForwardAutotuneResults(
    se::dnn::DataType element_type, se::DeviceMemoryBase input_buffer,
    se::DeviceMemoryBase filter_buffer, se::DeviceMemoryBase output_buffer,
    se::DeviceMemoryBase bias_buffer, se::DeviceMemoryBase side_input_buffer,
    const se::dnn::BatchDescriptor& input_desc,
    const se::dnn::FilterDescriptor& filter_desc,
    const se::dnn::BatchDescriptor& output_desc,
    const se::dnn::ConvolutionDescriptor& conv_desc, double conv_scale,
    double side_value_scale, se::dnn::ActivationMode activation_mode,
    se::StreamExecutor* stream_exec, absl::Span<const AutotuneResult> results) {
  AutotuningLog log;
  {
    ConvolutionProto instr;
    instr.set_kind(se::dnn::ConvolutionKind::FORWARD_BIAS_ACTIVATION);
    *instr.mutable_input() = input_desc.ToProto(element_type);
    *instr.mutable_filter() = filter_desc.ToProto(element_type);
    *instr.mutable_output() = output_desc.ToProto(element_type);
    *instr.mutable_conv_desc() = conv_desc.ToProto();
    instr.set_conv_scale(conv_scale);
    instr.set_side_value_scale(side_value_scale);
    instr.set_activation(activation_mode);
    instr.set_input_address(reinterpret_cast<uint64>(input_buffer.opaque()));
    instr.set_filter_address(reinterpret_cast<uint64>(filter_buffer.opaque()));
    instr.set_output_address(reinterpret_cast<uint64>(output_buffer.opaque()));
    instr.set_bias_address(reinterpret_cast<uint64>(bias_buffer.opaque()));
    instr.set_side_input_address(
        reinterpret_cast<uint64>(side_input_buffer.opaque()));
    log.mutable_instr()->PackFrom(std::move(instr));
  }
  *log.mutable_cudnn_version() = GetCudnnVersion(stream_exec);
  *log.mutable_compute_capability() = GetComputeCapability(stream_exec);
  log.set_device_pci_bus_id(stream_exec->GetDeviceDescription().pci_bus_id());
  {
    string blas_version;
    if (auto* blas = stream_exec->AsBlas()) {
      if (blas->GetVersion(&blas_version).ok()) {
        log.set_blas_version(blas_version);
      }
    }
  }
  for (const auto& result : results) {
    *log.add_results() = result;
  }
  VLOG(2) << log.DebugString();
  Logger::GetSingleton()->LogProto(log);
}

Status BestCudnnConvAlgorithm(
    absl::Span<const AutotuneResult> results,
    std::vector<std::unique_ptr<se::dnn::ConvolveExecutionPlan>>* plans,
    se::dnn::AlgorithmConfig* algo) {
  auto compare_run_times = [](const AutotuneResult& lhs,
                              const AutotuneResult& rhs) {
    return proto_utils::FromDurationProto(lhs.run_time()) <
           proto_utils::FromDurationProto(rhs.run_time());
  };
  int idx = -1;
  int idx_no_scratch = -1;
  for (int i = 0; i < results.size(); i++) {
    if (!results[i].has_failure()) {
      if (idx == -1 || compare_run_times(results[i], results[idx])) {
        idx = i;
      }
      if (results[i].scratch_bytes() == 0 &&
          (idx_no_scratch == -1 ||
           compare_run_times(results[i], results[idx_no_scratch]))) {
        idx_no_scratch = i;
      }
    }
  }

  if (idx == -1) {
    return errors::NotFound("No algorithm worked!");
  }

  if (plans == nullptr) {
    VLOG(2) << "fastest algorithm: "
            << proto_utils::FromDurationProto(results[idx].run_time())
            << " with algo " << results[idx].conv().algorithm()
            << ", workspace bytes " << results[idx].scratch_bytes();
    algo->set_algorithm({results[idx].conv().algorithm(),
                         results[idx].conv().tensor_ops_enabled()});
    algo->set_scratch_size(results[idx].scratch_bytes());
    if (idx_no_scratch != -1) {
      algo->set_algorithm_no_scratch(
          {results[idx_no_scratch].conv().algorithm(),
           results[idx_no_scratch].conv().tensor_ops_enabled()});
    }
  } else {
    VLOG(2) << "fastest algorithm: "
            << proto_utils::FromDurationProto(results[idx].run_time())
            << " with algo " << (*plans)[idx]->getTag() << ", workspace bytes "
            << (*plans)[idx]->getWorkspaceSize();
    algo->set_algorithm(
        {(*plans)[idx]->getTag(), (*plans)[idx]->get_raw_desc()});
    algo->set_scratch_size((*plans)[idx]->getWorkspaceSize());
    if (idx_no_scratch != -1) {
      algo->set_algorithm_no_scratch(
          {(*plans)[idx_no_scratch]->getTag(),
           (*plans)[idx_no_scratch]->get_raw_desc()});
    }
    algo->set_plan((*plans)[idx]);
    if (idx_no_scratch != -1 && idx_no_scratch != idx) {
      algo->set_plan_no_scratch((*plans)[idx_no_scratch]);
    }
  }
  return Status::OK();
}

}  // namespace tensorflow

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
