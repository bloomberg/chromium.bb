// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google/cacheinvalidation/v2/types.pb.h"
#include "sync/notifier/sync_notifier_helper.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace syncer {

class SyncNotifierHelperTest : public testing::Test {
 protected:
  SyncNotifierHelperTest()
      : kObjectId1(ipc::invalidation::ObjectSource::TEST, "a"),
        kObjectId2(ipc::invalidation::ObjectSource::TEST, "b"),
        kObjectId3(ipc::invalidation::ObjectSource::TEST, "c") {
  }

  invalidation::ObjectId kObjectId1;
  invalidation::ObjectId kObjectId2;
  invalidation::ObjectId kObjectId3;
};

// Basic check that registrations are correctly updated for one handler.
TEST_F(SyncNotifierHelperTest, Basic) {
  SyncNotifierHelper helper;
  StrictMock<MockSyncNotifierObserver> observer;
  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  helper.UpdateRegisteredIds(&observer, ids);

  ObjectIdPayloadMap dispatched_payloads;
  dispatched_payloads[kObjectId1] = "1";
  dispatched_payloads[kObjectId2] = "2";
  dispatched_payloads[kObjectId3] = "3";

  // A object ID with no registration should be ignored.
  ObjectIdPayloadMap expected_payload1;
  expected_payload1[kObjectId1] = "1";
  expected_payload1[kObjectId2] = "2";
  EXPECT_CALL(observer, OnIncomingNotification(expected_payload1,
                                               REMOTE_NOTIFICATION));
  helper.DispatchInvalidationsToHandlers(dispatched_payloads,
                                         REMOTE_NOTIFICATION);

  // Removed object IDs should not be notified, newly-added ones should.
  ids.erase(kObjectId1);
  ids.insert(kObjectId3);
  helper.UpdateRegisteredIds(&observer, ids);

  ObjectIdPayloadMap expected_payload2;
  expected_payload2[kObjectId2] = "2";
  expected_payload2[kObjectId3] = "3";
  EXPECT_CALL(observer, OnIncomingNotification(expected_payload2,
                                               REMOTE_NOTIFICATION));
  helper.DispatchInvalidationsToHandlers(dispatched_payloads,
                                         REMOTE_NOTIFICATION);
}

// Tests that we correctly bucket and dispatch invalidations on multiple objects
// to the corresponding handlers.
TEST_F(SyncNotifierHelperTest, MultipleHandlers) {
  SyncNotifierHelper helper;
  StrictMock<MockSyncNotifierObserver> observer;
  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  helper.UpdateRegisteredIds(&observer, ids);
  StrictMock<MockSyncNotifierObserver> observer2;
  ObjectIdSet ids2;
  ids2.insert(kObjectId3);
  helper.UpdateRegisteredIds(&observer2, ids2);

  ObjectIdPayloadMap expected_payload1;
  expected_payload1[kObjectId1] = "1";
  expected_payload1[kObjectId2] = "2";
  EXPECT_CALL(observer, OnIncomingNotification(expected_payload1,
                                               REMOTE_NOTIFICATION));
  ObjectIdPayloadMap expected_payload2;
  expected_payload2[kObjectId3] = "3";
  EXPECT_CALL(observer2, OnIncomingNotification(expected_payload2,
                                                REMOTE_NOTIFICATION));

  ObjectIdPayloadMap dispatched_payloads;
  dispatched_payloads[kObjectId1] = "1";
  dispatched_payloads[kObjectId2] = "2";
  dispatched_payloads[kObjectId3] = "3";
  helper.DispatchInvalidationsToHandlers(dispatched_payloads,
                                         REMOTE_NOTIFICATION);

  // Also verify that the callbacks for OnNotificationsEnabled/Disabled work.
  EXPECT_CALL(observer, OnNotificationsEnabled());
  EXPECT_CALL(observer2, OnNotificationsEnabled());
  EXPECT_CALL(observer,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  EXPECT_CALL(observer2,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  helper.EmitOnNotificationsEnabled();
  helper.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

// Multiple registrations by different handlers on the same object ID should
// cause a CHECK.
TEST_F(SyncNotifierHelperTest, MultipleRegistration) {
  SyncNotifierHelper helper;
  StrictMock<MockSyncNotifierObserver> observer;
  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  helper.UpdateRegisteredIds(&observer, ids);

  StrictMock<MockSyncNotifierObserver> observer2;
  EXPECT_DEATH({ helper.UpdateRegisteredIds(&observer2, ids); },
               "Duplicate registration for .*");
}

// Make sure that passing an empty set to UpdateRegisteredIds clears the
// corresponding entries for the handler.
TEST_F(SyncNotifierHelperTest, EmptySetUnregisters) {
  SyncNotifierHelper helper;
  StrictMock<MockSyncNotifierObserver> observer;
  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  helper.UpdateRegisteredIds(&observer, ids);
  // Control observer.
  StrictMock<MockSyncNotifierObserver> observer2;
  ObjectIdSet ids2;
  ids2.insert(kObjectId3);
  helper.UpdateRegisteredIds(&observer2, ids2);
  // Unregister the first observer. It should not receive any further callbacks.
  helper.UpdateRegisteredIds(&observer, ObjectIdSet());

  ObjectIdPayloadMap expected_payload2;
  expected_payload2[kObjectId3] = "3";
  EXPECT_CALL(observer2, OnIncomingNotification(expected_payload2,
                                                REMOTE_NOTIFICATION));
  EXPECT_CALL(observer2, OnNotificationsEnabled());
  EXPECT_CALL(observer2,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  ObjectIdPayloadMap dispatched_payloads;
  dispatched_payloads[kObjectId1] = "1";
  dispatched_payloads[kObjectId2] = "2";
  dispatched_payloads[kObjectId3] = "3";
  helper.DispatchInvalidationsToHandlers(dispatched_payloads,
                                         REMOTE_NOTIFICATION);
  helper.EmitOnNotificationsEnabled();
  helper.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

}  // namespace syncer
