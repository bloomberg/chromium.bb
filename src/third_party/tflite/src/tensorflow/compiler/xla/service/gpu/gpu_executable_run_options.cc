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

#include "tensorflow/compiler/xla/service/gpu/gpu_executable_run_options.h"

#include "absl/algorithm/container.h"
#include "tensorflow/compiler/xla/status_macros.h"

namespace xla {
namespace gpu {

NcclCliqueKey::NcclCliqueKey(std::vector<GlobalDeviceId> devices)
    : devices_(std::move(devices)) {}

std::string NcclCliqueKey::ToString() const {
  return GlobalDeviceIdsToString(devices_);
}

GpuExecutableRunOptions& GpuExecutableRunOptions::set_gpu_global_device_ids(
    absl::optional<std::vector<GlobalDeviceId>> gpu_global_device_ids) {
  gpu_global_device_ids_ = std::move(gpu_global_device_ids);
  return *this;
}

const absl::optional<std::vector<GlobalDeviceId>>&
GpuExecutableRunOptions::gpu_global_device_ids() const {
  return gpu_global_device_ids_;
}

GpuExecutableRunOptions& GpuExecutableRunOptions::set_nccl_unique_id_callback(
    NcclUniqueIdCallback nccl_unique_id_callback) {
  nccl_unique_id_callback_ = std::move(nccl_unique_id_callback);
  return *this;
}

const NcclUniqueIdCallback& GpuExecutableRunOptions::nccl_unique_id_callback()
    const {
  return nccl_unique_id_callback_;
}

NcclExecuteParams::NcclExecuteParams(
    const ServiceExecutableRunOptions& run_options, se::Stream* stream)
    : stream(stream),
      run_id(run_options.run_options().run_id()),
      device_assn(run_options.run_options().device_assignment()) {
  const GpuExecutableRunOptions* gpu_options =
      run_options.run_options().gpu_executable_run_options();
  gpu_global_device_ids = gpu_options && gpu_options->gpu_global_device_ids()
                              ? &*gpu_options->gpu_global_device_ids()
                              : nullptr;
  nccl_unique_id_callback =
      gpu_options && gpu_options->nccl_unique_id_callback()
          ? &gpu_options->nccl_unique_id_callback()
          : nullptr;
}

StatusOr<GlobalDeviceId> NcclExecuteParams::GetGlobalDeviceId() const {
  int64_t local_device_ordinal = stream->parent()->device_ordinal();
  if (gpu_global_device_ids) {
    TF_RET_CHECK(0 <= local_device_ordinal &&
                 local_device_ordinal < gpu_global_device_ids->size());
    return (*gpu_global_device_ids)[local_device_ordinal];
  } else {
    // No local -> global mapping was provided; assume the identity mapping.
    return GlobalDeviceId(local_device_ordinal);
  }
}

}  // namespace gpu
}  // namespace xla
