// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "sync/internal_api/public/util/weak_handle.h"
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
  FakeDelegate() : state_(TRANSIENT_INVALIDATION_ERROR) {}
  virtual ~FakeDelegate() {}

  int GetInvalidationCount(const ObjectId& id) const {
    ObjectIdCountMap::const_iterator it = invalidation_counts_.find(id);
    return (it == invalidation_counts_.end()) ? 0 : it->second;
  }

  std::string GetPayload(const ObjectId& id) const {
    ObjectIdStateMap::const_iterator it = states_.find(id);
    return (it == states_.end()) ? "" : it->second.payload;
  }

  InvalidatorState GetInvalidatorState() const {
    return state_;
  }

  // SyncInvalidationListener::Delegate implementation.

  virtual void OnInvalidate(const ObjectIdStateMap& id_state_map) OVERRIDE {
    for (ObjectIdStateMap::const_iterator it = id_state_map.begin();
         it != id_state_map.end(); ++it) {
      ++invalidation_counts_[it->first];
      states_[it->first] = it->second;
    }
  }

  virtual void OnInvalidatorStateChange(InvalidatorState state) {
    state_ = state;
  }

 private:
  typedef std::map<ObjectId, int, ObjectIdLessThan> ObjectIdCountMap;
  ObjectIdCountMap invalidation_counts_;
  ObjectIdStateMap states_;
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

class SyncInvalidationListenerTest : public testing::Test {
 protected:
  SyncInvalidationListenerTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kExtensionsId_(kChromeSyncSourceId, "EXTENSION"),
        kAppsId_(kChromeSyncSourceId, "APP"),
        fake_push_client_(new notifier::FakePushClient()),
        fake_invalidation_client_(NULL),
        client_(scoped_ptr<notifier::PushClient>(fake_push_client_)) {}

  virtual void SetUp() {
    StartClient();

    registered_ids_.insert(kBookmarksId_);
    registered_ids_.insert(kPreferencesId_);
    client_.UpdateRegisteredIds(registered_ids_);
  }

  virtual void TearDown() {
    StopClient();
  }

