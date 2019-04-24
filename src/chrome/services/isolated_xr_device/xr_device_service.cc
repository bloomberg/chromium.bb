// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_device_service.h"

#include "base/bind.h"
#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"
#include "chrome/services/isolated_xr_device/xr_test_hook_registration.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

void XrDeviceService::OnDeviceProviderRequest(
    device::mojom::IsolatedXRRuntimeProviderRequest request) {
  mojo::MakeStrongBinding(std::make_unique<IsolatedXRRuntimeProvider>(
                              service_keepalive_.CreateRef()),
                          std::move(request));
}

void XrDeviceService::OnTestHookRequest(
    device_test::mojom::XRTestHookRegistrationRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<XRTestHookRegistration>(service_keepalive_.CreateRef()),
      std::move(request));
}

XrDeviceService::XrDeviceService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {
  // Register device provider here.
  registry_.AddInterface(base::BindRepeating(
      &XrDeviceService::OnDeviceProviderRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &XrDeviceService::OnTestHookRequest, base::Unretained(this)));
}

XrDeviceService::~XrDeviceService() = default;

void XrDeviceService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace device
