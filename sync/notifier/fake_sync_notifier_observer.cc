// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_sync_notifier_observer.h"

namespace syncer {

FakeSyncNotifierObserver::FakeSyncNotifierObserver()
    : reason_(TRANSIENT_NOTIFICATION_ERROR),
      last_source_(LOCAL_NOTIFICATION),
      notification_count_(0) {}

FakeSyncNotifierObserver::~FakeSyncNotifierObserver() {}

NotificationsDisabledReason
FakeSyncNotifierObserver::GetNotificationsDisabledReason() const {
  return reason_;
}

const ObjectIdStateMap&
FakeSyncNotifierObserver::GetLastNotificationIdStateMap() const {
  return last_id_state_map_;
}

IncomingNotificationSource
FakeSyncNotifierObserver::GetLastNotificationSource() const {
  return last_source_;
}

int FakeSyncNotifierObserver::GetNotificationCount() const {
  return notification_count_;
}

void FakeSyncNotifierObserver::OnNotificationsEnabled() {
  reason_ = NO_NOTIFICATION_ERROR;
}

void FakeSyncNotifierObserver::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  reason_ = reason;
}

void FakeSyncNotifierObserver::OnIncomingNotification(
    const ObjectIdStateMap& id_state_map,
    IncomingNotificationSource source) {
  last_id_state_map_ = id_state_map;
  last_source_ = source;
  ++notification_count_;
}

}  // namespace syncer