  // Restart client without re-registering IDs.
  void RestartClient() {
    StopClient();
    StartClient();
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

  std::string GetInvalidationState() const {
    return fake_tracker_.GetInvalidationState();
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
    client_.Invalidate(fake_invalidation_client_, inv, ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
    // Pump message loop to trigger
    // InvalidationStateTracker::SetMaxVersion().
    message_loop_.RunAllPending();
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const ObjectId& object_id) {
    const AckHandle ack_handle("fakedata_unknown");
    fake_invalidation_client_->ClearAckedHandles();
    client_.InvalidateUnknownVersion(fake_invalidation_client_, object_id,
                                     ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void FireInvalidateAll() {
    const AckHandle ack_handle("fakedata_all");
    fake_invalidation_client_->ClearAckedHandles();
    client_.InvalidateAll(fake_invalidation_client_, ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void WriteState(const std::string& new_state) {
    client_.WriteState(new_state);
    // Pump message loop to trigger
    // InvalidationStateTracker::WriteState().
    message_loop_.RunAllPending();
  }

  void EnableNotifications() {
    fake_push_client_->EnableNotifications();
  }

  void DisableNotifications(notifier::NotificationsDisabledReason reason) {
    fake_push_client_->DisableNotifications(reason);
  }

  const ObjectId kBookmarksId_;
  const ObjectId kPreferencesId_;
  const ObjectId kExtensionsId_;
  const ObjectId kAppsId_;

  ObjectIdSet registered_ids_;

 private:
  void StartClient() {
    fake_invalidation_client_ = NULL;
    client_.Start(base::Bind(&CreateFakeInvalidationClient,
                             &fake_invalidation_client_),
                  kClientId, kClientInfo, kState,
                  InvalidationVersionMap(),
                  MakeWeakHandle(fake_tracker_.AsWeakPtr()),
                  &fake_delegate_);
    DCHECK(fake_invalidation_client_);
  }

  void StopClient() {
    // client_.StopForTest() stops the invalidation scheduler, which
    // deletes any pending tasks without running them.  Some tasks
    // "run and delete" another task, so they must be run in order to
    // avoid leaking the inner task.  client_.StopForTest() does not
    // schedule any tasks, so it's both necessary and sufficient to
    // drain the task queue before calling it.
    message_loop_.RunAllPending();
    fake_invalidation_client_ = NULL;
    client_.StopForTest();
  }

  MessageLoop message_loop_;

  FakeDelegate fake_delegate_;
  FakeInvalidationStateTracker fake_tracker_;
  notifier::FakePushClient* const fake_push_client_;

 protected:
  // Tests need to access these directly.
  FakeInvalidationClient* fake_invalidation_client_;
  SyncInvalidationListener client_;
};

// Write a new state to the client.  It should propagate to the
// tracker.
TEST_F(SyncInvalidationListenerTest, WriteState) {
  WriteState(kNewState);

  EXPECT_EQ(kNewState, GetInvalidationState());
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
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateWithPayload) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion1, kPayload1);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
}

// Fire an invalidation with a payload.  It should still be processed,
// and both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateUnregistered) {
  const ObjectId kUnregisteredId(
      kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;

  EXPECT_EQ(0, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kMinVersion, GetMaxVersion(id));

  FireInvalidate(id, kVersion1, NULL);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kVersion1, GetMaxVersion(id));
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

  FireInvalidate(id, kVersion1, kPayload1);

  EXPECT_EQ(1, GetInvalidationCount(id));
  EXPECT_EQ(kPayload2, GetPayload(id));
  EXPECT_EQ(kVersion2, GetMaxVersion(id));
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

  FireInvalidateUnknownVersion(id);

  EXPECT_EQ(2, GetInvalidationCount(id));
  EXPECT_EQ("", GetPayload(id));
  EXPECT_EQ(kMinVersion, GetMaxVersion(id));
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
  }
}

// Comprehensive test of various scenarios for multiple IDs.
TEST_F(SyncInvalidationListenerTest, InvalidateMultipleIds) {
  FireInvalidate(kBookmarksId_, 3, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(3, GetMaxVersion(kBookmarksId_));

  FireInvalidate(kExtensionsId_, 2, NULL);

  EXPECT_EQ(1, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(2, GetMaxVersion(kExtensionsId_));

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

  EXPECT_EQ(1, GetInvalidationCount(kPreferencesId_));
  EXPECT_EQ("", GetPayload(kPreferencesId_));
  EXPECT_EQ(kMinVersion, GetMaxVersion(kPreferencesId_));

  EXPECT_EQ(1, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(2, GetMaxVersion(kExtensionsId_));

  // Invalidations with higher version numbers should be processed.

  FireInvalidate(kPreferencesId_, 5, NULL);
  EXPECT_EQ(2, GetInvalidationCount(kPreferencesId_));
  EXPECT_EQ("", GetPayload(kPreferencesId_));
  EXPECT_EQ(5, GetMaxVersion(kPreferencesId_));

  FireInvalidate(kExtensionsId_, 3, NULL);
  EXPECT_EQ(2, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ("", GetPayload(kExtensionsId_));
  EXPECT_EQ(3, GetMaxVersion(kExtensionsId_));

  FireInvalidate(kBookmarksId_, 4, NULL);
  EXPECT_EQ(3, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));
  EXPECT_EQ(4, GetMaxVersion(kBookmarksId_));
}

// Registration tests.

// With IDs already registered, enable notifications then ready the
// client.  The IDs should be registered only after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterEnableReady) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client then enable
// notifications.  The IDs should be registered after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterReadyEnable) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, re-register the IDs, then
// ready the client.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableRegisterReady) {
  client_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.UpdateRegisteredIds(registered_ids_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, ready the client, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableReadyRegister) {
  client_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, enable notifications, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, ReadyEnableRegister) {
  client_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, re-register the IDs, then
// enable notifications. The IDs should be registered only after the
// client is readied.
//
// This test is important: see http://crbug.com/139424.
TEST_F(SyncInvalidationListenerTest, ReadyRegisterEnable) {
  client_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client, restart the client,
// then re-ready it.  The IDs should still be registered.
TEST_F(SyncInvalidationListenerTest, RegisterTypesPreserved) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  RestartClient();

  EXPECT_TRUE(GetRegisteredIds().empty());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
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

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Ready the invalidation client then enable notifications.  The
// delegate should then be ready.
TEST_F(SyncInvalidationListenerTest, ReadyThenEnableNotifications) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then disable
// notifications with an auth error and re-enable notifications.  The
// delegate should go into an auth error mode and then back out.
TEST_F(SyncInvalidationListenerTest, PushClientAuthError) {
  EnableNotifications();
  client_.Ready(fake_invalidation_client_);

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
  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  client_.InformError(
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

  client_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

}  // namespace

}  // namespace syncer
