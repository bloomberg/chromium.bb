// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
#define GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
class TestGpuServiceHolder;
}

namespace gpu {

class SharedImageInterface;
class WebGPUInProcessContext;

void OnRequestDeviceCallback(bool is_request_device_success,
                             webgpu::DawnDeviceClientID device_client_id);

namespace webgpu {

class WebGPUImplementation;

}  // namespace webgpu

class WebGPUTest : public testing::Test {
 protected:
  struct Options {
    Options();

    // Shared memory limits
    SharedMemoryLimits shared_memory_limits =
        SharedMemoryLimits::ForWebGPUContext();
  };

  WebGPUTest();
  ~WebGPUTest() override;

  bool WebGPUSupported() const;
  bool WebGPUSharedImageSupported() const;
  void SetUp() override;
  void TearDown() override;

  void Initialize(const Options& options);

  webgpu::WebGPUImplementation* webgpu() const;
  SharedImageInterface* GetSharedImageInterface() const;

  void RunPendingTasks();
  void WaitForCompletion(wgpu::Device device);

  struct DeviceAndClientID {
    wgpu::Device device;
    webgpu::DawnDeviceClientID client_id;
  };
  DeviceAndClientID GetNewDeviceAndClientID();

  viz::TestGpuServiceHolder* GetGpuServiceHolder() {
    return gpu_service_holder_.get();
  }

  const uint32_t kAdapterServiceID = 0u;

 private:
  std::unique_ptr<viz::TestGpuServiceHolder> gpu_service_holder_;
  std::unique_ptr<WebGPUInProcessContext> context_;
  bool is_initialized_ = false;

  webgpu::DawnDeviceClientID next_device_client_id_ = 1;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
