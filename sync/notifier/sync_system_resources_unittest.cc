// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_system_resources.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"

#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "sync/notifier/push_client_channel.h"
#include "sync/notifier/state_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::_;
using ::testing::SaveArg;

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

class MockClosure {
 public:
  MOCK_CONST_METHOD0(Run, void(void));
  base::Closure* CreateClosure() {
    return new base::Closure(
        base::Bind(&MockClosure::Run, base::Unretained(this)));
  }
};

class MockStorageCallback {
 public:
  MOCK_CONST_METHOD1(Run, void(invalidation::Status));
  base::Callback<void(invalidation::Status)>* CreateCallback() {
    return new base::Callback<void(invalidation::Status)>(
        base::Bind(&MockStorageCallback::Run, base::Unretained(this)));
  }
};

class SyncSystemResourcesTest : public testing::Test {
 protected:
  SyncSystemResourcesTest()
      : push_client_channel_(
            scoped_ptr<notifier::PushClient>(new notifier::FakePushClient())),
        sync_system_resources_(&push_client_channel_, &mock_state_writer_) {}

  virtual ~SyncSystemResourcesTest() {}

  void ScheduleShouldNotRun() {
    {
      // Owned by ScheduleImmediately.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.internal_scheduler()->Schedule(
          invalidation::Scheduler::NoDelay(), should_not_run);
    }
    {
      // Owned by ScheduleOnListenerThread.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.listener_scheduler()->Schedule(
          invalidation::Scheduler::NoDelay(), should_not_run);
    }
    {
      // Owned by ScheduleWithDelay.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.internal_scheduler()->Schedule(
          invalidation::TimeDelta::FromSeconds(0), should_not_run);
    }
  }

  // Needed by |sync_system_resources_|.
  base::MessageLoop message_loop_;
  MockStateWriter mock_state_writer_;
  PushClientChannel push_client_channel_;
  SyncSystemResources sync_system_resources_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncSystemResourcesTest);
};

// Make sure current_time() doesn't crash or leak.
TEST_F(SyncSystemResourcesTest, CurrentTime) {
  invalidation::Time current_time =
      sync_system_resources_.internal_scheduler()->GetCurrentTime();
  DVLOG(1) << "current_time returned: " << current_time.ToInternalValue();
}

// Make sure Log() doesn't crash or leak.
TEST_F(SyncSystemResourcesTest, Log) {
  sync_system_resources_.logger()->Log(SyncLogger::INFO_LEVEL,
                                         __FILE__, __LINE__, "%s %d",
                                         "test string", 5);
}

TEST_F(SyncSystemResourcesTest, ScheduleBeforeStart) {
  ScheduleShouldNotRun();
  sync_system_resources_.Start();
}

TEST_F(SyncSystemResourcesTest, ScheduleAfterStop) {
  sync_system_resources_.Start();
  sync_system_resources_.Stop();
  ScheduleShouldNotRun();
}

TEST_F(SyncSystemResourcesTest, ScheduleAndStop) {
  sync_system_resources_.Start();
  ScheduleShouldNotRun();
  sync_system_resources_.Stop();
}

TEST_F(SyncSystemResourcesTest, ScheduleAndDestroy) {
  sync_system_resources_.Start();
  ScheduleShouldNotRun();
}

TEST_F(SyncSystemResourcesTest, ScheduleImmediately) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.internal_scheduler()->Schedule(
      invalidation::Scheduler::NoDelay(), mock_closure.CreateClosure());
  message_loop_.RunUntilIdle();
}

TEST_F(SyncSystemResourcesTest, ScheduleOnListenerThread) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.listener_scheduler()->Schedule(
      invalidation::Scheduler::NoDelay(), mock_closure.CreateClosure());
  EXPECT_TRUE(
      sync_system_resources_.internal_scheduler()->IsRunningOnThread());
  message_loop_.RunUntilIdle();
}

TEST_F(SyncSystemResourcesTest, ScheduleWithZeroDelay) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.internal_scheduler()->Schedule(
      invalidation::TimeDelta::FromSeconds(0), mock_closure.CreateClosure());
  message_loop_.RunUntilIdle();
}

// TODO(akalin): Figure out how to test with a non-zero delay.

TEST_F(SyncSystemResourcesTest, WriteState) {
  sync_system_resources_.Start();
  EXPECT_CALL(mock_state_writer_, WriteState(_));
  // Owned by WriteState.
  MockStorageCallback mock_storage_callback;
  invalidation::Status results(invalidation::Status::PERMANENT_FAILURE,
                               "fake-failure");
  EXPECT_CALL(mock_storage_callback, Run(_))
      .WillOnce(SaveArg<0>(&results));
  sync_system_resources_.storage()->WriteKey(
      std::string(), "state", mock_storage_callback.CreateCallback());
  message_loop_.RunUntilIdle();
  EXPECT_EQ(invalidation::Status(invalidation::Status::SUCCESS, std::string()),
            results);
}

class TestSyncNetworkChannel : public SyncNetworkChannel {
 public:
  TestSyncNetworkChannel() {}
  virtual ~TestSyncNetworkChannel() {}

  using SyncNetworkChannel::NotifyStateChange;
  using SyncNetworkChannel::DeliverIncomingMessage;

  virtual void SendEncodedMessage(const std::string& encoded_message) OVERRIDE {
    last_encoded_message_ = encoded_message;
  }

  virtual void UpdateCredentials(const std::string& email,
      const std::string& token) OVERRIDE {
  }

  std::string last_encoded_message_;
};

