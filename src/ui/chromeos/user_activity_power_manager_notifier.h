// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_
#define UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace service_manager {
class Connector;
}

namespace ui {

class UserActivityDetector;

// Notifies the power manager via D-Bus when the user is active.
class UI_CHROMEOS_EXPORT UserActivityPowerManagerNotifier
    : public InputDeviceEventObserver,
      public UserActivityObserver,
      public device::mojom::FingerprintObserver {
 public:
  // Registers and unregisters itself as an observer of |detector| on
  // construction and destruction.
  UserActivityPowerManagerNotifier(UserActivityDetector* detector,
                                   service_manager::Connector* connector);
  ~UserActivityPowerManagerNotifier() override;

  // InputDeviceEventObserver implementation.
  void OnStylusStateChanged(ui::StylusState state) override;

  // UserActivityObserver implementation.
  void OnUserActivity(const Event* event) override;

  // fingerprint::mojom::FingerprintObserver:
  void OnAuthScanDone(
      uint32_t scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;
  void OnRestarted() override;
  void OnEnrollScanDone(uint32_t scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;

 private:
  // Notifies power manager that the user is active and activity type. No-op if
  // it is within 5 seconds from |last_notify_time_|.
  void MaybeNotifyUserActivity(
      power_manager::UserActivityType user_activity_type);

  UserActivityDetector* detector_;  // not owned

  device::mojom::FingerprintPtr fingerprint_ptr_;
  mojo::Binding<device::mojom::FingerprintObserver>
      fingerprint_observer_binding_;

  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityPowerManagerNotifier);
};

}  // namespace ui

#endif  // UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_
