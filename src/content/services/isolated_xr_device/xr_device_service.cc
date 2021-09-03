// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_device_service.h"

#include "build/build_config.h"
#include "content/services/isolated_xr_device/xr_runtime_provider.h"
#include "content/services/isolated_xr_device/xr_service_test_hook.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_WIN)
#include "base/win/com_init_check_hook.h"
#endif  // defined(OS_WIN)

namespace device {

XrDeviceService::XrDeviceService(
    mojo::PendingReceiver<device::mojom::XRDeviceService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {
#if defined(OS_WIN)
  base::win::ComInitCheckHook::DisableCOMChecksForProcess();
#endif  // defined(OS_WIN)
}

XrDeviceService::~XrDeviceService() = default;

void XrDeviceService::BindRuntimeProvider(
    mojo::PendingReceiver<mojom::IsolatedXRRuntimeProvider> receiver,
    mojo::PendingRemote<mojom::XRDeviceServiceHost> device_service_host) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<IsolatedXRRuntimeProvider>(
          std::move(device_service_host), io_task_runner_),
      std::move(receiver));
}

void XrDeviceService::BindTestHook(
    mojo::PendingReceiver<device_test::mojom::XRServiceTestHook> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<XRServiceTestHook>(),
                              std::move(receiver));
}

}  // namespace device
