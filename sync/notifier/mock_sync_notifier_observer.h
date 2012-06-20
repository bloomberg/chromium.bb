// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
#define SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
#pragma once

#include <string>

#include "sync/notifier/sync_notifier_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace csync {

class MockSyncNotifierObserver : public SyncNotifierObserver {
 public:
  MockSyncNotifierObserver();
  virtual ~MockSyncNotifierObserver();

  MOCK_METHOD0(OnNotificationsEnabled, void());
  MOCK_METHOD1(OnNotificationsDisabled, void(NotificationsDisabledReason));
  MOCK_METHOD2(OnIncomingNotification,
               void(const syncable::ModelTypePayloadMap&,
                    IncomingNotificationSource));
};

}  // namespace csync

#endif  // SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
