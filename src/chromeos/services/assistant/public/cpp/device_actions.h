// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace assistant {

// Subscribes to App list events.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AppListEventSubscriber
    : public base::CheckedObserver {
 public:
  // Called when the android app list changed.
  virtual void OnAndroidAppListRefreshed(
      std::vector<mojom::AndroidAppInfoPtr> apps_info) = 0;
};

// Main interface for |chromeos::assistant::Service| to execute device related
// actions.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) DeviceActions {
 public:
  DeviceActions();
  DeviceActions(const DeviceActions&) = delete;
  DeviceActions& operator=(const DeviceActions&) = delete;
  virtual ~DeviceActions();

  static DeviceActions* Get();

  // Enables or disables WiFi.
  virtual void SetWifiEnabled(bool enabled) = 0;

  // Enables or disables Bluetooth.
  virtual void SetBluetoothEnabled(bool enabled) = 0;

  // Gets the current screen brightness level (0-1.0).
  // The level is set to 0 in the event of an error.
  using GetScreenBrightnessLevelCallback =
      base::OnceCallback<void(bool success, double level)>;
  virtual void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) = 0;

  // Sets the screen brightness level (0-1.0).  If |gradual| is true, the
  // transition will be animated.
  virtual void SetScreenBrightnessLevel(double level, bool gradual) = 0;

  // Enables or disables Night Light.
  virtual void SetNightLightEnabled(bool enabled) = 0;

  // Enables or disables Switch Access.
  virtual void SetSwitchAccessEnabled(bool enabled) = 0;

  // Open the Android app if the app is available. Returns true if app is
  // successfully openned, false otherwise.
  virtual bool OpenAndroidApp(mojom::AndroidAppInfoPtr app_info) = 0;

  // Get the status of the Android app.
  virtual mojom::AppStatus GetAndroidAppStatus(
      const mojom::AndroidAppInfo& app_info) = 0;

  // Launch Android intent. The intent is encoded as a URI string.
  // See Intent.toUri().
  virtual void LaunchAndroidIntent(const std::string& intent) = 0;

  // Register App list event subscriber.
  virtual void AddAppListEventSubscriber(
      AppListEventSubscriber* subscriber) = 0;
  virtual void RemoveAppListEventSubscriber(
      AppListEventSubscriber* subscriber) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_
