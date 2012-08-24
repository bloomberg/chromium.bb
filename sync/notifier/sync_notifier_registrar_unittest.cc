// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google/cacheinvalidation/types.pb.h"
#include "sync/notifier/fake_sync_notifier_observer.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "sync/notifier/sync_notifier_registrar.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

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
  FakeSyncNotifierObserver handler;

  SyncNotifierRegistrar registrar;

  registrar.RegisterHandler(&handler);

  ObjectIdStateMap states;
  states[kObjectId1].payload = "1";
  states[kObjectId2].payload = "2";
  states[kObjectId3].payload = "3";

  // Should be ignored since no IDs are registered to |handler|.
  registrar.DispatchInvalidationsToHandlers(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(0, handler.GetNotificationCount());

  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  registrar.UpdateRegisteredIds(&handler, ids);

  ObjectIdStateMap expected_states;
  expected_states[kObjectId1].payload = "1";
  expected_states[kObjectId2].payload = "2";

  registrar.DispatchInvalidationsToHandlers(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(1, handler.GetNotificationCount());
  EXPECT_THAT(
      expected_states,
      Eq(handler.GetLastNotificationIdStateMap()));
  EXPECT_EQ(REMOTE_NOTIFICATION, handler.GetLastNotificationSource());

  ids.erase(kObjectId1);
  ids.insert(kObjectId3);
  registrar.UpdateRegisteredIds(&handler, ids);

  expected_states.erase(kObjectId1);
  expected_states[kObjectId3].payload = "3";

  // Removed object IDs should not be notified, newly-added ones should.
  registrar.DispatchInvalidationsToHandlers(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(2, handler.GetNotificationCount());
  EXPECT_THAT(
      expected_states,
      Eq(handler.GetLastNotificationIdStateMap()));
  EXPECT_EQ(REMOTE_NOTIFICATION, handler.GetLastNotificationSource());

  registrar.UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  registrar.DispatchInvalidationsToHandlers(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(2, handler.GetNotificationCount());
}

// Register handlers and some IDs for those handlers, register a handler with
// no IDs, and register a handler with some IDs but unregister it.  Then,
// dispatch some notifications and invalidations.  Handlers that are registered
// should get notifications, and the ones that have registered IDs should
// receive invalidations for those IDs.
TEST_F(SyncNotifierRegistrarTest, MultipleHandlers) {
  FakeSyncNotifierObserver handler1;
  FakeSyncNotifierObserver handler2;
  FakeSyncNotifierObserver handler3;
  FakeSyncNotifierObserver handler4;

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
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler3.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler4.GetNotificationsDisabledReason());

  {
    ObjectIdStateMap states;
    states[kObjectId1].payload = "1";
    states[kObjectId2].payload = "2";
    states[kObjectId3].payload = "3";
    states[kObjectId4].payload = "4";
    registrar.DispatchInvalidationsToHandlers(states, REMOTE_NOTIFICATION);

    ObjectIdStateMap expected_states;
    expected_states[kObjectId1].payload = "1";
    expected_states[kObjectId2].payload = "2";

    EXPECT_EQ(1, handler1.GetNotificationCount());
    EXPECT_THAT(
        expected_states,
        Eq(handler1.GetLastNotificationIdStateMap()));
    EXPECT_EQ(REMOTE_NOTIFICATION, handler1.GetLastNotificationSource());

    expected_states.clear();
    expected_states[kObjectId3].payload = "3";

    EXPECT_EQ(1, handler2.GetNotificationCount());
    EXPECT_THAT(
        expected_states,
        Eq(handler2.GetLastNotificationIdStateMap()));
    EXPECT_EQ(REMOTE_NOTIFICATION, handler2.GetLastNotificationSource());

    EXPECT_EQ(0, handler3.GetNotificationCount());
    EXPECT_EQ(0, handler4.GetNotificationCount());
  }

  registrar.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler3.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler4.GetNotificationsDisabledReason());
}

// Multiple registrations by different handlers on the same object ID should
// cause a CHECK.
TEST_F(SyncNotifierRegistrarTest, MultipleRegistration) {
  SyncNotifierRegistrar registrar;

  FakeSyncNotifierObserver handler1;
  registrar.RegisterHandler(&handler1);

  FakeSyncNotifierObserver handler2;
  registrar.RegisterHandler(&handler2);

  ObjectIdSet ids;
  ids.insert(kObjectId1);
  ids.insert(kObjectId2);
  registrar.UpdateRegisteredIds(&handler1, ids);

  registrar.DetachFromThreadForTest();
  // We expect a death via CHECK(). We can't match against the CHECK() message
  // though since they are removed in official builds.
  EXPECT_DEATH({ registrar.UpdateRegisteredIds(&handler2, ids); }, "");
}

// Make sure that passing an empty set to UpdateRegisteredIds clears the
// corresponding entries for the handler.
TEST_F(SyncNotifierRegistrarTest, EmptySetUnregisters) {
  FakeSyncNotifierObserver handler1;

  // Control observer.
  FakeSyncNotifierObserver handler2;

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
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());

  {
    ObjectIdStateMap states;
    states[kObjectId1].payload = "1";
    states[kObjectId2].payload = "2";
    states[kObjectId3].payload = "3";
    registrar.DispatchInvalidationsToHandlers(states,
                                              REMOTE_NOTIFICATION);
    EXPECT_EQ(0, handler1.GetNotificationCount());
    EXPECT_EQ(1, handler2.GetNotificationCount());
  }

  registrar.EmitOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());
}

}  // namespace

}  // namespace syncer
