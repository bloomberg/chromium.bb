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

#include "tensorflow/compiler/xla/service/tpu_computation_placer.h"

#include "tensorflow/core/tpu/tpu_api.h"
#include "tensorflow/stream_executor/tpu/status_helper.h"
#include "tensorflow/stream_executor/tpu/tpu_platform.h"
#include "tensorflow/stream_executor/tpu/tpu_platform_id.h"

namespace tensorflow {
namespace tpu {

template <typename T>
using StatusOr = TpuComputationPlacer::StatusOr<T>;

TpuComputationPlacer::TpuComputationPlacer() {
  placer_ = tensorflow::tpu::ExecutorApiFn()->TpuComputationPlacer_NewFn();
}

TpuComputationPlacer::~TpuComputationPlacer() {
  tensorflow::tpu::ExecutorApiFn()->TpuComputationPlacer_FreeFn(placer_);
}

StatusOr<int> TpuComputationPlacer::DeviceId(int replica, int computation,
                                             int replica_count,
                                             int computation_count) {
  LOG(FATAL) << "Unimplemented.";
}

StatusOr<xla::DeviceAssignment> TpuComputationPlacer::AssignDevices(
    int replica_count, int computation_count) {
  StatusHelper status;
  xla::DeviceAssignment result(replica_count, computation_count);
  tensorflow::tpu::ExecutorApiFn()->TpuComputationPlacer_AssignDevicesFn(
      placer_, replica_count, computation_count, result.data(),
      status.c_status);
  if (!status.ok()) {
    return status.status();
  }
  return result;
}

/*static*/ StatusOr<xla::DeviceAssignment>
TpuComputationPlacer::AssignLocalDevices(TpuHostLocationExternal host_location,
                                         int replica_count,
                                         int computation_count) {
  StatusHelper status;
  xla::DeviceAssignment result(replica_count, computation_count);
  tensorflow::tpu::ExecutorApiFn()->TpuComputationPlacer_AssignLocalDevicesFn(
      host_location.impl(), replica_count, computation_count, result.data(),
      status.c_status);
  if (!status.ok()) {
    return status.status();
  }
  return result;
}

static std::unique_ptr<xla::ComputationPlacer> CreateTpuComputationPlacer() {
  return std::make_unique<TpuComputationPlacer>();
}

static bool InitModule() {
  xla::ComputationPlacer::RegisterComputationPlacer(GetTpuPlatformId(),
                                                    CreateTpuComputationPlacer);
  return true;
}
static bool module_initialized = InitModule();

}  // namespace tpu
}  // namespace tensorflow
