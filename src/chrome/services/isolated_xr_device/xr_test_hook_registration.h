// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_REGISTRATION_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_REGISTRATION_H_

#include <memory>

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace device {
class XRTestHookWrapper;

class XRTestHookRegistration
    : public device_test::mojom::XRTestHookRegistration {
 public:
  explicit XRTestHookRegistration(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~XRTestHookRegistration() final;

  // device_test::mojom::XRTestHookRegistration
  void SetTestHook(
      device_test::mojom::XRTestHookPtr hook,
      device_test::mojom::XRTestHookRegistration::SetTestHookCallback callback)
      override;

 private:
  std::unique_ptr<XRTestHookWrapper> wrapper_;
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_REGISTRATION_H_
