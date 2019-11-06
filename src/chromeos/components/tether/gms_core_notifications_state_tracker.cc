// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/gms_core_notifications_state_tracker.h"

namespace chromeos {

namespace tether {

GmsCoreNotificationsStateTracker::GmsCoreNotificationsStateTracker() = default;

GmsCoreNotificationsStateTracker::~GmsCoreNotificationsStateTracker() = default;

void GmsCoreNotificationsStateTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GmsCoreNotificationsStateTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GmsCoreNotificationsStateTracker::NotifyGmsCoreNotificationStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnGmsCoreNotificationStateChanged();
}

}  // namespace tether

}  // namespace chromeos
