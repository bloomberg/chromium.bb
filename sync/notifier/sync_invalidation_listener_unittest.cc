// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/time/tick_clock.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "sync/internal_api/public/base/invalidation_test_util.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/ack_tracker.h"
#include "sync/notifier/fake_invalidation_state_tracker.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/sync_invalidation_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using invalidation::AckHandle;
using invalidation::ObjectId;

const char kClientId[] = "client_id";
const char kClientInfo[] = "client_info";

const char kState[] = "state";
const char kNewState[] = "new_state";

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64 kMinVersion = FakeInvalidationStateTracker::kMinVersion;
const int64 kVersion1 = 1LL;
const int64 kVersion2 = 2LL;

const int kChromeSyncSourceId = 1004;

struct AckHandleLessThan {
  bool operator()(const AckHandle& lhs, const AckHandle& rhs) const {
    return lhs.handle_data() < rhs.handle_data();
  }
};

typedef std::set<AckHandle, AckHandleLessThan> AckHandleSet;

// Fake invalidation::InvalidationClient implementation that keeps
// track of registered IDs and acked handles.
class FakeInvalidationClient : public invalidation::InvalidationClient {
 public:
  FakeInvalidationClient() : started_(false) {}
  virtual ~FakeInvalidationClient() {}

  const ObjectIdSet& GetRegisteredIds() const {
    return registered_ids_;
  }

  void ClearAckedHandles() {
    acked_handles_.clear();
  }

  bool IsAckedHandle(const AckHandle& ack_handle) const {
    return (acked_handles_.find(ack_handle) != acked_handles_.end());
  }

  // invalidation::InvalidationClient implementation.

  virtual void Start() OVERRIDE {
    started_ = true;
  }

  virtual void Stop() OVERRIDE {
    started_ = false;
  }

  virtual void Register(const ObjectId& object_id) OVERRIDE {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.insert(object_id);
  }

  virtual void Register(
      const invalidation::vector<ObjectId>& object_ids) OVERRIDE {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.insert(object_ids.begin(), object_ids.end());
  }

  virtual void Unregister(const ObjectId& object_id) OVERRIDE {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.erase(object_id);
  }

  virtual void Unregister(
      const invalidation::vector<ObjectId>& object_ids) OVERRIDE {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    for (invalidation::vector<ObjectId>::const_iterator
             it = object_ids.begin(); it != object_ids.end(); ++it) {
      registered_ids_.erase(*it);
    }
  }

  virtual void Acknowledge(const AckHandle& ack_handle) OVERRIDE {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    acked_handles_.insert(ack_handle);
  }

 private:
  bool started_;
  ObjectIdSet registered_ids_;
  AckHandleSet acked_handles_;
};

// Fake delegate tkat keeps track of invalidation counts, payloads,
// and state.
class FakeDelegate : public SyncInvalidationListener::Delegate {
 public:
  explicit FakeDelegate(SyncInvalidationListener* listener)
      : listener_(listener),
        state_(TRANSIENT_INVALIDATION_ERROR) {}
  virtual ~FakeDelegate() {}

  int GetInvalidationCount(const ObjectId& id) const {
    ObjectIdCountMap::const_iterator it = invalidation_counts_.find(id);
    return (it == invalidation_counts_.end()) ? 0 : it->second;
  }

  std::string GetPayload(const ObjectId& id) const {
    ObjectIdInvalidationMap::const_iterator it = invalidations_.find(id);
    return (it == invalidations_.end()) ? "" : it->second.payload;
  }

  InvalidatorState GetInvalidatorState() const {
    return state_;
  }

  void Acknowledge(const ObjectId& id) {
    listener_->Acknowledge(id, invalidations_[id].ack_handle);
  }

  // SyncInvalidationListener::Delegate implementation.

  virtual void OnInvalidate(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE {
    for (ObjectIdInvalidationMap::const_iterator it = invalidation_map.begin();
         it != invalidation_map.end(); ++it) {
      ++invalidation_counts_[it->first];
      invalidations_[it->first] = it->second;
    }
  }

  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE {
    state_ = state;
  }

 private:
  typedef std::map<ObjectId, int, ObjectIdLessThan> ObjectIdCountMap;
  ObjectIdCountMap invalidation_counts_;
  ObjectIdInvalidationMap invalidations_;
  SyncInvalidationListener* listener_;
  InvalidatorState state_;
};

invalidation::InvalidationClient* CreateFakeInvalidationClient(
    FakeInvalidationClient** fake_invalidation_client,
    invalidation::SystemResources* resources,
    int client_type,
    const invalidation::string& client_name,
    const invalidation::string& application_name,
    invalidation::InvalidationListener* listener) {
  *fake_invalidation_client = new FakeInvalidationClient();
  return *fake_invalidation_client;
}

// TODO(dcheng): FakeTickClock and FakeBackoffEntry ought to be factored out
// into a helpers file so it can be shared with the AckTracker unittest.
class FakeTickClock : public base::TickClock {
 public:
  FakeTickClock() {}
  virtual ~FakeTickClock() {}