class SyncNetworkChannelTest
    : public testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  SyncNetworkChannelTest()
      : last_invalidator_state_(DEFAULT_INVALIDATION_ERROR),
        connected_(false) {
    network_channel_.AddObserver(this);
    network_channel_.SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &SyncNetworkChannelTest::OnIncomingMessage));
    network_channel_.AddNetworkStatusReceiver(
        invalidation::NewPermanentCallback(
            this, &SyncNetworkChannelTest::OnNetworkStatusChange));
  }

  virtual ~SyncNetworkChannelTest() {
    network_channel_.RemoveObserver(this);
  }

  virtual void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) OVERRIDE {
    last_invalidator_state_ = invalidator_state;
  }

  void OnIncomingMessage(std::string incoming_message) {
    last_message_ = incoming_message;
  }

  void OnNetworkStatusChange(bool connected) {
    connected_ = connected;
  }

  TestSyncNetworkChannel network_channel_;
  InvalidatorState last_invalidator_state_;
  std::string last_message_;
  bool connected_;
};

const char kMessage[] = "message";
const char kServiceContext[] = "service context";
const int64 kSchedulingHash = 100;

// Encode a message with some context and then decode it.  The decoded info
// should match the original info.
TEST_F(SyncNetworkChannelTest, EncodeDecode) {
  const std::string& data =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);
  std::string message;
  std::string service_context;
  int64 scheduling_hash = 0LL;
  EXPECT_TRUE(SyncNetworkChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Encode a message with no context and then decode it.  The decoded message
// should match the original message, but the context and hash should be
// untouched.
TEST_F(SyncNetworkChannelTest, EncodeDecodeNoContext) {
  const std::string& data =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage, std::string(), kSchedulingHash);
  std::string message;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash + 1;
  EXPECT_TRUE(SyncNetworkChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash + 1, scheduling_hash);
}

// Decode an empty notification. It should result in an empty message
// but should leave the context and hash untouched.
TEST_F(SyncNetworkChannelTest, DecodeEmpty) {
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash;
  EXPECT_TRUE(SyncNetworkChannel::DecodeMessageForTest(
      std::string(), &message, &service_context, &scheduling_hash));
  EXPECT_TRUE(message.empty());
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Try to decode a garbage notification.  It should leave all its
// arguments untouched and return false.
TEST_F(SyncNetworkChannelTest, DecodeGarbage) {
  std::string data = "garbage";
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash;
  EXPECT_FALSE(SyncNetworkChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Simulate network channel state change. It should propagate to observer.
TEST_F(SyncNetworkChannelTest, OnNetworkChannelStateChanged) {
  EXPECT_EQ(DEFAULT_INVALIDATION_ERROR, last_invalidator_state_);
  EXPECT_FALSE(connected_);
  network_channel_.NotifyStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, last_invalidator_state_);
  EXPECT_TRUE(connected_);
  network_channel_.NotifyStateChange(INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, last_invalidator_state_);
  EXPECT_FALSE(connected_);
}

// Call SendMessage on the channel.  SendEncodedMessage should be called for it.
TEST_F(SyncNetworkChannelTest, SendMessage) {
  network_channel_.SendMessage(kMessage);
  std::string expected_encoded_message =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage,
          network_channel_.GetServiceContextForTest(),
          network_channel_.GetSchedulingHashForTest());
  ASSERT_EQ(expected_encoded_message, network_channel_.last_encoded_message_);
}

// Simulate an incoming notification. It should be decoded properly
// by the channel.
TEST_F(SyncNetworkChannelTest, OnIncomingMessage) {
  const std::string message =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);

  network_channel_.DeliverIncomingMessage(message);
  EXPECT_EQ(kServiceContext,
            network_channel_.GetServiceContextForTest());
  EXPECT_EQ(kSchedulingHash,
            network_channel_.GetSchedulingHashForTest());
  EXPECT_EQ(kMessage, last_message_);
}

// Simulate an incoming notification with no receiver. It should be dropped by
// the channel.
TEST_F(SyncNetworkChannelTest, OnIncomingMessageNoReceiver) {
  const std::string message =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);

  network_channel_.SetMessageReceiver(NULL);
  network_channel_.DeliverIncomingMessage(message);
  EXPECT_TRUE(network_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64>(0),
            network_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Simulate an incoming garbage notification. It should be dropped by
// the channel.
TEST_F(SyncNetworkChannelTest, OnIncomingMessageGarbage) {
  std::string message = "garbage";

  network_channel_.DeliverIncomingMessage(message);
  EXPECT_TRUE(network_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64>(0),
            network_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Send a message, simulate an incoming message with context, and then
// send the same message again.  The first sent message should not
// have any context, but the second sent message should have the
// context from the incoming emssage.
TEST_F(SyncNetworkChannelTest, PersistedMessageState) {
  network_channel_.SendMessage(kMessage);
  ASSERT_FALSE(network_channel_.last_encoded_message_.empty());
  {
    std::string message;
    std::string service_context;
    int64 scheduling_hash = 0LL;
    EXPECT_TRUE(SyncNetworkChannel::DecodeMessageForTest(
        network_channel_.last_encoded_message_,
        &message, &service_context, &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_TRUE(service_context.empty());
    EXPECT_EQ(0LL, scheduling_hash);
  }

  const std::string& encoded_message =
      SyncNetworkChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);
  network_channel_.DeliverIncomingMessage(encoded_message);

  network_channel_.last_encoded_message_.clear();
  network_channel_.SendMessage(kMessage);
  ASSERT_FALSE(network_channel_.last_encoded_message_.empty());
  {
    std::string message;
    std::string service_context;
    int64 scheduling_hash = 0LL;
    EXPECT_TRUE(SyncNetworkChannel::DecodeMessageForTest(
        network_channel_.last_encoded_message_,
        &message, &service_context, &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_EQ(kServiceContext, service_context);
    EXPECT_EQ(kSchedulingHash, scheduling_hash);
  }
}

}  // namespace
}  // namespace syncer
