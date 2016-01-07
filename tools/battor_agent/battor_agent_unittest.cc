// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/battor_agent/battor_protocol_types.h"

using namespace testing;

namespace battor {

namespace {

BattOrControlMessageAck kInitAck{BATTOR_CONTROL_MESSAGE_TYPE_INIT, 0};
BattOrControlMessageAck kSetGainAck{BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN, 0};
BattOrControlMessageAck kStartTracingAck{
    BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, 0};

// Creates a byte vector copy of the specified ack.
scoped_ptr<std::vector<char>> AckToCharVector(BattOrControlMessageAck ack) {
  return scoped_ptr<std::vector<char>>(
      new std::vector<char>(reinterpret_cast<char*>(&ack),
                            reinterpret_cast<char*>(&ack) + sizeof(ack)));
}

MATCHER_P2(
    BufferEq,
    expected_buffer,
    expected_buffer_size,
    "Makes sure that the argument has the same contents as the buffer.") {
  return memcmp(reinterpret_cast<const void*>(arg),
                reinterpret_cast<const void*>(expected_buffer),
                expected_buffer_size) == 0;
}

class MockBattOrConnection : public BattOrConnection {
 public:
  MockBattOrConnection(BattOrConnection::Listener* listener)
      : BattOrConnection(listener) {}
  ~MockBattOrConnection() override {}

  MOCK_METHOD0(Open, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD3(SendBytes,
               void(BattOrMessageType type,
                    const void* buffer,
                    size_t bytes_to_send));
  MOCK_METHOD1(ReadMessage, void(BattOrMessageType type));
  MOCK_METHOD0(Flush, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBattOrConnection);
};

}  // namespace

// TestableBattOrAgent uses a fake BattOrConnection to be testable.
class TestableBattOrAgent : public BattOrAgent {
 public:
  TestableBattOrAgent(BattOrAgent::Listener* listener)
      : BattOrAgent("/dev/test", listener, nullptr, nullptr) {
    connection_ = scoped_ptr<BattOrConnection>(new MockBattOrConnection(this));
  }

  MockBattOrConnection* GetConnection() {
    return static_cast<MockBattOrConnection*>(connection_.get());
  }
};

// BattOrAgentTest provides a BattOrAgent and captures the results of its
// tracing commands.
class BattOrAgentTest : public testing::Test, public BattOrAgent::Listener {
 public:
  BattOrAgentTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  void OnStartTracingComplete(BattOrError error) override {
    is_start_tracing_complete_ = true;
    start_tracing_error_ = error;
  }

 protected:
  void SetUp() override {
    agent_.reset(new TestableBattOrAgent(this));
    task_runner_->ClearPendingTasks();
    is_start_tracing_complete_ = false;
  }

  // Possible states that the BattOrAgent can be in.
  enum class BattOrAgentState {
    CONNECTED,
    RESET_SENT,
    INIT_SENT,
    INIT_ACKED,
    SET_GAIN_SENT,
    GAIN_ACKED,
    START_TRACING_SENT,
    START_TRACING_COMPLETE,
  };

  // Runs BattOrAgent::StartTracing until it reaches the specified state by
  // feeding it the callbacks it needs to progress.
  void RunStartTracingTo(BattOrAgentState end_state) {
    GetAgent()->StartTracing();
    GetTaskRunner()->RunUntilIdle();

    GetAgent()->OnConnectionOpened(true);
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::CONNECTED)
      return;

    GetAgent()->OnBytesSent(true);
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::RESET_SENT)
      return;

    GetAgent()->OnBytesSent(true);
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::INIT_SENT)
      return;

    GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                              AckToCharVector(kInitAck));
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::INIT_ACKED)
      return;

    GetAgent()->OnBytesSent(true);
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::SET_GAIN_SENT)
      return;

    GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                              AckToCharVector(kSetGainAck));
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::GAIN_ACKED)
      return;

    GetAgent()->OnBytesSent(true);
    GetTaskRunner()->RunUntilIdle();

    if (end_state == BattOrAgentState::START_TRACING_SENT)
      return;

    // Make sure that we're actually forwarding to a state in the start tracing
    // state machine.
    DCHECK(end_state == BattOrAgentState::START_TRACING_COMPLETE);

    GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                              AckToCharVector(kStartTracingAck));
    GetTaskRunner()->RunUntilIdle();
  }

  TestableBattOrAgent* GetAgent() { return agent_.get(); }

  scoped_refptr<base::TestSimpleTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  BattOrError GetStartTracingError() { return start_tracing_error_; }

  bool IsStartTracingComplete() { return is_start_tracing_complete_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  // Needed to support ThreadTaskRunnerHandle::Get() in code under test.
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

  scoped_ptr<TestableBattOrAgent> agent_;
  bool is_start_tracing_complete_;

  BattOrError start_tracing_error_;
};

