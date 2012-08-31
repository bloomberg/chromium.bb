// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class defines tests that implementations of Invalidator should pass in
// order to be conformant.  Here's how you use it to test your implementation.
//
// Say your class is called MyInvalidator.  Then you need to define a class
// called MyInvalidatorTestDelegate in my_sync_notifier_unittest.cc like this:
//
//   class MyInvalidatorTestDelegate {
//    public:
//     MyInvalidatorTestDelegate() ...
//
//     ~MyInvalidatorTestDelegate() {
//       // DestroyInvalidator() may not be explicitly called by tests.
//       DestroyInvalidator();
//     }
//
//     // Create the Invalidator implementation with the given parameters.
//     void CreateInvalidator(
//         const std::string& initial_state,
//         const base::WeakPtr<InvalidationStateTracker>&
//             invalidation_state_tracker) {
//       ...
//     }
//
//     // Should return the Invalidator implementation.  Only called after
//     // CreateInvalidator and before DestroyInvalidator.
//     MyInvalidator* GetInvalidator() {
//       ...
//     }
//
//     // Destroy the Invalidator implementation.
//     void DestroyInvalidator() {
//       ...
//     }
//
//     // Called after a call to SetStateDeprecated(), SetUniqueId(), or
//     // UpdateCredentials() on the Invalidator implementation.  Should block
//     // until the effects of the call are visible on the current thread.
//     void WaitForInvalidator() {
//       ...
//     }
//
//     // The Trigger* functions below should block until the effects of
//     // the call are visible on the current thread.
//
//     // Should cause OnNotificationsEnabled() to be called on all
//     // observers of the Invalidator implementation.
//     void TriggerOnNotificationsEnabled() {
//       ...
//     }
//
//     // Should cause OnIncomingNotification() to be called on all
//     // observers of the Invalidator implementation with the given
//     // parameters.
//     void TriggerOnIncomingNotification(const ObjectIdStateMap& id_state_map,
//                                        IncomingNotificationSource source) {
//       ...
//     }
//
//     // Should cause OnNotificationsDisabled() to be called on all
//     // observers of the Invalidator implementation with the given
//     // parameters.
//     void TriggerOnNotificationsDisabled(
//         NotificationsDisabledReason reason) {
//       ...
//     }
//
//     // Returns whether or not the notifier handles storing the old
//     // (deprecated) notifier state.
//     static bool InvalidatorHandlesDeprecatedState() {
//       return false;
//     }
//   };
//
// The InvalidatorTest test harness will have a member variable of
// this delegate type and will call its functions in the various
// tests.
//
// Then you simply #include this file as well as gtest.h and add the
// following statement to my_sync_notifier_unittest.cc:
//
//   INSTANTIATE_TYPED_TEST_CASE_P(
//       MyInvalidator, InvalidatorTest, MyInvalidatorTestDelegate);
//
// Easy!

#ifndef SYNC_NOTIFIER_INVALIDATOR_TEST_TEMPLATE_H_
#define SYNC_NOTIFIER_INVALIDATOR_TEST_TEMPLATE_H_

#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidation_state_tracker.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/object_id_state_map.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

template <typename InvalidatorTestDelegate>
class InvalidatorTest : public testing::Test {
 protected:
  InvalidatorTest()
      : id1(ipc::invalidation::ObjectSource::TEST, "a"),
        id2(ipc::invalidation::ObjectSource::TEST, "b"),
        id3(ipc::invalidation::ObjectSource::TEST, "c"),
        id4(ipc::invalidation::ObjectSource::TEST, "d") {
  }

  Invalidator* CreateAndInitializeInvalidator() {
    this->delegate_.CreateInvalidator("fake_initial_state",
                                      this->fake_tracker_.AsWeakPtr());
    Invalidator* const invalidator = this->delegate_.GetInvalidator();

    // TODO(tim): This call should be a no-op. Remove once bug 124140 and
    // associated issues are fixed.
    invalidator->SetStateDeprecated("fake_state");
    this->delegate_.WaitForInvalidator();
    // We don't expect |fake_tracker_|'s state to change, as we
    // initialized with non-empty initial_invalidation_state above.
    EXPECT_TRUE(this->fake_tracker_.GetInvalidationState().empty());
    invalidator->SetUniqueId("fake_id");
    this->delegate_.WaitForInvalidator();
    invalidator->UpdateCredentials("foo@bar.com", "fake_token");
    this->delegate_.WaitForInvalidator();

    return invalidator;
  }

