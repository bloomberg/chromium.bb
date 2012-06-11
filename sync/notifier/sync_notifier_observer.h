// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#define SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#pragma once

#include <string>

#include "sync/internal_api/public/syncable/model_type_payload_map.h"

namespace sync_notifier {

enum IncomingNotificationSource {
  // The server is notifying us that one or more datatypes have stale data.
  REMOTE_NOTIFICATION,
  // A chrome datatype is requesting an optimistic refresh of its data.
  LOCAL_NOTIFICATION,
};

class SyncNotifierObserver {
 public:
  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      IncomingNotificationSource source) = 0;
  virtual void OnNotificationStateChange(bool notifications_enabled) = 0;

 protected:
  virtual ~SyncNotifierObserver() {}
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
