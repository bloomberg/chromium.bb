// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/test_ws/test_gpu_interface_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/ui/gpu_host/gpu_host.h"

namespace ui {
namespace test {

TestGpuInterfaceProvider::TestGpuInterfaceProvider(
    ui::gpu_host::GpuHost* gpu_host,
    discardable_memory::DiscardableSharedMemoryManager*
        discardable_shared_memory_manager)
    : gpu_host_(gpu_host),
      discardable_shared_memory_manager_(discardable_shared_memory_manager) {}

TestGpuInterfaceProvider::~TestGpuInterfaceProvider() = default;

void TestGpuInterfaceProvider::RegisterGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface(base::BindRepeating(
      &TestGpuInterfaceProvider::BindDiscardableSharedMemoryManagerRequest,
      base::Unretained(this)));
  registry->AddInterface(base::BindRepeating(
      &TestGpuInterfaceProvider::BindGpuRequest, base::Unretained(this)));
}

#if defined(USE_OZONE)
void TestGpuInterfaceProvider::RegisterOzoneGpuInterfaces(
    service_manager::BinderRegistry* registry) {}
#endif

void TestGpuInterfaceProvider::BindDiscardableSharedMemoryManagerRequest(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  // |discardable_shared_memory_manager_| could be null. See test_ws.cc for
  // details.
  if (!discardable_shared_memory_manager_) {
    NOTIMPLEMENTED_LOG_ONCE();
    return;
  }

  discardable_shared_memory_manager_->Bind(std::move(request),
                                           service_manager::BindSourceInfo());
}

void TestGpuInterfaceProvider::BindGpuRequest(ui::mojom::GpuRequest request) {
  // |gpu_host_| could be null. See test_ws.cc for details.
  if (!gpu_host_) {
    NOTIMPLEMENTED_LOG_ONCE();
    return;
  }

  gpu_host_->Add(std::move(request));
}

}  // namespace test
}  // namespace ui