  FakeInvalidationStateTracker fake_tracker_;
  InvalidatorTestDelegate delegate_;

  const invalidation::ObjectId id1;
  const invalidation::ObjectId id2;
  const invalidation::ObjectId id3;
  const invalidation::ObjectId id4;
};

TYPED_TEST_CASE_P(InvalidatorTest);

// Initialize the invalidator, register a handler, register some IDs for that
// handler, and then unregister the handler, dispatching invalidations in
// between.  The handler should only see invalidations when its registered and
// its IDs are registered.
TYPED_TEST_P(InvalidatorTest, Basic) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler;

  invalidator->RegisterHandler(&handler);

  ObjectIdStateMap states;
  states[this->id1].payload = "1";
  states[this->id2].payload = "2";
  states[this->id3].payload = "3";

  // Should be ignored since no IDs are registered to |handler|.
  this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(0, handler.GetNotificationCount());

  ObjectIdSet ids;
  ids.insert(this->id1);
  ids.insert(this->id2);
  invalidator->UpdateRegisteredIds(&handler, ids);

  this->delegate_.TriggerOnNotificationsEnabled();
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler.GetNotificationsDisabledReason());

  ObjectIdStateMap expected_states;
  expected_states[this->id1].payload = "1";
  expected_states[this->id2].payload = "2";

  this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(1, handler.GetNotificationCount());
  EXPECT_THAT(
      expected_states,
      Eq(handler.GetLastNotificationIdStateMap()));
  EXPECT_EQ(REMOTE_NOTIFICATION, handler.GetLastNotificationSource());

  ids.erase(this->id1);
  ids.insert(this->id3);
  invalidator->UpdateRegisteredIds(&handler, ids);

  expected_states.erase(this->id1);
  expected_states[this->id3].payload = "3";

  // Removed object IDs should not be notified, newly-added ones should.
  this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(2, handler.GetNotificationCount());
  EXPECT_THAT(
      expected_states,
      Eq(handler.GetLastNotificationIdStateMap()));
  EXPECT_EQ(REMOTE_NOTIFICATION, handler.GetLastNotificationSource());

  this->delegate_.TriggerOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler.GetNotificationsDisabledReason());

  this->delegate_.TriggerOnNotificationsDisabled(
      NOTIFICATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(NOTIFICATION_CREDENTIALS_REJECTED,
            handler.GetNotificationsDisabledReason());

  invalidator->UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);
  EXPECT_EQ(2, handler.GetNotificationCount());
}

// Register handlers and some IDs for those handlers, register a handler with
// no IDs, and register a handler with some IDs but unregister it.  Then,
// dispatch some notifications and invalidations.  Handlers that are registered
// should get notifications, and the ones that have registered IDs should
// receive invalidations for those IDs.
TYPED_TEST_P(InvalidatorTest, MultipleHandlers) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;
  FakeInvalidationHandler handler3;
  FakeInvalidationHandler handler4;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);
  invalidator->RegisterHandler(&handler3);
  invalidator->RegisterHandler(&handler4);

  {
    ObjectIdSet ids;
    ids.insert(this->id1);
    ids.insert(this->id2);
    invalidator->UpdateRegisteredIds(&handler1, ids);
  }

  {
    ObjectIdSet ids;
    ids.insert(this->id3);
    invalidator->UpdateRegisteredIds(&handler2, ids);
  }

  // Don't register any IDs for handler3.

  {
    ObjectIdSet ids;
    ids.insert(this->id4);
    invalidator->UpdateRegisteredIds(&handler4, ids);
  }

  invalidator->UnregisterHandler(&handler4);

  this->delegate_.TriggerOnNotificationsEnabled();
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
    states[this->id1].payload = "1";
    states[this->id2].payload = "2";
    states[this->id3].payload = "3";
    states[this->id4].payload = "4";
    this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);

    ObjectIdStateMap expected_states;
    expected_states[this->id1].payload = "1";
    expected_states[this->id2].payload = "2";

    EXPECT_EQ(1, handler1.GetNotificationCount());
    EXPECT_THAT(
        expected_states,
        Eq(handler1.GetLastNotificationIdStateMap()));
    EXPECT_EQ(REMOTE_NOTIFICATION, handler1.GetLastNotificationSource());

    expected_states.clear();
    expected_states[this->id3].payload = "3";

    EXPECT_EQ(1, handler2.GetNotificationCount());
    EXPECT_THAT(
        expected_states,
        Eq(handler2.GetLastNotificationIdStateMap()));
    EXPECT_EQ(REMOTE_NOTIFICATION, handler2.GetLastNotificationSource());

    EXPECT_EQ(0, handler3.GetNotificationCount());
    EXPECT_EQ(0, handler4.GetNotificationCount());
  }

  this->delegate_.TriggerOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler3.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler4.GetNotificationsDisabledReason());
}

