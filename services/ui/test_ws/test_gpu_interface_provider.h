// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_TEST_WS_TEST_GPU_INTERFACE_PROVIDER_H_
#define SERVICES_UI_TEST_WS_TEST_GPU_INTERFACE_PROVIDER_H_

#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/ui/ws2/gpu_interface_provider.h"

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace ui {

namespace gpu_host {
class GpuHost;
}

namespace test {

// TestGpuInterfaceProvider used by test_ws.
class TestGpuInterfaceProvider : public ui::ws2::GpuInterfaceProvider {
 public:
  TestGpuInterfaceProvider(ui::gpu_host::GpuHost* gpu_host,
                           discardable_memory::DiscardableSharedMemoryManager*
                               discardable_shared_memory_manager);
  ~TestGpuInterfaceProvider() override;

  // ui::ws2::GpuInterfaceProvider:
  void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) override;
#if defined(USE_OZONE)
  void RegisterOzoneGpuInterfaces(
      service_manager::BinderRegistry* registry) override;
#endif

 private:
  void BindDiscardableSharedMemoryManagerRequest(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request);
  void BindGpuRequest(ui::mojom::GpuRequest request);

  ui::gpu_host::GpuHost* const gpu_host_;
  discardable_memory::DiscardableSharedMemoryManager* const
      discardable_shared_memory_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuInterfaceProvider);
};

}  // namespace test
}  // namespace ui

#endif  // SERVICES_UI_TEST_WS_TEST_GPU_INTERFACE_PROVIDER_H_
