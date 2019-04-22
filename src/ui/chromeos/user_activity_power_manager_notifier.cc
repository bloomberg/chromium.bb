// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/user_activity_power_manager_notifier.h"

#include "chromeos/dbus/power/power_manager_client.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

// Returns a UserActivityType describing |event|.
power_manager::UserActivityType GetUserActivityTypeForEvent(
    const Event* event) {
  if (!event || event->type() != ET_KEY_PRESSED)
    return power_manager::USER_ACTIVITY_OTHER;

  switch (static_cast<const KeyEvent*>(event)->key_code()) {
    case VKEY_BRIGHTNESS_DOWN:
      return power_manager::USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS;
    case VKEY_BRIGHTNESS_UP:
      return power_manager::USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS;
    case VKEY_VOLUME_DOWN:
      return power_manager::USER_ACTIVITY_VOLUME_DOWN_KEY_PRESS;
    case VKEY_VOLUME_MUTE:
      return power_manager::USER_ACTIVITY_VOLUME_MUTE_KEY_PRESS;
    case VKEY_VOLUME_UP:
      return power_manager::USER_ACTIVITY_VOLUME_UP_KEY_PRESS;
    default:
      return power_manager::USER_ACTIVITY_OTHER;
  }
}

}  // namespace

UserActivityPowerManagerNotifier::UserActivityPowerManagerNotifier(
    UserActivityDetector* detector,
    service_manager::Connector* connector)
    : detector_(detector), fingerprint_observer_binding_(this) {
  detector_->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);

  // Connector can be null in tests.
  if (connector) {
    // Treat fingerprint attempts as user activies to turn on the screen.
    // I.e., when user tried to use fingerprint to unlock.
    connector->BindInterface(device::mojom::kServiceName, &fingerprint_ptr_);
    device::mojom::FingerprintObserverPtr observer;
    fingerprint_observer_binding_.Bind(mojo::MakeRequest(&observer));
    fingerprint_ptr_->AddFingerprintObserver(std::move(observer));
  }
}

UserActivityPowerManagerNotifier::~UserActivityPowerManagerNotifier() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  detector_->RemoveObserver(this);
}

void UserActivityPowerManagerNotifier::OnStylusStateChanged(
    ui::StylusState state) {
  if (state == StylusState::REMOVED)
    MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::OnUserActivity(const Event* event) {
  MaybeNotifyUserActivity(GetUserActivityTypeForEvent(event));
}

void UserActivityPowerManagerNotifier::OnAuthScanDone(
    device::mojom::ScanResult scan_result,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::OnSessionFailed() {}

void UserActivityPowerManagerNotifier::OnRestarted() {}

void UserActivityPowerManagerNotifier::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::MaybeNotifyUserActivity(
    power_manager::UserActivityType user_activity_type) {
  base::TimeTicks now = base::TimeTicks::Now();
  // InSeconds() truncates rather than rounding, so it's fine for this
  // comparison.
  if (last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    chromeos::PowerManagerClient::Get()->NotifyUserActivity(user_activity_type);
    last_notify_time_ = now;
  }
}

}  // namespace ui
