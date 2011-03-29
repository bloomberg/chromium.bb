// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/session_config.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::remoting::protocol::LocalLoginCredentials;
using ::remoting::protocol::MockClientStub;
using ::remoting::protocol::MockConnectionToClient;
using ::remoting::protocol::MockConnectionToClientEventHandler;
using ::remoting::protocol::MockHostStub;
using ::remoting::protocol::MockInputStub;
using ::remoting::protocol::MockSession;
using ::remoting::protocol::MockVideoStub;
using ::remoting::protocol::SessionConfig;

using testing::_;
using testing::AnyNumber;
using testing::CreateFunctor;
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

}  // namespace

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

    Capturer* capturer = new CapturerFake();
    host_stub_ = new MockHostStub();
    host_stub2_ = new MockHostStub();
    input_stub_ = new MockInputStub();
    input_stub2_ = new MockInputStub();
    DesktopEnvironment* desktop =
        new DesktopEnvironment(capturer, input_stub_);
    host_ = ChromotingHost::Create(&context_, config_, desktop);
    connection_ = new MockConnectionToClient(
        &message_loop_, &handler_, host_stub_, input_stub_);
    connection2_ = new MockConnectionToClient(
        &message_loop_, &handler_, host_stub2_, input_stub2_);
    session_ = new MockSession();
    session2_ = new MockSession();
    session_config_.reset(SessionConfig::CreateDefault());
    session_config2_.reset(SessionConfig::CreateDefault());

    ON_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillByDefault(
            DoAll(DeleteArg<0>(), DeleteArg<1>()));
    ON_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .WillByDefault(
            DoAll(DeleteArg<0>(), DeleteArg<1>()));
    ON_CALL(*connection_.get(), video_stub())
        .WillByDefault(Return(&video_stub_));
    ON_CALL(*connection_.get(), client_stub())
        .WillByDefault(Return(&client_stub_));
    ON_CALL(*connection_.get(), session())
        .WillByDefault(Return(session_));
    ON_CALL(*connection2_.get(), video_stub())
        .WillByDefault(Return(&video_stub2_));
    ON_CALL(*connection2_.get(), client_stub())
        .WillByDefault(Return(&client_stub2_));
    ON_CALL(*connection2_.get(), session())
        .WillByDefault(Return(session2_));
    ON_CALL(*session_.get(), config())
        .WillByDefault(Return(session_config_.get()));
    ON_CALL(*session2_.get(), config())
        .WillByDefault(Return(session_config2_.get()));
    EXPECT_CALL(*connection_.get(), video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection_.get(), client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection_.get(), session())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_.get(), video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_.get(), client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_.get(), session())
        .Times(AnyNumber());
    EXPECT_CALL(*session_.get(), config())
        .Times(AnyNumber());
    EXPECT_CALL(*session2_.get(), config())
        .Times(AnyNumber());

  }

  virtual void TearDown() {
  }

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index) {
    scoped_refptr<MockConnectionToClient> connection =
        (connection_index == 0) ? connection_ : connection2_;
    scoped_refptr<ClientSession> client = new ClientSession(host_.get(),
                                                            connection);
    connection->set_host_stub(client.get());

    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::AddClient,
                          client));
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::OnClientConnected,
                          connection));
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::LocalLoginSucceeded,
                          connection));
  }

  // Helper method to remove a client connection from ChromotingHost.
  void RemoveClientConnection() {
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::OnClientDisconnected,
                          connection_));
  }

 protected:
  MessageLoop message_loop_;
  MockConnectionToClientEventHandler handler_;
  scoped_refptr<ChromotingHost> host_;
  scoped_refptr<InMemoryHostConfig> config_;
  MockChromotingHostContext context_;
  scoped_refptr<MockConnectionToClient> connection_;
  scoped_refptr<MockSession> session_;
  scoped_ptr<SessionConfig> session_config_;
  MockVideoStub video_stub_;
  MockClientStub client_stub_;
  MockHostStub* host_stub_;
  MockInputStub* input_stub_;
  scoped_refptr<MockConnectionToClient> connection2_;
  scoped_refptr<MockSession> session2_;
  scoped_ptr<SessionConfig> session_config2_;
  MockVideoStub video_stub2_;
  MockClientStub client_stub2_;
  MockHostStub* host_stub2_;
  MockInputStub* input_stub2_;
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

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());

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
  EXPECT_CALL(*connection_.get(), Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Reconnect) {
  host_->Start(NewRunnableFunction(&PostQuitTask, &message_loop_));

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .Times(2)
      .WillRepeatedly(RunDoneTask());

  // When the video packet is received we first disconnect the mock
  // connection.
  {
    InSequence s;
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this,
                              &ChromotingHostTest::RemoveClientConnection),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }

  // If Disconnect() is called we can break the main message loop.
  EXPECT_CALL(*connection_.get(), Disconnect())
      .WillOnce(QuitMainMessageLoop(&message_loop_))
      .RetiresOnSaturation();

  SimulateClientConnection(0);
  message_loop_.Run();

  // Connect the client again.
  {
    InSequence s;
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(host_.get(), &ChromotingHost::Shutdown),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }
  EXPECT_CALL(*connection_.get(), Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, ConnectTwice) {
  host_->Start(NewRunnableFunction(&PostQuitTask, &message_loop_));

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .Times(1)
      .WillRepeatedly(RunDoneTask());

  EXPECT_CALL(client_stub2_, BeginSessionResponse(_, _))
      .Times(1)
      .WillRepeatedly(RunDoneTask());

  // When a video packet is received we connect the second mock
  // connection.
  {
    InSequence s;
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                CreateFunctor(
                    this, &ChromotingHostTest::SimulateClientConnection, 1)),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(host_.get(), &ChromotingHost::Shutdown),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(*connection_.get(), Disconnect())
      .RetiresOnSaturation();
  EXPECT_CALL(*connection2_.get(), Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0);
  message_loop_.Run();
}

}  // namespace remoting