TEST_F(BattOrAgentTest, StartTracing) {
  testing::InSequence s;
  EXPECT_CALL(*GetAgent()->GetConnection(), Open());

  BattOrControlMessage reset_msg{BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0, 0};
  EXPECT_CALL(
      *GetAgent()->GetConnection(),
      SendBytes(BATTOR_MESSAGE_TYPE_CONTROL,
                BufferEq(&reset_msg, sizeof(reset_msg)), sizeof(reset_msg)));

  EXPECT_CALL(*GetAgent()->GetConnection(), Flush());
  BattOrControlMessage init_msg{BATTOR_CONTROL_MESSAGE_TYPE_INIT, 0, 0};
  EXPECT_CALL(
      *GetAgent()->GetConnection(),
      SendBytes(BATTOR_MESSAGE_TYPE_CONTROL,
                BufferEq(&init_msg, sizeof(init_msg)), sizeof(init_msg)));

  EXPECT_CALL(*GetAgent()->GetConnection(),
              ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK));

  BattOrControlMessage set_gain_msg{BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN,
                                    BATTOR_GAIN_LOW, 0};
  EXPECT_CALL(*GetAgent()->GetConnection(),
              SendBytes(BATTOR_MESSAGE_TYPE_CONTROL,
                        BufferEq(&set_gain_msg, sizeof(set_gain_msg)),
                        sizeof(set_gain_msg)));

  EXPECT_CALL(*GetAgent()->GetConnection(),
              ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK));

  BattOrControlMessage start_tracing_msg{
      BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, 0, 0};
  EXPECT_CALL(*GetAgent()->GetConnection(),
              SendBytes(BATTOR_MESSAGE_TYPE_CONTROL,
                        BufferEq(&start_tracing_msg, sizeof(start_tracing_msg)),
                        sizeof(start_tracing_msg)));

  EXPECT_CALL(*GetAgent()->GetConnection(),
              ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK));

  RunStartTracingTo(BattOrAgentState::START_TRACING_COMPLETE);
  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_NONE, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsWithoutConnection) {
  GetAgent()->StartTracing();
  GetTaskRunner()->RunUntilIdle();

  GetAgent()->OnConnectionOpened(false);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_CONNECTION_FAILED, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfResetSendFails) {
  RunStartTracingTo(BattOrAgentState::CONNECTED);
  GetAgent()->OnBytesSent(false);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_SEND_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfInitSendFails) {
  RunStartTracingTo(BattOrAgentState::RESET_SENT);
  GetAgent()->OnBytesSent(false);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_SEND_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfInitAckReadFails) {
  RunStartTracingTo(BattOrAgentState::INIT_SENT);
  GetAgent()->OnMessageRead(false, BATTOR_MESSAGE_TYPE_CONTROL_ACK, nullptr);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_RECEIVE_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfInitWrongAckRead) {
  RunStartTracingTo(BattOrAgentState::INIT_SENT);
  GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                            AckToCharVector(kStartTracingAck));
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_UNEXPECTED_MESSAGE, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfSetGainSendFails) {
  RunStartTracingTo(BattOrAgentState::RESET_SENT);
  GetAgent()->OnBytesSent(false);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_SEND_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfSetGainAckReadFails) {
  RunStartTracingTo(BattOrAgentState::SET_GAIN_SENT);
  GetAgent()->OnMessageRead(false, BATTOR_MESSAGE_TYPE_CONTROL_ACK, nullptr);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_RECEIVE_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfSetGainWrongAckRead) {
  RunStartTracingTo(BattOrAgentState::SET_GAIN_SENT);
  GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                            AckToCharVector(kStartTracingAck));
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_UNEXPECTED_MESSAGE, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfStartTracingSendFails) {
  RunStartTracingTo(BattOrAgentState::RESET_SENT);
  GetAgent()->OnBytesSent(false);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_SEND_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfStartTracingAckReadFails) {
  RunStartTracingTo(BattOrAgentState::START_TRACING_SENT);
  GetAgent()->OnMessageRead(false, BATTOR_MESSAGE_TYPE_CONTROL_ACK, nullptr);
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_RECEIVE_ERROR, GetStartTracingError());
}

TEST_F(BattOrAgentTest, StartTracingFailsIfStartTracingWrongAckRead) {
  RunStartTracingTo(BattOrAgentState::START_TRACING_SENT);
  GetAgent()->OnMessageRead(true, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
                            AckToCharVector(kInitAck));
  GetTaskRunner()->RunUntilIdle();

  EXPECT_TRUE(IsStartTracingComplete());
  EXPECT_EQ(BATTOR_ERROR_UNEXPECTED_MESSAGE, GetStartTracingError());
}

}  // namespace battor
