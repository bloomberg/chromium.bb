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
#include "sync/internal_api/public/base/model_type.h"
#include "sync/notifier/fake_sync_notifier_observer.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/sync_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class SyncNotifierFactoryTest : public testing::Test {
 protected:

  virtual void SetUp() OVERRIDE {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    EXPECT_EQ(0, fake_observer_.GetNotificationCount());
  }

  MessageLoop message_loop_;
  FakeSyncNotifierObserver fake_observer_;
  notifier::NotifierOptions notifier_options_;
  scoped_ptr<SyncNotifierFactory> factory_;
};

// Test basic creation of a NonBlockingInvalidationNotifier.
TEST_F(SyncNotifierFactoryTest, Basic) {
  notifier_options_.notification_method = notifier::NOTIFICATION_SERVER;
  SyncNotifierFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<InvalidationStateTracker>());
  scoped_ptr<SyncNotifier> notifier(factory.CreateSyncNotifier());
#if defined(OS_ANDROID)
  ASSERT_FALSE(notifier.get());
#else
  ASSERT_TRUE(notifier.get());
  ObjectIdSet ids = ModelTypeSetToObjectIdSet(ModelTypeSet(syncer::BOOKMARKS));
  notifier->RegisterHandler(&fake_observer_);
  notifier->UpdateRegisteredIds(&fake_observer_, ids);
  notifier->UnregisterHandler(&fake_observer_);
#endif
}

// Test basic creation of a P2PNotifier.
TEST_F(SyncNotifierFactoryTest, Basic_P2P) {
  notifier_options_.notification_method = notifier::NOTIFICATION_P2P;
  SyncNotifierFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<InvalidationStateTracker>());
  scoped_ptr<SyncNotifier> notifier(factory.CreateSyncNotifier());
#if defined(OS_ANDROID)
  ASSERT_FALSE(notifier.get());
#else
  ASSERT_TRUE(notifier.get());
  ObjectIdSet ids = ModelTypeSetToObjectIdSet(ModelTypeSet(syncer::BOOKMARKS));
  notifier->RegisterHandler(&fake_observer_);
  notifier->UpdateRegisteredIds(&fake_observer_, ids);
  notifier->UnregisterHandler(&fake_observer_);
#endif
}

}  // namespace
}  // namespace syncer