  void LeapForward(int seconds) {
    ASSERT_GT(seconds, 0);
    fake_now_ticks_ += base::TimeDelta::FromSeconds(seconds);
  }

  virtual base::TimeTicks NowTicks() OVERRIDE {
    return fake_now_ticks_;
  }

 private:
  base::TimeTicks fake_now_ticks_;

  DISALLOW_COPY_AND_ASSIGN(FakeTickClock);
};

class FakeBackoffEntry : public net::BackoffEntry {
 public:
  FakeBackoffEntry(const Policy *const policy, base::TickClock* tick_clock)
      : BackoffEntry(policy), tick_clock_(tick_clock) {
  }

 protected:
  virtual base::TimeTicks ImplGetTimeNow() const OVERRIDE {
    return tick_clock_->NowTicks();
  }

 private:
  base::TickClock* const tick_clock_;
};

scoped_ptr<net::BackoffEntry> CreateMockEntry(
    base::TickClock* tick_clock,
    const net::BackoffEntry::Policy *const policy) {
  return scoped_ptr<net::BackoffEntry>(
      new FakeBackoffEntry(policy, tick_clock));
}

class SyncInvalidationListenerTest : public testing::Test {
 protected:
  SyncInvalidationListenerTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kExtensionsId_(kChromeSyncSourceId, "EXTENSION"),
        kAppsId_(kChromeSyncSourceId, "APP"),
        fake_push_client_(new notifier::FakePushClient()),
        fake_invalidation_client_(NULL),
        listener_(&tick_clock_,
                  scoped_ptr<notifier::PushClient>(fake_push_client_)),
        fake_delegate_(&listener_) {}

  virtual void SetUp() {
    StartClient();

    registered_ids_.insert(kBookmarksId_);
    registered_ids_.insert(kPreferencesId_);
    listener_.UpdateRegisteredIds(registered_ids_);
  }

  virtual void TearDown() {
    StopClient();
  }

  // Restart client without re-registering IDs.
  void RestartClient() {
    StopClient();
    StartClient();
  }

  void StartClient() {
    fake_invalidation_client_ = NULL;
    listener_.Start(base::Bind(&CreateFakeInvalidationClient,
                               &fake_invalidation_client_),
                    kClientId, kClientInfo, kState,
                    fake_tracker_.GetAllInvalidationStates(),
                    MakeWeakHandle(fake_tracker_.AsWeakPtr()),
                    &fake_delegate_);
    DCHECK(fake_invalidation_client_);
  }

  void StopClient() {
    // listener_.StopForTest() stops the invalidation scheduler, which
    // deletes any pending tasks without running them.  Some tasks
    // "run and delete" another task, so they must be run in order to
    // avoid leaking the inner task.  listener_.StopForTest() does not
    // schedule any tasks, so it's both necessary and sufficient to
    // drain the task queue before calling it.
    message_loop_.RunUntilIdle();
    fake_invalidation_client_ = NULL;
    listener_.StopForTest();
  }

  int GetInvalidationCount(const ObjectId& id) const {
    return fake_delegate_.GetInvalidationCount(id);
  }

  std::string GetPayload(const ObjectId& id) const {
    return fake_delegate_.GetPayload(id);
  }

  InvalidatorState GetInvalidatorState() const {
    return fake_delegate_.GetInvalidatorState();
  }

  int64 GetMaxVersion(const ObjectId& id) const {
    return fake_tracker_.GetMaxVersion(id);
  }

  std::string GetInvalidatorClientId() const {
    return fake_tracker_.GetInvalidatorClientId();
  }

  std::string GetBootstrapData() const {
    return fake_tracker_.GetBootstrapData();
  }

  ObjectIdSet GetRegisteredIds() const {
    return fake_invalidation_client_->GetRegisteredIds();
  }

