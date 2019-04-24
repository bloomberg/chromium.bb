// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace device {

class XrDeviceService : public service_manager::Service {
 public:
  explicit XrDeviceService(service_manager::mojom::ServiceRequest request);
  ~XrDeviceService() override;

  void OnDeviceProviderRequest(
      device::mojom::IsolatedXRRuntimeProviderRequest request);
  void OnTestHookRequest(
      device_test::mojom::XRTestHookRegistrationRequest request);

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(XrDeviceService);
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
