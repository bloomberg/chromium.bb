// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/invalidation_version_tracker.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/model_type_payload_map.h"
#include "sync/util/weak_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class InvalidationNotifierTest : public testing::Test {
 protected:
  virtual void SetUp() {
    notifier::NotifierOptions notifier_options;
    // Note: URLRequestContextGetters are ref-counted.
    notifier_options.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
    invalidation_notifier_.reset(
        new InvalidationNotifier(
            notifier_options,
            InvalidationVersionMap(),
            browser_sync::MakeWeakHandle(
                base::WeakPtr<InvalidationVersionTracker>()),
            "fake_client_info"));
    invalidation_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    invalidation_notifier_->RemoveObserver(&mock_observer_);
    // Stopping the invalidation notifier stops its scheduler, which deletes any
    // pending tasks without running them.  Some tasks "run and delete" another
    // task, so they must be run in order to avoid leaking the inner task.
    // Stopping does not schedule any tasks, so it's both necessary and
    // sufficient to drain the task queue before stopping the notifier.
    message_loop_.RunAllPending();
    invalidation_notifier_.reset();
  }

  MessageLoop message_loop_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
};

// Crashes on Linux and Mac, http://crbug.com/119467
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_F(InvalidationNotifierTest, MAYBE_Basic) {
  InSequence dummy;

  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[syncable::PREFERENCES] = "payload";
  type_payloads[syncable::BOOKMARKS] = "";
  type_payloads[syncable::AUTOFILL] = "";

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_, StoreState("new_fake_state"));
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(type_payloads,
                                     REMOTE_NOTIFICATION));
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(false));

  invalidation_notifier_->SetState("fake_state");
  invalidation_notifier_->SetUniqueId("fake_id");
  invalidation_notifier_->UpdateCredentials("foo@bar.com", "fake_token");

  invalidation_notifier_->OnConnect(fake_base_task_.AsWeakPtr());
  invalidation_notifier_->OnSessionStatusChanged(true);

  invalidation_notifier_->WriteState("new_fake_state");

  invalidation_notifier_->OnInvalidate(type_payloads);

  // Shouldn't trigger notification state change.
  invalidation_notifier_->OnDisconnect();
  invalidation_notifier_->OnConnect(fake_base_task_.AsWeakPtr());

  invalidation_notifier_->OnSessionStatusChanged(false);
}

}  // namespace

}  // namespace sync_notifier
