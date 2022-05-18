// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
#define ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace hid_detection {

// TODO(gordonseto): Move this to HidDetectionManager when that class is
// created.
struct BluetoothHidPairingState {
  BluetoothHidPairingState(const std::string& code, uint8_t num_keys_entered);
  BluetoothHidPairingState(BluetoothHidPairingState&& other);
  BluetoothHidPairingState& operator=(BluetoothHidPairingState&& other);
  ~BluetoothHidPairingState();

  // The code required to be entered for the HID to pair.
  std::string code;

  // The number of keys of the code which have been entered.
  uint8_t num_keys_entered;
};

// Manages searching for unpaired Bluetooth human interactive devices and
// automatically attempting to pairing with them if their device type is not
// currently paired with.
class BluetoothHidDetector {
 public:
  struct InputDevicesStatus {
    bool pointer_is_missing;
    bool keyboard_is_missing;
  };

  enum class BluetoothHidType {
    // A mouse, trackball, touchpad, etc.
    kPointer,
    kKeyboard,
    kKeyboardPointerCombo
  };

  // Struct representing a Bluetooth human-interactive device.
  struct BluetoothHidMetadata {
    BluetoothHidMetadata(std::string name, BluetoothHidType type);
    BluetoothHidMetadata(BluetoothHidMetadata&& other);
    BluetoothHidMetadata& operator=(BluetoothHidMetadata&& other);
    ~BluetoothHidMetadata();

    std::string name;
    BluetoothHidType type;
  };

  // Struct representing the current status of BluetoothHidDetector.
  struct BluetoothHidDetectionStatus {
    BluetoothHidDetectionStatus(
        absl::optional<BluetoothHidMetadata> current_pairing_device,
        absl::optional<BluetoothHidPairingState> pairing_state);
    BluetoothHidDetectionStatus(BluetoothHidDetectionStatus&& other);
    BluetoothHidDetectionStatus& operator=(BluetoothHidDetectionStatus&& other);
    ~BluetoothHidDetectionStatus();

    // The metadata of the device currently being paired with.
    absl::optional<BluetoothHidMetadata> current_pairing_device;

    // Set if the current pairing requires a code that should be displayed to
    // the user to enter.
    absl::optional<BluetoothHidPairingState> pairing_state;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked whenever any Bluetooth detection status property changes.
    virtual void OnBluetoothHidStatusChanged() = 0;
  };

  virtual ~BluetoothHidDetector();

  // Begins scanning for Bluetooth devices. Invokes
  // OnBluetoothHidStatusChanged() for |delegate| whenever the HID detection
  // status updates. Calling this method when HID detection has already started
  // is an error.
  void StartBluetoothHidDetection(Delegate* delegate,
                                  InputDevicesStatus input_devices_status);

  // Stops scanning for Bluetooth devices. Calling this method when HID
  // detection has not been started is an error.
  void StopBluetoothHidDetection();

  // Informs BluetoothHidDetector which HID types have been connected.
  virtual void SetInputDevicesStatus(
      InputDevicesStatus input_devices_status) = 0;

  // Fetches the current Bluetooth HID detection status.
  virtual const BluetoothHidDetectionStatus
  GetBluetoothHidDetectionStatus() = 0;

 protected:
  BluetoothHidDetector();

  // Implementation-specific version of StartBluetoothHidDetection().
  virtual void PerformStartBluetoothHidDetection(
      InputDevicesStatus input_devices_status) = 0;

  // Implementation-specific version of StopBluetoothHidDetection().
  virtual void PerformStopBluetoothHidDetection() = 0;

  // Notifies |delegate_| of status changes; should be called by derived
  // types to notify observers of status changes.
  void NotifyBluetoothHidDetectionStatusChanged();

 private:
  Delegate* delegate_ = nullptr;
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
