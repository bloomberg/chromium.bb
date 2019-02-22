// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_device_service.h"

#include "base/bind.h"
#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"
#include "chrome/services/isolated_xr_device/xr_test_hook_registration.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace device {

std::unique_ptr<service_manager::Service>
XrDeviceService::CreateXrDeviceService() {
  return std::make_unique<XrDeviceService>();
}

void XrDeviceService::OnDeviceProviderRequest(
    device::mojom::IsolatedXRRuntimeProviderRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<IsolatedXRRuntimeProvider>(ref_factory_->CreateRef()),
      std::move(request));
}

void XrDeviceService::OnTestHookRequest(
    device_test::mojom::XRTestHookRegistrationRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<XRTestHookRegistration>(ref_factory_->CreateRef()),
      std::move(request));
}

XrDeviceService::XrDeviceService() {
  // Register device provider here.
  registry_.AddInterface(base::BindRepeating(
      &XrDeviceService::OnDeviceProviderRequest, base::Unretained(this)));

  registry_.AddInterface(base::BindRepeating(
      &XrDeviceService::OnTestHookRequest, base::Unretained(this)));
}

XrDeviceService::~XrDeviceService() {}

void XrDeviceService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      context()->CreateQuitClosure());
}

void XrDeviceService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace device
