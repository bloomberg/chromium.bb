// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_invalidation_handler.h"

namespace syncer {

FakeInvalidationHandler::FakeInvalidationHandler()
    : reason_(TRANSIENT_NOTIFICATION_ERROR),
      last_source_(LOCAL_NOTIFICATION),
      notification_count_(0) {}

FakeInvalidationHandler::~FakeInvalidationHandler() {}

NotificationsDisabledReason
FakeInvalidationHandler::GetNotificationsDisabledReason() const {
  return reason_;
}

const ObjectIdStateMap&
FakeInvalidationHandler::GetLastNotificationIdStateMap() const {
  return last_id_state_map_;
}

IncomingNotificationSource
FakeInvalidationHandler::GetLastNotificationSource() const {
  return last_source_;
}

int FakeInvalidationHandler::GetNotificationCount() const {
  return notification_count_;
}

void FakeInvalidationHandler::OnNotificationsEnabled() {
  reason_ = NO_NOTIFICATION_ERROR;
}

void FakeInvalidationHandler::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  reason_ = reason;
}

void FakeInvalidationHandler::OnIncomingNotification(
    const ObjectIdStateMap& id_state_map,
    IncomingNotificationSource source) {
  last_id_state_map_ = id_state_map;
  last_source_ = source;
  ++notification_count_;
}

}  // namespace syncer
