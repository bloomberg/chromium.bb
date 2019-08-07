// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_

#include "chromeos/services/device_sync/remote_device_provider.h"

namespace chromeos {

namespace device_sync {

// Test double for RemoteDeviceProvider.
class FakeRemoteDeviceProvider : public RemoteDeviceProvider {
 public:
  FakeRemoteDeviceProvider();
  ~FakeRemoteDeviceProvider() override;

  void set_synced_remote_devices(
      const multidevice::RemoteDeviceList& synced_remote_devices) {
    synced_remote_devices_ = synced_remote_devices;
  }

  void NotifyObserversDeviceListChanged();

  // RemoteDeviceProvider:
  const multidevice::RemoteDeviceList& GetSyncedDevices() const override;

 private:
  multidevice::RemoteDeviceList synced_remote_devices_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteDeviceProvider);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_
