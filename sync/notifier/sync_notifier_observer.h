// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#define SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#pragma once

#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/notifier/notifications_disabled_reason.h"

namespace csync {

enum IncomingNotificationSource {
  // The server is notifying us that one or more datatypes have stale data.
  REMOTE_NOTIFICATION,
  // A chrome datatype is requesting an optimistic refresh of its data.
  LOCAL_NOTIFICATION,
};

class SyncNotifierObserver {
 public:
  // Called when notifications are enabled.
  virtual void OnNotificationsEnabled() = 0;

  // Called when notifications are disabled, with the reason in
  // |reason|.
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) = 0;

  // Called when a notification is received.  The per-type payloads
  // are in |type_payloads| and the source is in |source|.
  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      IncomingNotificationSource source) = 0;

 protected:
  virtual ~SyncNotifierObserver() {}
};

}  // namespace csync

#endif  // SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
