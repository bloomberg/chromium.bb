// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/session_config.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::remoting::protocol::MockClientStub;
using ::remoting::protocol::MockConnectionToClient;
using ::remoting::protocol::MockConnectionToClientEventHandler;
using ::remoting::protocol::MockHostStub;
using ::remoting::protocol::MockSession;
using ::remoting::protocol::MockVideoStub;
using ::remoting::protocol::SessionConfig;

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::CreateFunctor;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::InvokeArgument;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;

namespace remoting {

namespace {

void PostQuitTask(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// Run the task and delete it afterwards. This action is used to deal with
// done callbacks.
ACTION(RunDoneTask) {
  arg1.Run();
}

ACTION_P(QuitMainMessageLoop, message_loop) {
  PostQuitTask(message_loop);
}

void DummyDoneTask() {
}

}  // namespace

class ChromotingHostTest : public testing::Test {
 public:
  ChromotingHostTest() {
  }

  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    ON_CALL(context_, main_message_loop())
        .WillByDefault(Return(&message_loop_));
    ON_CALL(context_, encode_message_loop())
        .WillByDefault(Return(&message_loop_));
    ON_CALL(context_, network_message_loop())
        .WillByDefault(Return(message_loop_proxy_.get()));
    ON_CALL(context_, ui_message_loop())
        .WillByDefault(Return(message_loop_proxy_.get()));
    EXPECT_CALL(context_, main_message_loop())
        .Times(AnyNumber());
    EXPECT_CALL(context_, encode_message_loop())
        .Times(AnyNumber());
    EXPECT_CALL(context_, network_message_loop())
        .Times(AnyNumber());
    EXPECT_CALL(context_, ui_message_loop())
        .Times(AnyNumber());

    scoped_ptr<Capturer> capturer(new CapturerFake());
    scoped_ptr<EventExecutor> event_executor(new MockEventExecutor());
    desktop_environment_ = DesktopEnvironment::CreateFake(
        &context_,
        capturer.Pass(),
        event_executor.Pass());

    host_ = new ChromotingHost(
        &context_, &signal_strategy_, desktop_environment_.get(),
        NetworkSettings());

    disconnect_window_ = new MockDisconnectWindow();
    continue_window_ = new MockContinueWindow();
    local_input_monitor_ = new MockLocalInputMonitor();
    it2me_host_user_interface_.reset(new It2MeHostUserInterface(&context_));
    it2me_host_user_interface_->StartForTest(
        host_,
        base::Bind(&ChromotingHost::Shutdown, host_, base::Closure()),
        scoped_ptr<DisconnectWindow>(disconnect_window_),
        scoped_ptr<ContinueWindow>(continue_window_),
        scoped_ptr<LocalInputMonitor>(local_input_monitor_));

    session_ = new MockSession();
    session2_ = new MockSession();
    session_config_ = SessionConfig::GetDefault();
    session_jid_ = "user@domain/rest-of-jid";
    session_config2_ = SessionConfig::GetDefault();
    session2_jid_ = "user2@domain/rest-of-jid";
    EXPECT_CALL(*session_, jid())
        .WillRepeatedly(ReturnRef(session_jid_));
    EXPECT_CALL(*session2_, jid())
        .WillRepeatedly(ReturnRef(session2_jid_));
    EXPECT_CALL(*session_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_, config())
        .WillRepeatedly(ReturnRef(session_config_));
    EXPECT_CALL(*session2_, config())
        .WillRepeatedly(ReturnRef(session_config2_));
    EXPECT_CALL(*session_, Close())
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, Close())
        .Times(AnyNumber());

    owned_connection_.reset(new MockConnectionToClient(
        session_, &host_stub_, desktop_environment_->event_executor()));
    connection_ = owned_connection_.get();
    owned_connection2_.reset(new MockConnectionToClient(
        session2_, &host_stub2_, desktop_environment_->event_executor()));
    connection2_ = owned_connection2_.get();

