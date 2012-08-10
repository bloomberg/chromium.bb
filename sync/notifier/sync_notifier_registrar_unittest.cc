// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google/cacheinvalidation/types.pb.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/notifier/sync_notifier_registrar.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::InSequence;
using testing::Mock;
using testing::StrictMock;

class SyncNotifierRegistrarTest : public testing::Test {
 protected:
  SyncNotifierRegistrarTest()
      : kObjectId1(ipc::invalidation::ObjectSource::TEST, "a"),
        kObjectId2(ipc::invalidation::ObjectSource::TEST, "b"),
        kObjectId3(ipc::invalidation::ObjectSource::TEST, "c"),
        kObjectId4(ipc::invalidation::ObjectSource::TEST, "d") {
  }

  const invalidation::ObjectId kObjectId1;
  const invalidation::ObjectId kObjectId2;
  const invalidation::ObjectId kObjectId3;
  const invalidation::ObjectId kObjectId4;
};

// Register a handler, register some IDs for that handler, and then unregister
// the handler, dispatching invalidations in between.  The handler should only
// see invalidations when its registered and its IDs are registered.
TEST_F(SyncNotifierRegistrarTest, Basic) {
  StrictMock<MockSyncNotifierObserver> handler;

  SyncNotifierRegistrar registrar;

  registrar.RegisterHandler(&handler);

  ObjectIdPayloadMap payloads;
  payloads[kObjectId1] = "1";
  payloads[kObjectId2] = "2";
  payloads[kObjectId3] = "3";

  // Should be ignored since no IDs are registered to |handler|.
  registrar.DispatchInvalidationsToHandlers(payloads, REMOTE_NOTIFICATION);

  Mock::VerifyAndClearExpectations(&handler);

  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  registrar.UpdateRegisteredIds(&handler, ids);

  {
    ObjectIdPayloadMap expected_payloads;
    expected_payloads[kObjectId1] = "1";
    expected_payloads[kObjectId2] = "2";
    EXPECT_CALL(handler, OnIncomingNotification(expected_payloads,
                                                REMOTE_NOTIFICATION));
  }

  registrar.DispatchInvalidationsToHandlers(payloads, REMOTE_NOTIFICATION);

  Mock::VerifyAndClearExpectations(&handler);

  ids.erase(kObjectId1);
  ids.insert(kObjectId3);
  registrar.UpdateRegisteredIds(&handler, ids);

  {
    ObjectIdPayloadMap expected_payloads;
    expected_payloads[kObjectId2] = "2";
    expected_payloads[kObjectId3] = "3";
    EXPECT_CALL(handler, OnIncomingNotification(expected_payloads,
                                                REMOTE_NOTIFICATION));
  }

  // Removed object IDs should not be notified, newly-added ones should.
  registrar.DispatchInvalidationsToHandlers(payloads, REMOTE_NOTIFICATION);

  Mock::VerifyAndClearExpectations(&handler);

  registrar.UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  registrar.DispatchInvalidationsToHandlers(payloads, REMOTE_NOTIFICATION);
}