  // |payload| can be NULL.
  void FireInvalidate(const ObjectId& object_id,
                      int64 version, const char* payload) {
    invalidation::Invalidation inv;
    if (payload) {
      inv = invalidation::Invalidation(object_id, version, payload);
    } else {
      inv = invalidation::Invalidation(object_id, version);
    }
    const AckHandle ack_handle("fakedata");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.Invalidate(fake_invalidation_client_, inv, ack_handle);
    // Pump message loop to trigger InvalidationStateTracker::SetMaxVersion()
    // and callback from InvalidationStateTracker::GenerateAckHandles().
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const ObjectId& object_id) {
    const AckHandle ack_handle("fakedata_unknown");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.InvalidateUnknownVersion(fake_invalidation_client_, object_id,
                                     ack_handle);
    // Pump message loop to trigger callback from
    // InvalidationStateTracker::GenerateAckHandles().
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void FireInvalidateAll() {
    const AckHandle ack_handle("fakedata_all");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.InvalidateAll(fake_invalidation_client_, ack_handle);
    // Pump message loop to trigger callback from
    // InvalidationStateTracker::GenerateAckHandles().
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void WriteState(const std::string& new_state) {
    listener_.WriteState(new_state);
    // Pump message loop to trigger
    // InvalidationStateTracker::WriteState().
    message_loop_.RunUntilIdle();
  }

  void EnableNotifications() {
    fake_push_client_->EnableNotifications();
  }

  void DisableNotifications(notifier::NotificationsDisabledReason reason) {
    fake_push_client_->DisableNotifications(reason);
  }

  void VerifyUnacknowledged(const ObjectId& object_id) {
    InvalidationStateMap state_map = fake_tracker_.GetAllInvalidationStates();
    EXPECT_THAT(state_map[object_id].current,
                Not(Eq(state_map[object_id].expected)));
    EXPECT_EQ(listener_.GetStateMapForTest(), state_map);
  }

  void VerifyAcknowledged(const ObjectId& object_id) {
    InvalidationStateMap state_map = fake_tracker_.GetAllInvalidationStates();
    EXPECT_THAT(state_map[object_id].current,
                Eq(state_map[object_id].expected));
    EXPECT_EQ(listener_.GetStateMapForTest(), state_map);
  }

  void AcknowledgeAndVerify(const ObjectId& object_id) {
    VerifyUnacknowledged(object_id);
    fake_delegate_.Acknowledge(object_id);
    // Pump message loop to trigger
    // InvalidationStateTracker::Acknowledge().
    message_loop_.RunUntilIdle();
    VerifyAcknowledged(object_id);
  }

  const ObjectId kBookmarksId_;
  const ObjectId kPreferencesId_;
  const ObjectId kExtensionsId_;
  const ObjectId kAppsId_;

  ObjectIdSet registered_ids_;

 private:
  MessageLoop message_loop_;
  FakeInvalidationStateTracker fake_tracker_;
  notifier::FakePushClient* const fake_push_client_;

 protected:
  // Tests need to access these directly.
  FakeInvalidationClient* fake_invalidation_client_;
  FakeTickClock tick_clock_;
  SyncInvalidationListener listener_;

 private:
  FakeDelegate fake_delegate_;
};

// Write a new state to the client.  It should propagate to the
// tracker.
TEST_F(SyncInvalidationListenerTest, WriteState) {
  WriteState(kNewState);

  EXPECT_EQ(kNewState, GetBootstrapData());
}

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateNoPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, NULL);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
  AcknowledgeAndVerify(id);
}

// Fire an invalidation with an empty payload.  It should be
// processed, the payload should remain empty, and the version should
// be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateEmptyPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, "");

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
  AcknowledgeAndVerify(id);
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateWithPayload) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion1, kPayload1);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
  AcknowledgeAndVerify(id);
}

// Fire an invalidation for an unregistered object ID with a payload.  It should
// still be processed, and both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateUnregisteredWithPayload) {
  const ObjectId kUnregisteredId(
      kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;

  EXPECT_EQ(0, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kMinVersion, GetMaxVersion(id));

  FireInvalidate(id, kVersion1, "unregistered payload");

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ("unregistered payload", GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
  AcknowledgeAndVerify(id);
}

// Fire an invalidation, then fire another one with a lower version.
// The first one should be processed and should update the payload and
// version, but the second one shouldn't.
TEST_F(SyncInvalidationListenerTest, InvalidateVersion) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion2, kPayload2);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ(kPayload2, GetPayload(id));
  EXPECT_EQ(kVersion2, GetMaxVersion(id));
  AcknowledgeAndVerify(id);

  FireInvalidate(id, kVersion1, kPayload1);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ(kPayload2, GetPayload(id));
  EXPECT_EQ(kVersion2, GetMaxVersion(id));
  VerifyAcknowledged(id);
}

