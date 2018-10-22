// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_invalidation_client.h"

#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/logger.h"
#include "components/invalidation/impl/network_channel.h"
#include "google/cacheinvalidation/include/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class MockInvalidationListener : public InvalidationListener {
 public:
  MOCK_METHOD1(Ready, void(InvalidationClient*));
  MOCK_METHOD5(Invalidate,
               void(InvalidationClient*,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    int64_t));
  MOCK_METHOD2(InformTokenRecieved,
               void(InvalidationClient*, const std::string&));
};

// A mock of the Network interface.
class MockNetwork : public NetworkChannel {
 public:
  MOCK_METHOD1(SetMessageReceiver, void(MessageCallback));
  MOCK_METHOD1(SetTokenReceiver, void(TokenCallback));
};

// Tests the basic functionality of the invalidation client.
class InvalidationClientImplTest : public testing::Test {
 public:
  ~InvalidationClientImplTest() override {}

  // Performs setup for protocol handler unit tests, e.g. creating resource
  // components and setting up common expectations for certain mock objects.
  void SetUp() override {
    client_ = std::make_unique<PerUserTopicInvalidationClient>(
        &network_, &logger_, &listener_);
  }

  // The client being tested. Created fresh for each test function.
  std::unique_ptr<InvalidationClient> client_;
  MockNetwork network_;
  Logger logger_;

  // A mock invalidation listener.
  MockInvalidationListener listener_;
};

// Starts the ticl and checks that appropriate calls are made on the listener
// and that a proper message is sent on the network. Test is disabled on ASAN
// because using callback mechanism from cacheinvalidations library introduces a
// leak.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_Start DISABLED_Start
#else
#define MAYBE_Start Start
#endif
TEST_F(InvalidationClientImplTest, MAYBE_Start) {
  // Expect the listener to indicate that it is ready.
  EXPECT_CALL(listener_, Ready(testing::Eq(client_.get())));
  client_->Start();
}

}  // namespace invalidation
