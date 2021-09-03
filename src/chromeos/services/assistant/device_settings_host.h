// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_DEVICE_SETTINGS_HOST_H_
#define CHROMEOS_SERVICES_ASSISTANT_DEVICE_SETTINGS_HOST_H_

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/device_settings_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
class AssistantNotificationController;
}  // namespace ash

namespace chromeos {
namespace assistant {

class DeviceActions;
class ServiceContext;

class COMPONENT_EXPORT(ASSISTANT_SERVICE) DeviceSettingsHost
    : public chromeos::libassistant::mojom::DeviceSettingsDelegate {
 public:
  explicit DeviceSettingsHost(ServiceContext* context);
  DeviceSettingsHost(const DeviceSettingsHost&) = delete;
  DeviceSettingsHost& operator=(const DeviceSettingsHost&) = delete;
  ~DeviceSettingsHost() override;

  void Bind(mojo::PendingReceiver<DeviceSettingsDelegate> pending_receiver);

  // chromeos::libassistant::mojom::DeviceSettingsDelegate implementation:
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override;
  void SetBluetoothEnabled(bool enabled) override;
  void SetDoNotDisturbEnabled(bool enabled) override;
  void SetNightLightEnabled(bool enabled) override;
  void SetScreenBrightnessLevel(double level, bool gradual) override;
  void SetSwitchAccessEnabled(bool enabled) override;
  void SetWifiEnabled(bool enabled) override;

  // Return if any setting has been modified.
  bool has_setting_changed() const { return has_setting_changed_; }
  void reset_has_setting_changed();

 private:
  ServiceContext& context_;

  DeviceActions& device_actions();
  ash::AssistantNotificationController& assistant_notification_controller();

  bool has_setting_changed_ = false;

  mojo::Receiver<DeviceSettingsDelegate> receiver_{this};
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_DEVICE_SETTINGS_HOST_H_
