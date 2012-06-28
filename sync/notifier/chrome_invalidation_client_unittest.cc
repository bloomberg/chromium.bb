// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/v2/types.pb.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/chrome_invalidation_client.h"
#include "sync/notifier/mock_invalidation_state_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

const char kClientId[] = "client_id";
const char kClientInfo[] = "client_info";
const char kState[] = "state";
const char kNewState[] = "new_state";

const int kChromeSyncSourceId = 1004;

class MockInvalidationClient : public invalidation::InvalidationClient {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(Register, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Register, void(const std::vector<invalidation::ObjectId>&));
  MOCK_METHOD1(Unregister, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Unregister, void(const std::vector<invalidation::ObjectId>&));
  MOCK_METHOD1(Acknowledge, void(const invalidation::AckHandle&));
};

class MockListener : public ChromeInvalidationClient::Listener {
 public:
  MOCK_METHOD1(OnInvalidate, void(const syncable::ModelTypePayloadMap&));
  MOCK_METHOD0(OnNotificationsEnabled, void());
  MOCK_METHOD1(OnNotificationsDisabled, void(NotificationsDisabledReason));
};

}  // namespace

class ChromeInvalidationClientTest : public testing::Test {
 protected:
  ChromeInvalidationClientTest()
      : fake_push_client_(new notifier::FakePushClient()),
        client_(scoped_ptr<notifier::PushClient>(fake_push_client_)),
        kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kExtensionsId_(kChromeSyncSourceId, "EXTENSION"),
        kAppsId_(kChromeSyncSourceId, "APP") {}

  virtual void SetUp() {
    client_.Start(kClientId, kClientInfo, kState,
                  InvalidationVersionMap(),
                  syncer::MakeWeakHandle(
                      mock_invalidation_state_tracker_.AsWeakPtr()),
                  &mock_listener_);
  }

  virtual void TearDown() {
    // client_.Stop() stops the invalidation scheduler, which deletes any
    // pending tasks without running them.  Some tasks "run and delete" another
    // task, so they must be run in order to avoid leaking the inner task.
    // client_.Stop() does not schedule any tasks, so it's both necessary and
    // sufficient to drain the task queue before calling it.
    message_loop_.RunAllPending();
    client_.Stop();
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidate(const char* type_name,
                      int64 version, const char* payload) {
    const invalidation::ObjectId object_id(
        ipc::invalidation::ObjectSource::CHROME_SYNC, type_name);
    std::string payload_tmp = payload ? payload : "";
    invalidation::Invalidation inv;
    if (payload) {
      inv = invalidation::Invalidation(object_id, version, payload);
    } else {
      inv = invalidation::Invalidation(object_id, version);
    }
    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.Invalidate(&mock_invalidation_client_, inv, ack_handle);
    // Pump message loop to trigger
    // InvalidationStateTracker::SetMaxVersion().
    message_loop_.RunAllPending();
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const char* type_name) {
    const invalidation::ObjectId object_id(
        ipc::invalidation::ObjectSource::CHROME_SYNC, type_name);

    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.InvalidateUnknownVersion(&mock_invalidation_client_, object_id,
                                     ack_handle);
  }

  void FireInvalidateAll() {
    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.InvalidateAll(&mock_invalidation_client_, ack_handle);
  }

  MessageLoop message_loop_;
  StrictMock<MockListener> mock_listener_;
  StrictMock<MockInvalidationStateTracker>
      mock_invalidation_state_tracker_;
  StrictMock<MockInvalidationClient> mock_invalidation_client_;
  notifier::FakePushClient* const fake_push_client_;
  ChromeInvalidationClient client_;

  const invalidation::ObjectId kBookmarksId_;
  const invalidation::ObjectId kPreferencesId_;
  const invalidation::ObjectId kExtensionsId_;
  const invalidation::ObjectId kAppsId_;
};

namespace {

syncable::ModelTypePayloadMap MakeMap(syncable::ModelType model_type,
                                      const std::string& payload) {
  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[model_type] = payload;
  return type_payloads;
}

syncable::ModelTypePayloadMap MakeMapFromSet(syncable::ModelTypeSet types,
                                             const std::string& payload) {
  return syncable::ModelTypePayloadMapFromEnumSet(types, payload);
}

}  // namespace

TEST_F(ChromeInvalidationClientTest, InvalidateBadObjectId) {
  syncable::ModelTypeSet types(syncable::BOOKMARKS, syncable::APPS);
  client_.RegisterTypes(types);
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(invalidation::ObjectId(kChromeSyncSourceId, "bad"),
                            1));
  FireInvalidate("bad", 1, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateNoPayload) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::BOOKMARKS, "")));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kBookmarksId_, 1));
  FireInvalidate("BOOKMARK", 1, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateWithPayload) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::PREFERENCES, "payload")));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kPreferencesId_, 1));
  FireInvalidate("PREFERENCE", 1, "payload");
}

