// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/mock_objects.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/mock_objects.h"
#include "remoting/protocol/session_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace remoting {

namespace {

void PostQuitTask(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

// Run the task and delete it afterwards. This action is used to deal with
// done callbacks.
ACTION(RunDoneTask) {
  arg1->Run();
  delete arg1;
}

ACTION_P(QuitMainMessageLoop, message_loop) {
  PostQuitTask(message_loop);
}

}  // namepace

class ChromotingHostTest : public testing::Test {
 public:
  ChromotingHostTest() {
  }

  virtual void SetUp() {
    config_ = new InMemoryHostConfig();
    ON_CALL(context_, main_message_loop())
        .WillByDefault(Return(&message_loop_));
    ON_CALL(context_, encode_message_loop())
        .WillByDefault(Return(&message_loop_));
    ON_CALL(context_, network_message_loop())
        .WillByDefault(Return(&message_loop_));
    EXPECT_CALL(context_, main_message_loop())
        .Times(AnyNumber());
    EXPECT_CALL(context_, encode_message_loop())
        .Times(AnyNumber());
    EXPECT_CALL(context_, network_message_loop())
        .Times(AnyNumber());

    Capturer* capturer = new CapturerFake(context_.main_message_loop());
    input_stub_ = new protocol::MockInputStub();
    host_ = ChromotingHost::Create(&context_, config_, capturer, input_stub_);
    connection_ = new protocol::MockConnectionToClient();
    session_ = new protocol::MockSession();
    session_config_.reset(protocol::SessionConfig::CreateDefault());

    ON_CALL(*connection_.get(), video_stub())
        .WillByDefault(Return(&video_stub_));
    ON_CALL(*connection_.get(), session())
        .WillByDefault(Return(session_));
    ON_CALL(*session_.get(), config())
        .WillByDefault(Return(session_config_.get()));
  }

  virtual void TearDown() {
  }

  // Helper metjod to pretend a client is connected to ChromotingHost.
  void InjectClientConnection() {
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::OnConnectionOpened,
                          connection_));
  }

  // Helper method to remove a client connection from ChromotongHost.
  void RemoveClientConnection() {
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::OnConnectionClosed,
                          connection_));
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<ChromotingHost> host_;
  scoped_refptr<InMemoryHostConfig> config_;
  MockChromotingHostContext context_;
  scoped_refptr<protocol::MockConnectionToClient> connection_;
  scoped_refptr<protocol::MockSession> session_;
  scoped_ptr<protocol::SessionConfig> session_config_;
  protocol::MockVideoStub video_stub_;
  protocol::MockInputStub* input_stub_;
};

TEST_F(ChromotingHostTest, StartAndShutdown) {
  host_->Start(NewRunnableFunction(&PostQuitTask, &message_loop_));

  message_loop_.PostTask(FROM_HERE,
                         NewRunnableMethod(host_.get(),
                                           &ChromotingHost::Shutdown));
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Connect) {
  host_->Start(NewRunnableFunction(&PostQuitTask, &message_loop_));

  ON_CALL(video_stub_, ProcessVideoPacket(_, _))
      .WillByDefault(
          DoAll(DeleteArg<0>(), DeleteArg<1>()));

  // When the video packet is received we first shutdown ChromotingHost
  // then execute the done task.
  InSequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(host_.get(), &ChromotingHost::Shutdown),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .Times(AnyNumber());

  InjectClientConnection();
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Reconnect) {
  host_->Start(NewRunnableFunction(&PostQuitTask, &message_loop_));

  ON_CALL(video_stub_, ProcessVideoPacket(_, _))
      .WillByDefault(
          DoAll(DeleteArg<0>(), DeleteArg<1>()));

  // When the video packet is received we first disconnect the mock
  // connection.
  InSequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ChromotingHostTest::RemoveClientConnection),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .Times(AnyNumber());

  // If Disconnect() is called we can break the main message loop.
  EXPECT_CALL(*connection_.get(), Disconnect())
      .WillOnce(QuitMainMessageLoop(&message_loop_))
      .RetiresOnSaturation();

  InjectClientConnection();
  message_loop_.Run();

  // Connect the client again.
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(host_.get(), &ChromotingHost::Shutdown),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
      .Times(AnyNumber());

  InjectClientConnection();
  message_loop_.Run();
}

}  // namespace remoting
