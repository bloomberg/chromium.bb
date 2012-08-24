// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_
#define SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "sync/notifier/sync_notifier_observer.h"

namespace syncer {

class FakeSyncNotifierObserver : public SyncNotifierObserver {
 public:
  FakeSyncNotifierObserver();
  virtual ~FakeSyncNotifierObserver();

  NotificationsDisabledReason GetNotificationsDisabledReason() const;
  const ObjectIdStateMap& GetLastNotificationIdStateMap() const;
  IncomingNotificationSource GetLastNotificationSource() const;
  int GetNotificationCount() const;

  // SyncNotifierObserver implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const ObjectIdStateMap& id_state_map,
      IncomingNotificationSource source) OVERRIDE;

 private:
  NotificationsDisabledReason reason_;
  ObjectIdStateMap last_id_state_map_;
  IncomingNotificationSource last_source_;
  int notification_count_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_
