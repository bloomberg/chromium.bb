// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_
#define DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_

#include "device/gamepad/abstract_haptic_gamepad.h"

#include "device/gamepad/gamepad_standard_mappings.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// NintendoController represents a gaming input for a Nintendo console. In some
// cases, multiple discrete devices can be associated to form one logical
// input. A single NintendoController instance may represent a discrete device,
// or may represent two devices associated to form a composite input. (For
// instance, a pair of matched Joy-Cons may be treated as a single gamepad.)
//
// Switch devices must be initialized in order to provide a good experience.
// Devices that connect over Bluetooth (Joy-Cons and the Pro Controller) default
// to a HID interface that exposes only partial functionality. Devices that
// connect over USB (Pro Controller and Charging Grip) send no controller data
// reports until the initialization sequence is started. In both cases,
// initialization is necessary in order to configure device LEDs, enable and
// disable device features, and fetch calibration data.
//
// After initialization, the Joy-Con or Pro Controller should be in the
// following state:
// * Faster baud rate, if connected over USB.
// * Player indicator light 1 is lit, others are unlit.
// * Home light is 100% on.
// * Accelerometer and gyroscope inputs are enabled with default sensitivity.
// * Vibration is enabled.
// * NFC is disabled.
// * Configured to send controller data reports with report ID 0x30.
//
// Joy-Cons and the Pro Controller provide uncalibrated joystick input. The
// devices store factory calibration data for scaling the joystick axes and
// applying deadzones.
//
// Dual-rumble vibration effects are supported for both discrete devices and for
// composite devices. Joy-Cons and the Pro Controller use linear resonant
// actuators (LRAs) instead of the traditional eccentric rotating mass (ERM)
// vibration actuators. The LRAs are controlled using frequency and amplitude
// pairs rather than a single magnitude value. To simulate a dual-rumble effect,
// the left and right LRAs are set to vibrate at different frequencies as though
// they were the strong and weak ERM actuators. The amplitudes are set to the
// strong and weak effect magnitudes. When a vibration effect is played on a
// composite device, the effect is split so that each component receives one
// channel of the dual-rumble effect.
class NintendoController : public AbstractHapticGamepad {
 public:
  ~NintendoController() override;

  // Create a NintendoController for a newly-connected HID device.
  static std::unique_ptr<NintendoController> Create(
      int source_id,
      mojom::HidDeviceInfoPtr device_info,
      mojom::HidManager* hid_manager);

  // Create a composite NintendoController from two already-connected HID
  // devices.
  static std::unique_ptr<NintendoController> CreateComposite(
      int source_id,
      std::unique_ptr<NintendoController> composite1,
      std::unique_ptr<NintendoController> composite2,
      mojom::HidManager* hid_manager);

  // Return true if |vendor_id| and |product_id| describe a Nintendo controller.
  static bool IsNintendoController(uint16_t vendor_id, uint16_t product_id);

  // Decompose a composite device and return a vector of its subcomponents.
  // Return an empty vector if the device is non-composite.
  std::vector<std::unique_ptr<NintendoController>> Decompose();

  // Begin the initialization sequence. When the device is ready to report
  // controller data, |device_ready_closure| is called.
  void Open(base::OnceClosure device_ready_closure);

  // Return true if the device has completed initialization and is ready to
  // report controller data.
  bool IsOpen() const { return false; }

  // Return true if the device is ready to be assigned a gamepad slot.
  bool IsUsable() const;

  // Return true if the device is composed of multiple subdevices.
  bool IsComposite() const { return is_composite_; }

  // Return the source ID assigned to this device.
  int GetSourceId() const { return source_id_; }

  // Return the bus type for this controller.
  GamepadBusType GetBusType() const { return bus_type_; }

  // Return the mapping function for this controller.
  GamepadStandardMappingFunction GetMappingFunction() const;

  // Return true if |guid| is the device GUID for any of the HID devices
  // opened for this controller.
  bool HasGuid(const std::string& guid) const;

  // Perform one-time initialization for the gamepad data in |pad|.
  void InitializeGamepadState(bool has_standard_mapping, Gamepad& pad) const;

  // Update the button and axis state in |pad|.
  void UpdateGamepadState(Gamepad& pad) const;

  // Return the handedness of the device, or GamepadHand::kNone if the device
  // is not intended to be used in a specific hand.
  GamepadHand GetGamepadHand() const;

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  void SetVibration(double strong_magnitude, double weak_magnitude) override;

  NintendoController(int source_id,
                     mojom::HidDeviceInfoPtr device_info,
                     mojom::HidManager* hid_manager);
  NintendoController(int source_id,
                     std::unique_ptr<NintendoController> composite1,
                     std::unique_ptr<NintendoController> composite2,
                     mojom::HidManager* hid_manager);

 private:
  // Initiate a connection request to the HID device.
  void Connect(mojom::HidManager::ConnectCallback callback);

  // Completion callback for the HID connection request.
  void OnConnect(mojom::HidConnectionPtr connection);

  // Initiate the sequence of exchanges to prepare the device to provide
  // controller data.
  void StartInitSequence();

  // An ID value to identify this device among other devices enumerated by the
  // data fetcher.
  const int source_id_;

  // The bus type for the underlying HID device.
  GamepadBusType bus_type_ = GAMEPAD_BUS_UNKNOWN;

  // A composite device contains up to two Joy-Cons as sub-devices.
  bool is_composite_ = false;

  // Left and right sub-devices for a composite device.
  std::unique_ptr<NintendoController> composite_left_;
  std::unique_ptr<NintendoController> composite_right_;

  // The most recent gamepad state.
  Gamepad pad_;

  // Information about the underlying HID device.
  mojom::HidDeviceInfoPtr device_info_;

  // HID service manager.
  mojom::HidManager* const hid_manager_;

  // The open connection to the underlying HID device.
  mojom::HidConnectionPtr connection_;

  // A closure, provided in the call to Open, to be called once the device
  // becomes ready.
  base::OnceClosure device_ready_closure_;

  base::WeakPtrFactory<NintendoController> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_