    ON_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .WillByDefault(DeleteArg<0>());
    ON_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
        .WillByDefault(DeleteArg<0>());
    ON_CALL(*connection_, video_stub())
        .WillByDefault(Return(&video_stub_));
    ON_CALL(*connection_, client_stub())
        .WillByDefault(Return(&client_stub_));
    ON_CALL(*connection_, session())
        .WillByDefault(Return(session_));
    ON_CALL(*connection2_, video_stub())
        .WillByDefault(Return(&video_stub2_));
    ON_CALL(*connection2_, client_stub())
        .WillByDefault(Return(&client_stub2_));
    ON_CALL(*connection2_, session())
        .WillByDefault(Return(session2_));
    EXPECT_CALL(*connection_, video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection_, client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection_, session())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, session())
        .Times(AnyNumber());
  }

  virtual void TearDown() OVERRIDE {
    owned_connection_.reset();
    owned_connection2_.reset();
    host_ = NULL;
    // Run message loop before destroying because protocol::Session is
    // destroyed asynchronously.
    message_loop_.RunAllPending();
  }

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index, bool authenticate) {
    scoped_ptr<protocol::ConnectionToClient> connection =
        ((connection_index == 0) ? owned_connection_ : owned_connection2_).
        PassAs<protocol::ConnectionToClient>();
    protocol::ConnectionToClient* connection_ptr = connection.get();
    ClientSession* client = new ClientSession(
        host_.get(), connection.Pass(), desktop_environment_->event_executor(),
        desktop_environment_->capturer());
    connection->set_host_stub(client);

    context_.network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHostTest::AddClientToHost,
                              host_, client));
    if (authenticate) {
      context_.network_message_loop()->PostTask(
          FROM_HERE, base::Bind(&ClientSession::OnConnectionAuthenticated,
                                base::Unretained(client), connection_ptr));
      context_.network_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&ClientSession::OnConnectionChannelsConnected,
                     base::Unretained(client), connection_ptr));
    }

    if (connection_index == 0) {
      client_ = client;
    } else {
      client2_ = client;
    }
  }

  // Helper method to remove a client connection from ChromotingHost.
  void RemoveClientSession() {
    client_->OnConnectionClosed(connection_, protocol::OK);
  }

  static void AddClientToHost(scoped_refptr<ChromotingHost> host,
                              ClientSession* session) {
    host->clients_.push_back(session);
  }

  void ShutdownHost() {
    message_loop_.PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Shutdown, host_,
                              base::Bind(&PostQuitTask, &message_loop_)));
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockConnectionToClientEventHandler handler_;
  MockSignalStrategy signal_strategy_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface_;
  scoped_refptr<ChromotingHost> host_;
  MockChromotingHostContext context_;
  MockConnectionToClient* connection_;
  scoped_ptr<MockConnectionToClient> owned_connection_;
  ClientSession* client_;
  std::string session_jid_;
  MockSession* session_;  // Owned by |connection_|.
  SessionConfig session_config_;
  MockVideoStub video_stub_;
  MockClientStub client_stub_;
  MockHostStub host_stub_;
  MockConnectionToClient* connection2_;
  scoped_ptr<MockConnectionToClient> owned_connection2_;
  ClientSession* client2_;
  std::string session2_jid_;
  MockSession* session2_;  // Owned by |connection2_|.
  SessionConfig session_config2_;
  MockVideoStub video_stub2_;
  MockClientStub client_stub2_;
  MockHostStub host_stub2_;

  // Owned by |host_|.
  MockDisconnectWindow* disconnect_window_;
  MockContinueWindow* continue_window_;
  MockLocalInputMonitor* local_input_monitor_;
};

TEST_F(ChromotingHostTest, DISABLED_StartAndShutdown) {
  host_->Start();

  message_loop_.PostTask(
      FROM_HERE, base::Bind(
          &ChromotingHost::Shutdown, host_.get(),
          base::Bind(&PostQuitTask, &message_loop_)));
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_Connect) {
  host_->Start();

  // When the video packet is received we first shutdown ChromotingHost
  // then execute the done task.
  {
    InSequence s;
    EXPECT_CALL(*disconnect_window_, Show(_, _, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*connection_, Disconnect())
        .RetiresOnSaturation();
  }
  SimulateClientConnection(0, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_Reconnect) {
  host_->Start();

  // When the video packet is received we first disconnect the mock
  // connection.
  {
    InSequence s;
    EXPECT_CALL(*disconnect_window_, Show(_, _, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::RemoveClientSession),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
  }

  // If Disconnect() is called we can break the main message loop.
  EXPECT_CALL(*connection_, Disconnect())
      .WillOnce(QuitMainMessageLoop(&message_loop_))
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();

  // Connect the client again.
  {
    InSequence s;
    EXPECT_CALL(*disconnect_window_, Show(_, _, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(*connection_, Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_ConnectTwice) {
  host_->Start();

  // When a video packet is received we connect the second mock
  // connection.
  {
    InSequence s;
    EXPECT_CALL(*disconnect_window_, Show(_, _, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                CreateFunctor(
                    this,
                    &ChromotingHostTest::SimulateClientConnection, 1, true)),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(*disconnect_window_, Show(_, _, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(*connection_, Disconnect())
      .RetiresOnSaturation();
  EXPECT_CALL(*connection2_, Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();
}

}  // namespace remoting
