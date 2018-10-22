// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace multidevice_setup {

// Test EligibleHostDevicesProvider implementation.
class FakeEligibleHostDevicesProvider : public EligibleHostDevicesProvider {
 public:
  FakeEligibleHostDevicesProvider();
  ~FakeEligibleHostDevicesProvider() override;

  void set_eligible_host_devices(
      const cryptauth::RemoteDeviceRefList eligible_host_devices) {
    eligible_host_devices_ = eligible_host_devices;
  }

 private:
  // EligibleHostDevicesProvider:
  cryptauth::RemoteDeviceRefList GetEligibleHostDevices() const override;

  cryptauth::RemoteDeviceRefList eligible_host_devices_;

  DISALLOW_COPY_AND_ASSIGN(FakeEligibleHostDevicesProvider);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
