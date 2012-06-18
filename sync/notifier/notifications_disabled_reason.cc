// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/notifications_disabled_reason.h"

#include "base/logging.h"

namespace sync_notifier {

const char* NotificationsDisabledReasonToString(
    NotificationsDisabledReason reason) {
  switch (reason) {
    case NO_NOTIFICATION_ERROR:
      return "NO_NOTIFICATION_ERROR";
    case TRANSIENT_NOTIFICATION_ERROR:
      return "TRANSIENT_NOTIFICATION_ERROR";
    case NOTIFICATION_CREDENTIALS_REJECTED:
      return "NOTIFICATION_CREDENTIALS_REJECTED";
    default:
      NOTREACHED();
      return "UNKNOWN";
  }
}

NotificationsDisabledReason FromNotifierReason(
    notifier::NotificationsDisabledReason reason) {
  switch (reason) {
    case notifier::NO_NOTIFICATION_ERROR:
      return NO_NOTIFICATION_ERROR;
    case notifier::TRANSIENT_NOTIFICATION_ERROR:
      return TRANSIENT_NOTIFICATION_ERROR;
    case notifier::NOTIFICATION_CREDENTIALS_REJECTED:
      return NOTIFICATION_CREDENTIALS_REJECTED;
    default:
      NOTREACHED();
      return TRANSIENT_NOTIFICATION_ERROR;
  }
}

}  // sync_notifier
