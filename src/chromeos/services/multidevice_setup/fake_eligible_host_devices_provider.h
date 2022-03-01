// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_

#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider.h"

namespace chromeos {

namespace multidevice_setup {

// Test EligibleHostDevicesProvider implementation.
class FakeEligibleHostDevicesProvider : public EligibleHostDevicesProvider {
 public:
  FakeEligibleHostDevicesProvider();

  FakeEligibleHostDevicesProvider(const FakeEligibleHostDevicesProvider&) =
      delete;
  FakeEligibleHostDevicesProvider& operator=(
      const FakeEligibleHostDevicesProvider&) = delete;

  ~FakeEligibleHostDevicesProvider() override;

  void set_eligible_host_devices(
      const multidevice::RemoteDeviceRefList eligible_host_devices) {
    eligible_host_devices_ = eligible_host_devices;
  }

  void set_eligible_active_host_devices(
      const multidevice::DeviceWithConnectivityStatusList
          eligible_host_devices) {
    eligible_active_host_devices_ = eligible_host_devices;
  }

 private:
  // EligibleHostDevicesProvider:
  multidevice::RemoteDeviceRefList GetEligibleHostDevices() const override;
  multidevice::DeviceWithConnectivityStatusList GetEligibleActiveHostDevices()
      const override;

  multidevice::RemoteDeviceRefList eligible_host_devices_;
  multidevice::DeviceWithConnectivityStatusList eligible_active_host_devices_;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
