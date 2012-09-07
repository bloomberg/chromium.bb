// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidator_factory.h"

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
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class InvalidatorFactoryTest : public testing::Test {
 protected:

  virtual void SetUp() OVERRIDE {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    EXPECT_EQ(0, fake_handler_.GetInvalidationCount());
  }

  MessageLoop message_loop_;
  FakeInvalidationHandler fake_handler_;
  notifier::NotifierOptions notifier_options_;
  scoped_ptr<InvalidatorFactory> factory_;
};

// Test basic creation of a NonBlockingInvalidationNotifier.
TEST_F(InvalidatorFactoryTest, Basic) {
  notifier_options_.notification_method = notifier::NOTIFICATION_SERVER;
  InvalidatorFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<InvalidationStateTracker>());
  scoped_ptr<Invalidator> invalidator(factory.CreateInvalidator());
#if defined(OS_ANDROID)
  ASSERT_FALSE(invalidator.get());
#else
  ASSERT_TRUE(invalidator.get());
  ObjectIdSet ids = ModelTypeSetToObjectIdSet(ModelTypeSet(syncer::BOOKMARKS));
  invalidator->RegisterHandler(&fake_handler_);
  invalidator->UpdateRegisteredIds(&fake_handler_, ids);
  invalidator->UnregisterHandler(&fake_handler_);
#endif
}

// Test basic creation of a P2PNotifier.
TEST_F(InvalidatorFactoryTest, Basic_P2P) {
  notifier_options_.notification_method = notifier::NOTIFICATION_P2P;
  InvalidatorFactory factory(
      notifier_options_,
      "test client info",
      base::WeakPtr<InvalidationStateTracker>());
  scoped_ptr<Invalidator> invalidator(factory.CreateInvalidator());
#if defined(OS_ANDROID)
  ASSERT_FALSE(invalidator.get());
#else
  ASSERT_TRUE(invalidator.get());
  ObjectIdSet ids = ModelTypeSetToObjectIdSet(ModelTypeSet(syncer::BOOKMARKS));
  invalidator->RegisterHandler(&fake_handler_);
  invalidator->UpdateRegisteredIds(&fake_handler_, ids);
  invalidator->UnregisterHandler(&fake_handler_);
#endif
}

}  // namespace
}  // namespace syncer
