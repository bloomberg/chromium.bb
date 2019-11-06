// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_gms_core_notifications_state_tracker.h"

namespace chromeos {

namespace tether {

FakeGmsCoreNotificationsStateTracker::FakeGmsCoreNotificationsStateTracker() =
    default;

FakeGmsCoreNotificationsStateTracker::~FakeGmsCoreNotificationsStateTracker() =
    default;

void FakeGmsCoreNotificationsStateTracker::
    NotifyGmsCoreNotificationStateChanged() {
  GmsCoreNotificationsStateTracker::NotifyGmsCoreNotificationStateChanged();
}

std::vector<std::string> FakeGmsCoreNotificationsStateTracker::
    GetGmsCoreNotificationsDisabledDeviceNames() {
  return device_names_;
}

}  // namespace tether

}  // namespace chromeos
