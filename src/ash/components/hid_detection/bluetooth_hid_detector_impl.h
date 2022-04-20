// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
#define ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_

#include "ash/components/hid_detection/bluetooth_hid_detector.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace hid_detection {

// Concrete BluetoothHidDetector implementation that uses CrosBluetoothConfig.
class BluetoothHidDetectorImpl
    : public BluetoothHidDetector,
      public chromeos::bluetooth_config::mojom::SystemPropertiesObserver,
      public chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate,
      public chromeos::bluetooth_config::mojom::DevicePairingDelegate {
 public:
  BluetoothHidDetectorImpl();
  ~BluetoothHidDetectorImpl() override;

  // BluetoothHidDetector:
  void StartBluetoothHidDetection(
      Delegate* delegate,
      InputDevicesStatus input_devices_status) override;
  void StopBluetoothHidDetection() override;
  const BluetoothHidDetectionStatus GetBluetoothHidDetectionStatus() override;

 private:
  // States used for internal state machine.
  enum State {
    // HID detection is currently not active.
    kNotStarted,

    // HID detection has began activating.
    kStarting,

    // HID detection has began activating and is waiting for the Bluetooth
    // adapter to be enabled.
    kEnablingAdapter,

    // HID detection is fully active and is now searching for devices.
    kDetecting,

    // HID detection is paused due to the Bluetooth adapter becoming unenabled
    // for external reasons.
    kStoppedExternally,
  };

  // chromeos::bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(
      chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
          properties) override;

  // chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate
  void OnBluetoothDiscoveryStarted(
      mojo::PendingRemote<
          chromeos::bluetooth_config::mojom::DevicePairingHandler> handler)
      override;
  void OnBluetoothDiscoveryStopped() override;
  void OnDiscoveredDevicesListChanged(
      std::vector<
          chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
          discovered_devices) override;

  // mojom::DevicePairingDelegate:
  void RequestPinCode(RequestPinCodeCallback callback) override;
  void RequestPasskey(RequestPasskeyCallback callback) override;
  void DisplayPinCode(const std::string& pin_code,
                      mojo::PendingReceiver<
                          chromeos::bluetooth_config::mojom::KeyEnteredHandler>
                          handler) override;
  void DisplayPasskey(const std::string& passkey,
                      mojo::PendingReceiver<
                          chromeos::bluetooth_config::mojom::KeyEnteredHandler>
                          handler) override;
  void ConfirmPasskey(const std::string& passkey,
                      ConfirmPasskeyCallback callback) override;
  void AuthorizePairing(AuthorizePairingCallback callback) override;

  bool ShouldAttemptToPairWithDevice(
      const chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr&
          device);

  void ProcessQueue();
  void OnPairDevice(
      chromeos::bluetooth_config::mojom::PairingResult pairing_result);

  // Resets properties related to discovery, pairing handlers and queueing.
  void ResetDiscoveryState();

  // Map that contains the ids of the devices in |queue_|.
  base::flat_set<std::string> queued_device_ids_;

  // The queue of devices that will be attempted to be paired with.
  std::unique_ptr<base::queue<
      chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>>
      queue_ = std::make_unique<base::queue<
          chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>>();

  // The device currently being paired with.
  absl::optional<
      chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
      current_pairing_device_;

  Delegate* delegate_ = nullptr;
  InputDevicesStatus input_devices_status_;
  State state_ = kNotStarted;

  mojo::Remote<chromeos::bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;
  mojo::Receiver<chromeos::bluetooth_config::mojom::SystemPropertiesObserver>
      system_properties_observer_receiver_{this};
  mojo::Receiver<chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate>
      bluetooth_discovery_delegate_receiver_{this};
  mojo::Remote<chromeos::bluetooth_config::mojom::DevicePairingHandler>
      device_pairing_handler_remote_;
  mojo::Receiver<chromeos::bluetooth_config::mojom::DevicePairingDelegate>
      device_pairing_delegate_receiver_{this};

  base::WeakPtrFactory<BluetoothHidDetectorImpl> weak_ptr_factory_{this};
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
