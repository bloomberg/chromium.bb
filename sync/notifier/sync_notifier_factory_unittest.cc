// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_notifier_factory.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/invalidation_version_tracker.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/model_type_payload_map.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;

class SyncNotifierFactoryTest : public testing::Test {
 protected:

  virtual void SetUp() OVERRIDE {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual void TearDown() OVERRIDE {
    Mock::VerifyAndClearExpectations(&mock_observer_);
    message_loop_.RunAllPending();
  }

  MessageLoop message_loop_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::NotifierOptions notifier_options_;
  scoped_ptr<SyncNotifierFactory> factory_;
};

// Test basic creation of a NonBlockingInvalidationNotifier.
TEST_F(SyncNotifierFactoryTest, Basic) {
  notifier_options_.notification_method = notifier::NOTIFICATION_SERVER;
  SyncNotifierFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<sync_notifier::InvalidationVersionTracker>());
  scoped_ptr<SyncNotifier> notifier(factory.CreateSyncNotifier());
#if defined(OS_ANDROID)
  ASSERT_FALSE(notifier.get());
#else
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
#endif
}

// Test basic creation of a P2PNotifier.
TEST_F(SyncNotifierFactoryTest, Basic_P2P) {
  notifier_options_.notification_method = notifier::NOTIFICATION_P2P;
  SyncNotifierFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<sync_notifier::InvalidationVersionTracker>());
  scoped_ptr<SyncNotifier> notifier(factory.CreateSyncNotifier());
#if defined(OS_ANDROID)
  ASSERT_FALSE(notifier.get());
#else
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
#endif
}

}  // namespace
}  // namespace sync_notifier