// Register handlers and some IDs for those handlers, register a handler with
// no IDs, and register a handler with some IDs but unregister it.  Then,
// dispatch some notifications and invalidations.  Handlers that are registered
// should get notifications, and the ones that have registered IDs should
// receive invalidations for those IDs.
TEST_F(SyncNotifierRegistrarTest, MultipleHandlers) {
  StrictMock<MockSyncNotifierObserver> handler1;
  EXPECT_CALL(handler1, OnNotificationsEnabled());
  {
    ObjectIdPayloadMap expected_payloads;
    expected_payloads[kObjectId1] = "1";
    expected_payloads[kObjectId2] = "2";
    EXPECT_CALL(handler1, OnIncomingNotification(expected_payloads,
                                                 REMOTE_NOTIFICATION));
  }
  EXPECT_CALL(handler1,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  StrictMock<MockSyncNotifierObserver> handler2;
  EXPECT_CALL(handler2, OnNotificationsEnabled());
  {
    ObjectIdPayloadMap expected_payloads;
    expected_payloads[kObjectId3] = "3";
    EXPECT_CALL(handler2, OnIncomingNotification(expected_payloads,
                                                 REMOTE_NOTIFICATION));
  }
  EXPECT_CALL(handler2,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  StrictMock<MockSyncNotifierObserver> handler3;
  EXPECT_CALL(handler3, OnNotificationsEnabled());
  EXPECT_CALL(handler3,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  StrictMock<MockSyncNotifierObserver> handler4;

  SyncNotifierRegistrar registrar;

  registrar.RegisterHandler(&handler1);
  registrar.RegisterHandler(&handler2);
  registrar.RegisterHandler(&handler3);
  registrar.RegisterHandler(&handler4);

  {
    ObjectIdSet ids;
    ids.insert(kObjectId1);
    ids.insert(kObjectId2);
    registrar.UpdateRegisteredIds(&handler1, ids);
  }

  {
    ObjectIdSet ids;
    ids.insert(kObjectId3);
    registrar.UpdateRegisteredIds(&handler2, ids);
  }

  // Don't register any IDs for handler3.

  {
    ObjectIdSet ids;
    ids.insert(kObjectId4);
    registrar.UpdateRegisteredIds(&handler4, ids);
  }

  registrar.UnregisterHandler(&handler4);

  registrar.EmitOnNotificationsEnabled();
  {
    ObjectIdPayloadMap payloads;
    payloads[kObjectId1] = "1";
    payloads[kObjectId2] = "2";
    payloads[kObjectId3] = "3";
    payloads[kObjectId4] = "4";
    registrar.DispatchInvalidationsToHandlers(payloads, REMOTE_NOTIFICATION);
  }
  registrar.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

// Multiple registrations by different handlers on the same object ID should
// cause a CHECK.
TEST_F(SyncNotifierRegistrarTest, MultipleRegistration) {
  SyncNotifierRegistrar registrar;

  StrictMock<MockSyncNotifierObserver> handler1;
  registrar.RegisterHandler(&handler1);

  MockSyncNotifierObserver handler2;
  registrar.RegisterHandler(&handler2);

  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  registrar.UpdateRegisteredIds(&handler1, ids);

  registrar.DetachFromThreadForTest();
  EXPECT_DEATH({ registrar.UpdateRegisteredIds(&handler2, ids); },
               "Duplicate registration: .*");
}

// Make sure that passing an empty set to UpdateRegisteredIds clears the
// corresponding entries for the handler.
TEST_F(SyncNotifierRegistrarTest, EmptySetUnregisters) {
  StrictMock<MockSyncNotifierObserver> handler1;
  EXPECT_CALL(handler1, OnNotificationsEnabled());
  EXPECT_CALL(handler1,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  // Control observer.
  StrictMock<MockSyncNotifierObserver> handler2;
  EXPECT_CALL(handler2, OnNotificationsEnabled());
  {
    ObjectIdPayloadMap expected_payloads;
    expected_payloads[kObjectId3] = "3";
    EXPECT_CALL(handler2, OnIncomingNotification(expected_payloads,
                                                 REMOTE_NOTIFICATION));
  }
  EXPECT_CALL(handler2,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  SyncNotifierRegistrar registrar;

  registrar.RegisterHandler(&handler1);
  registrar.RegisterHandler(&handler2);

  {
    ObjectIdSet ids;
    ids.insert(kObjectId1);
    ids.insert(kObjectId2);
    registrar.UpdateRegisteredIds(&handler1, ids);
  }

  {
    ObjectIdSet ids;
    ids.insert(kObjectId3);
    registrar.UpdateRegisteredIds(&handler2, ids);
  }

  // Unregister the IDs for the first observer. It should not receive any
  // further invalidations.
  registrar.UpdateRegisteredIds(&handler1, ObjectIdSet());

  registrar.EmitOnNotificationsEnabled();
  {
    ObjectIdPayloadMap payloads;
    payloads[kObjectId1] = "1";
    payloads[kObjectId2] = "2";
    payloads[kObjectId3] = "3";
    registrar.DispatchInvalidationsToHandlers(payloads,
                                              REMOTE_NOTIFICATION);
  }
  registrar.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

}  // namespace

}  // namespace syncer
