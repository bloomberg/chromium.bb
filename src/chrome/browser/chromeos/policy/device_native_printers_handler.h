// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_NATIVE_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_NATIVE_PRINTERS_HANDLER_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

namespace policy {

class PolicyService;

class DeviceNativePrintersHandler
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  explicit DeviceNativePrintersHandler(PolicyService* policy_service);
  ~DeviceNativePrintersHandler() override;

  // DeviceCloudExternalDataPolicyObserver::Delegate:
  void OnDeviceExternalDataSet(const std::string& policy) override;
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;

  void Shutdown();

 private:
  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_native_printers_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceNativePrintersHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_NATIVE_PRINTERS_HANDLER_H_