// Make sure that passing an empty set to UpdateRegisteredIds clears the
// corresponding entries for the handler.
TYPED_TEST_P(InvalidatorTest, EmptySetUnregisters) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler1;

  // Control observer.
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  {
    ObjectIdSet ids;
    ids.insert(this->id1);
    ids.insert(this->id2);
    invalidator->UpdateRegisteredIds(&handler1, ids);
  }

  {
    ObjectIdSet ids;
    ids.insert(this->id3);
    invalidator->UpdateRegisteredIds(&handler2, ids);
  }

  // Unregister the IDs for the first observer. It should not receive any
  // further invalidations.
  invalidator->UpdateRegisteredIds(&handler1, ObjectIdSet());

  this->delegate_.TriggerOnNotificationsEnabled();
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());

  {
    ObjectIdStateMap states;
    states[this->id1].payload = "1";
    states[this->id2].payload = "2";
    states[this->id3].payload = "3";
    this->delegate_.TriggerOnIncomingNotification(states, REMOTE_NOTIFICATION);
    EXPECT_EQ(0, handler1.GetNotificationCount());
    EXPECT_EQ(1, handler2.GetNotificationCount());
  }

  this->delegate_.TriggerOnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler1.GetNotificationsDisabledReason());
  EXPECT_EQ(TRANSIENT_NOTIFICATION_ERROR,
            handler2.GetNotificationsDisabledReason());
}

// Initialize the invalidator with an empty initial state.  Call the deprecated
// state setter function a number of times, destroying and re-creating the
// invalidator in between.  Only the first one should take effect (i.e., state
// migration should only happen once).
TYPED_TEST_P(InvalidatorTest, MigrateState) {
  if (!this->delegate_.InvalidatorHandlesDeprecatedState()) {
    DLOG(INFO) << "This Invalidator doesn't handle deprecated state; "
               << "skipping";
    return;
  }

  this->delegate_.CreateInvalidator(std::string(),
                                    this->fake_tracker_.AsWeakPtr());
  Invalidator* invalidator = this->delegate_.GetInvalidator();

  invalidator->SetStateDeprecated("fake_state");
  this->delegate_.WaitForInvalidator();
  EXPECT_EQ("fake_state", this->fake_tracker_.GetInvalidationState());

  // Should do nothing.
  invalidator->SetStateDeprecated("spurious_fake_state");
  this->delegate_.WaitForInvalidator();
  EXPECT_EQ("fake_state", this->fake_tracker_.GetInvalidationState());

  // Pretend that Chrome has shut down.
  this->delegate_.DestroyInvalidator();
  this->delegate_.CreateInvalidator("fake_state",
                                 this->fake_tracker_.AsWeakPtr());
  invalidator = this->delegate_.GetInvalidator();

  // Should do nothing.
  invalidator->SetStateDeprecated("more_spurious_fake_state");
  this->delegate_.WaitForInvalidator();
  EXPECT_EQ("fake_state", this->fake_tracker_.GetInvalidationState());
}

REGISTER_TYPED_TEST_CASE_P(InvalidatorTest,
                           Basic, MultipleHandlers, EmptySetUnregisters,
                           MigrateState);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATOR_TEST_TEMPLATE_H_
