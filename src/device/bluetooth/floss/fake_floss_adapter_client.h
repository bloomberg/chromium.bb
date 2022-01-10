// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossAdapterClient
    : public FlossAdapterClient {
 public:
  FakeFlossAdapterClient();
  ~FakeFlossAdapterClient() override;

  static const char kBondedAddress1[];
  static const char kBondedAddress2[];
  // The address of a device without Keyboard nor Display IO capability,
  // triggering Just Works pairing when used in tests.
  static const char kJustWorksAddress[];
  static const char kKeyboardAddress[];
  static const char kPhoneAddress[];
  static const char kOldDeviceAddress[];
  static const uint32_t kPasskey;

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const std::string& adapter_path) override;
  void StartDiscovery(ResponseCallback<Void> callback) override;
  void CancelDiscovery(ResponseCallback<Void> callback) override;
  void CreateBond(ResponseCallback<Void> callback,
                  FlossDeviceId device,
                  BluetoothTransport transport) override;
  void GetConnectionState(ResponseCallback<uint32_t> callback,
                          const FlossDeviceId& device) override;
  void GetBondState(ResponseCallback<uint32_t> callback,
                    const FlossDeviceId& device) override;
  void ConnectAllEnabledProfiles(ResponseCallback<Void> callback,
                                 const FlossDeviceId& device) override;
  void SetPairingConfirmation(ResponseCallback<Void> callback,
                              const FlossDeviceId& device,
                              bool accept) override;
  void SetPin(ResponseCallback<Void> callback,
              const FlossDeviceId& device,
              bool accept,
              const std::vector<uint8_t>& pin) override;
  void GetBondedDevices(
      ResponseCallback<std::vector<FlossDeviceId>> callback) override;

  // Helper for posting a delayed task.
  void PostDelayedTask(base::OnceClosure callback);

  // Test utility to do fake notification to observers.
  void NotifyObservers(
      const base::RepeatingCallback<void(Observer*)>& notify) const;

 private:
  base::WeakPtrFactory<FakeFlossAdapterClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_
