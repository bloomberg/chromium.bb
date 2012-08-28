// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
#define SYNC_NOTIFIER_INVALIDATION_HANDLER_H_

#include "sync/notifier/object_id_state_map.h"
#include "sync/notifier/notifications_disabled_reason.h"

namespace syncer {

enum IncomingNotificationSource {
  // The server is notifying us that one or more datatypes have stale data.
  REMOTE_NOTIFICATION,
  // A chrome datatype is requesting an optimistic refresh of its data.
  LOCAL_NOTIFICATION,
};

class InvalidationHandler {
 public:
  // Called when notifications are enabled.
  virtual void OnNotificationsEnabled() = 0;

  // Called when notifications are disabled, with the reason in
  // |reason|.
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) = 0;

  // Called when a notification is received.  The per-id states
  // are in |id_state_map| and the source is in |source|.
  virtual void OnIncomingNotification(
      const ObjectIdStateMap& id_state_map,
      IncomingNotificationSource source) = 0;

 protected:
  virtual ~InvalidationHandler() {}
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