TEST_F(ChromeInvalidationClientTest, InvalidateVersion) {
  using ::testing::Mock;

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kAppsId_, 1));

  // Should trigger.
  FireInvalidate("APP", 1, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);

  // Should be dropped.
  FireInvalidate("APP", 1, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateUnknownVersion) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")))
      .Times(2);

  // Should trigger twice.
  FireInvalidateUnknownVersion("EXTENSION");
  FireInvalidateUnknownVersion("EXTENSION");
}

TEST_F(ChromeInvalidationClientTest, InvalidateVersionMultipleTypes) {
  using ::testing::Mock;

  syncable::ModelTypeSet types(syncable::BOOKMARKS, syncable::APPS);
  client_.RegisterTypes(types);

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")));

  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kAppsId_, 3));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kExtensionsId_, 2));

  // Should trigger both.
  FireInvalidate("APP", 3, NULL);
  FireInvalidate("EXTENSION", 2, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_state_tracker_);

  // Should both be dropped.
  FireInvalidate("APP", 1, NULL);
  FireInvalidate("EXTENSION", 1, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_state_tracker_);

  // InvalidateAll shouldn't change any version state.
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_state_tracker_);

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::PREFERENCES, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));

  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kPreferencesId_, 5));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kExtensionsId_, 3));
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetMaxVersion(kAppsId_, 4));

  // Should trigger all three.
  FireInvalidate("PREFERENCE", 5, NULL);
  FireInvalidate("EXTENSION", 3, NULL);
  FireInvalidate("APP", 4, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateAll) {
  syncable::ModelTypeSet types(syncable::PREFERENCES, syncable::EXTENSIONS);
  client_.RegisterTypes(types);
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();
}

TEST_F(ChromeInvalidationClientTest, RegisterTypes) {
  syncable::ModelTypeSet types(syncable::PREFERENCES, syncable::EXTENSIONS);
  client_.RegisterTypes(types);
  // Registered types should be preserved across Stop/Start.
  TearDown();
  SetUp();
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();
}

TEST_F(ChromeInvalidationClientTest, WriteState) {
  EXPECT_CALL(mock_invalidation_state_tracker_,
              SetInvalidationState(kNewState));
  client_.WriteState(kNewState);
}

TEST_F(ChromeInvalidationClientTest, StateChangesNotReady) {
  InSequence dummy;
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(NOTIFICATION_CREDENTIALS_REJECTED));
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));

  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  fake_push_client_->EnableNotifications();
}

TEST_F(ChromeInvalidationClientTest, StateChangesReady) {
  InSequence dummy;
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  EXPECT_CALL(mock_listener_, OnNotificationsEnabled());
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(NOTIFICATION_CREDENTIALS_REJECTED));
  EXPECT_CALL(mock_listener_, OnNotificationsEnabled());

  fake_push_client_->EnableNotifications();
  client_.Ready(NULL);
  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  fake_push_client_->EnableNotifications();
}

TEST_F(ChromeInvalidationClientTest, StateChangesAuthError) {
  InSequence dummy;
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  EXPECT_CALL(mock_listener_, OnNotificationsEnabled());
  EXPECT_CALL(mock_listener_,
              OnNotificationsDisabled(NOTIFICATION_CREDENTIALS_REJECTED))
      .Times(4);
  EXPECT_CALL(mock_listener_, OnNotificationsEnabled());

  fake_push_client_->EnableNotifications();
  client_.Ready(NULL);

  client_.InformError(
      NULL,
      invalidation::ErrorInfo(
          invalidation::ErrorReason::AUTH_FAILURE,
          false /* is_transient */,
          "auth error",
          invalidation::ErrorContext()));
  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  fake_push_client_->EnableNotifications();
  client_.Ready(NULL);
}

}  // namespace syncer