// Fire an invalidation with an unknown version twice.  It shouldn't
// update the payload or version either time, but it should still be
// processed.
TEST_F(SyncInvalidationListenerTest, InvalidateUnknownVersion) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidateUnknownVersion(id);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kMinVersion, GetMaxVersion(id));
  AcknowledgeAndVerify(id);

  FireInvalidateUnknownVersion(id);

  EXPECT_EQ(2, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kMinVersion, GetMaxVersion(id));
  AcknowledgeAndVerify(id);
}

// Fire an invalidation for all enabled IDs.  It shouldn't update the
// payload or version, but it should still invalidate the IDs.
TEST_F(SyncInvalidationListenerTest, InvalidateAll) {
  FireInvalidateAll();

  for (ObjectIdSet::const_iterator it = registered_ids_.begin();
       it != registered_ids_.end(); ++it) {
    EXPECT_EQ(1, GetInvalidationCount(*it));
    EXPECT_EQ("", GetPayload(*it));
    EXPECT_EQ(kMinVersion, GetMaxVersion(*it));
    AcknowledgeAndVerify(*it);
  }
}

// Comprehensive test of various scenarios for multiple IDs.
TEST_F(SyncInvalidationListenerTest, InvalidateMultipleIds) {
  FireInvalidate(kBookmarksId_, 3, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));
  AcknowledgeAndVerify(kBookmarksId_);

  FireInvalidate(kExtensionsId_, 2, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(2, GetMaxVersion(kExtensionsId_));
  AcknowledgeAndVerify(kExtensionsId_);

  // Invalidations with lower version numbers should be ignored.

  FireInvalidate(kBookmarksId_, 1, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  FireInvalidate(kExtensionsId_, 1, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(2, GetMaxVersion(kExtensionsId_));

  // InvalidateAll shouldn't change any version state.

  FireInvalidateAll();

  EXPECT_EQ(2, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));
  AcknowledgeAndVerify(kBookmarksId_);

  EXPECT_EQ(1, GetInvalidationCount(kPreferencesId_));
  EXPECT_EQ("", GetPayload(kPreferencesId_));
  EXPECT_EQ(kMinVersion, GetMaxVersion(kPreferencesId_));
  AcknowledgeAndVerify(kPreferencesId_);

  // Note that kExtensionsId_ is not registered, so InvalidateAll() shouldn't
  // affect it.
  EXPECT_EQ(1, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(2, GetMaxVersion(kExtensionsId_));
  VerifyAcknowledged(kExtensionsId_);

  // Invalidations with higher version numbers should be processed.

  FireInvalidate(kPreferencesId_, 5, NULL);
  EXPECT_EQ(2, GetInvalidationCount(kPreferencesId_));
  EXPECT_EQ("", GetPayload(kPreferencesId_));
  EXPECT_EQ(5, GetMaxVersion(kPreferencesId_));
  AcknowledgeAndVerify(kPreferencesId_);

  FireInvalidate(kExtensionsId_, 3, NULL);
  EXPECT_EQ(2, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(3, GetMaxVersion(kExtensionsId_));
  AcknowledgeAndVerify(kExtensionsId_);

  FireInvalidate(kBookmarksId_, 4, NULL);
  EXPECT_EQ(3, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(4, GetMaxVersion(kBookmarksId_));
  AcknowledgeAndVerify(kBookmarksId_);
}

// Various tests for the local invalidation feature.
// Tests a "normal" scenario. We allow one timeout period to expire by sending
// ack handles that are not the "latest" ack handle. Once the timeout expires,
// we verify that we get a second callback and then acknowledge it. Once
// acknowledged, no further timeouts should occur.
TEST_F(SyncInvalidationListenerTest, InvalidateOneTimeout) {
  listener_.GetAckTrackerForTest()->SetCreateBackoffEntryCallbackForTest(
      base::Bind(&CreateMockEntry, &tick_clock_));

  // Trigger the initial invalidation.
  FireInvalidate(kBookmarksId_, 3, NULL);
  EXPECT_EQ(1, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));
  VerifyUnacknowledged(kBookmarksId_);

  // Trigger one timeout.
  tick_clock_.LeapForward(60);
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->TriggerTimeoutAtForTest(
      tick_clock_.NowTicks()));
  EXPECT_EQ(2, GetInvalidationCount(kBookmarksId_));
  // Other properties should remain the same.
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  AcknowledgeAndVerify(kBookmarksId_);

  // No more invalidations should remain in the queue.
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->IsQueueEmptyForTest());
}

// Test that an unacknowledged invalidation triggers reminders if the listener
// is restarted.
TEST_F(SyncInvalidationListenerTest, InvalidationTimeoutRestart) {
  listener_.GetAckTrackerForTest()->SetCreateBackoffEntryCallbackForTest(
      base::Bind(&CreateMockEntry, &tick_clock_));

  FireInvalidate(kBookmarksId_, 3, NULL);
  EXPECT_EQ(1, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  // Trigger one timeout.
  tick_clock_.LeapForward(60);
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->TriggerTimeoutAtForTest(
      tick_clock_.NowTicks()));
  EXPECT_EQ(2, GetInvalidationCount(kBookmarksId_));
  // Other properties should remain the same.
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  // Restarting the client should reset the retry count and the timeout period
  // (e.g. it shouldn't increase to 120 seconds). Skip ahead 1200 seconds to be
  // on the safe side.
  StopClient();
  tick_clock_.LeapForward(1200);
  StartClient();

  // The bookmark invalidation state should not have changed.
  EXPECT_EQ(2, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  // Now trigger the invalidation reminder after the client restarts.
  tick_clock_.LeapForward(60);
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->TriggerTimeoutAtForTest(
      tick_clock_.NowTicks()));
  EXPECT_EQ(3, GetInvalidationCount(kBookmarksId_));
  // Other properties should remain the same.
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  AcknowledgeAndVerify(kBookmarksId_);

  // No more invalidations should remain in the queue.
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->IsQueueEmptyForTest());

  // The queue should remain empty when we restart now.
  RestartClient();
  EXPECT_TRUE(listener_.GetAckTrackerForTest()->IsQueueEmptyForTest());
}

