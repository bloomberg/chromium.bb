// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_NOTIFICATIONS_DISABLED_REASON_H_
#define SYNC_NOTIFIER_NOTIFICATIONS_DISABLED_REASON_H_
#pragma once

#include "jingle/notifier/listener/push_client_observer.h"

namespace syncer {

enum NotificationsDisabledReason {
  // There is an underlying transient problem (e.g., network- or
  // XMPP-related).
  TRANSIENT_NOTIFICATION_ERROR,
  DEFAULT_NOTIFICATION_ERROR = TRANSIENT_NOTIFICATION_ERROR,
  // Our credentials have been rejected.
  NOTIFICATION_CREDENTIALS_REJECTED,
  // No error (useful as a default value or to avoid keeping a
  // separate bool for notifications enabled/disabled).
  NO_NOTIFICATION_ERROR
};

const char* NotificationsDisabledReasonToString(
    NotificationsDisabledReason reason);

NotificationsDisabledReason FromNotifierReason(
    notifier::NotificationsDisabledReason reason);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_NOTIFICATIONS_DISABLED_REASON_H_