// Registration tests.

// With IDs already registered, enable notifications then ready the
// client.  The IDs should be registered only after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterEnableReady) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client then enable
// notifications.  The IDs should be registered after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterReadyEnable) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, re-register the IDs, then
// ready the client.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableRegisterReady) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, ready the client, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableReadyRegister) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, enable notifications, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, ReadyEnableRegister) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, re-register the IDs, then
// enable notifications. The IDs should be registered only after the
// client is readied.
//
// This test is important: see http://crbug.com/139424.
TEST_F(SyncInvalidationListenerTest, ReadyRegisterEnable) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client, restart the client,
// then re-ready it.  The IDs should still be registered.
TEST_F(SyncInvalidationListenerTest, RegisterTypesPreserved) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  RestartClient();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Make sure that state is correctly purged from the local invalidation state
// map cache when an ID is unregistered.
TEST_F(SyncInvalidationListenerTest, UnregisterCleansUpStateMapCache) {
  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(listener_.GetStateMapForTest().empty());
  FireInvalidate(kBookmarksId_, 1, "hello");
  EXPECT_EQ(1U, listener_.GetStateMapForTest().size());
  EXPECT_TRUE(ContainsKey(listener_.GetStateMapForTest(), kBookmarksId_));
  FireInvalidate(kPreferencesId_, 2, "world");
  EXPECT_EQ(2U, listener_.GetStateMapForTest().size());
  EXPECT_TRUE(ContainsKey(listener_.GetStateMapForTest(), kBookmarksId_));
  EXPECT_TRUE(ContainsKey(listener_.GetStateMapForTest(), kPreferencesId_));

  ObjectIdSet ids;
  ids.insert(kBookmarksId_);
  listener_.UpdateRegisteredIds(ids);
  EXPECT_EQ(1U, listener_.GetStateMapForTest().size());
  EXPECT_TRUE(ContainsKey(listener_.GetStateMapForTest(), kBookmarksId_));
}

// Without readying the client, disable notifications, then enable
// them.  The listener should still think notifications are disabled.
TEST_F(SyncInvalidationListenerTest, EnableNotificationsNotReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR,
            GetInvalidatorState());

  DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  DisableNotifications(notifier::NOTIFICATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());
}

// Enable notifications then Ready the invalidation client.  The
// delegate should then be ready.
TEST_F(SyncInvalidationListenerTest, EnableNotificationsThenReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Ready the invalidation client then enable notifications.  The
// delegate should then be ready.
TEST_F(SyncInvalidationListenerTest, ReadyThenEnableNotifications) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then disable
// notifications with an auth error and re-enable notifications.  The
// delegate should go into an auth error mode and then back out.
TEST_F(SyncInvalidationListenerTest, PushClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then simulate an auth
// error from the invalidation client.  Simulate some notification
// events, then re-ready the client.  The delegate should go into an
// auth error mode and come out of it only after the client is ready.
TEST_F(SyncInvalidationListenerTest, InvalidationClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  listener_.InformError(
      fake_invalidation_client_,
      invalidation::ErrorInfo(
          invalidation::ErrorReason::AUTH_FAILURE,
          false /* is_transient */,
          "auth error",
          invalidation::ErrorContext()));

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  DisableNotifications(notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  DisableNotifications(notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

}  // namespace

}  // namespace syncer
