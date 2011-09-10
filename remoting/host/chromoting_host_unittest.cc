// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
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
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;

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

void DummyDoneTask() {
}

}  // namespace

class ChromotingHostTest : public testing::Test {
 public:
  ChromotingHostTest() {
  }

  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    config_ = new InMemoryHostConfig();
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

    Capturer* capturer = new CapturerFake();
    event_executor_ = new MockEventExecutor();
    curtain_ = new MockCurtain();
    disconnect_window_ = new MockDisconnectWindow();
    continue_window_ = new MockContinueWindow();
    local_input_monitor_ = new MockLocalInputMonitor();
    desktop_environment_.reset(
        new DesktopEnvironment(&context_, capturer, event_executor_, curtain_,
                               disconnect_window_, continue_window_,
                               local_input_monitor_));
    MockAccessVerifier* access_verifier = new MockAccessVerifier();

    host_ = ChromotingHost::Create(&context_, config_,
                                   desktop_environment_.get(),
                                   access_verifier, false);
    credentials_.set_type(protocol::PASSWORD);
    credentials_.set_username("user");
    credentials_.set_credential("password");
    connection_ = new MockConnectionToClient(
        &handler_, &host_stub_, event_executor_);
    connection2_ = new MockConnectionToClient(
        &handler_, &host_stub2_, &event_executor2_);
    session_.reset(new MockSession());
    session2_.reset(new MockSession());
    session_config_ = SessionConfig::GetDefault();
    session_jid_ = "user@domain/rest-of-jid";
    session_config2_ = SessionConfig::GetDefault();

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
        .WillByDefault(Return(session_.get()));
    ON_CALL(*connection2_.get(), video_stub())
        .WillByDefault(Return(&video_stub2_));
    ON_CALL(*connection2_.get(), client_stub())
        .WillByDefault(Return(&client_stub2_));
    ON_CALL(*connection2_.get(), session())
        .WillByDefault(Return(session2_.get()));
    ON_CALL(*session_.get(), config())
        .WillByDefault(ReturnRef(session_config_));
    ON_CALL(*session_, jid())
        .WillByDefault(ReturnRef(session_jid_));
    ON_CALL(*session2_.get(), config())
        .WillByDefault(ReturnRef(session_config2_));
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

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
  }

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index, bool authenticate) {
    scoped_refptr<MockConnectionToClient> connection =
        (connection_index == 0) ? connection_ : connection2_;

    MockUserAuthenticator *user_authenticator = new MockUserAuthenticator;
    EXPECT_CALL(*user_authenticator, Authenticate(_, _))
        .WillOnce(Return(authenticate));

    scoped_refptr<ClientSession> client = new ClientSession(
        host_.get(),
        user_authenticator,
        connection,
        event_executor_);
    connection->set_host_stub(client.get());

    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&ChromotingHostTest::AddClientToHost,
                            host_, client));
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(client.get(),
                          &ClientSession::BeginSessionRequest,
                          &credentials_,
                          NewRunnableFunction(&DummyDoneTask)));
  }

  // Helper method to remove a client connection from ChromotingHost.
  void RemoveClientConnection() {
    context_.network_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(host_.get(),
                          &ChromotingHost::OnClientDisconnected,
                          connection_));
  }

  static void AddClientToHost(scoped_refptr<ChromotingHost> host,
                              scoped_refptr<ClientSession> session) {
    host->clients_.push_back(session);
  }

  void ShutdownHost() {
    host_->Shutdown(
        NewRunnableFunction(&PostQuitTask, &message_loop_));
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockConnectionToClientEventHandler handler_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_refptr<ChromotingHost> host_;
  scoped_refptr<InMemoryHostConfig> config_;
  MockChromotingHostContext context_;
  protocol::LocalLoginCredentials credentials_;
  scoped_refptr<MockConnectionToClient> connection_;
  std::string session_jid_;
  scoped_ptr<MockSession> session_;
  SessionConfig session_config_;
  MockVideoStub video_stub_;
  MockClientStub client_stub_;
  MockHostStub host_stub_;
  scoped_refptr<MockConnectionToClient> connection2_;
  scoped_ptr<MockSession> session2_;
  SessionConfig session_config2_;
  MockVideoStub video_stub2_;
  MockClientStub client_stub2_;
  MockHostStub host_stub2_;
  MockEventExecutor event_executor2_;

  // Owned by |host_|.
  MockEventExecutor* event_executor_;
  MockCurtain* curtain_;
  MockDisconnectWindow* disconnect_window_;
  MockContinueWindow* continue_window_;
  MockLocalInputMonitor* local_input_monitor_;
};

TEST_F(ChromotingHostTest, DISABLED_StartAndShutdown) {
  host_->Start();

  message_loop_.PostTask(
      FROM_HERE,NewRunnableMethod(
          host_.get(), &ChromotingHost::Shutdown,
          NewRunnableFunction(&PostQuitTask, &message_loop_)));
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_Connect) {
  host_->Start();

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());

  // When the video packet is received we first shutdown ChromotingHost
  // then execute the done task.
  {
    InSequence s;
    EXPECT_CALL(*curtain_, EnableCurtainMode(true))
        .Times(1);
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*connection_.get(), Disconnect())
        .RetiresOnSaturation();
  }
  SimulateClientConnection(0, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_Reconnect) {
  host_->Start();

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .Times(2)
      .WillRepeatedly(RunDoneTask());

  // When the video packet is received we first disconnect the mock
  // connection.
  {
    InSequence s;
    // Ensure that curtain mode is activated before the first video packet.
    EXPECT_CALL(*curtain_, EnableCurtainMode(true))
        .Times(1);
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this,
                              &ChromotingHostTest::RemoveClientConnection),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*curtain_, EnableCurtainMode(false))
        .Times(1);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }

  // If Disconnect() is called we can break the main message loop.
  EXPECT_CALL(*connection_.get(), Disconnect())
      .WillOnce(QuitMainMessageLoop(&message_loop_))
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();

  // Connect the client again.
  {
    InSequence s;
    EXPECT_CALL(*curtain_, EnableCurtainMode(true))
        .Times(1);
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(*connection_.get(), Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DISABLED_ConnectTwice) {
  host_->Start();

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
    EXPECT_CALL(*curtain_, EnableCurtainMode(true))
        .Times(1)
        .WillOnce(QuitMainMessageLoop(&message_loop_));
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                CreateFunctor(
                    this,
                    &ChromotingHostTest::SimulateClientConnection, 1, true)),
            RunDoneTask()))
        .RetiresOnSaturation();
    // Check that the second connection does not affect curtain mode.
    EXPECT_CALL(*curtain_, EnableCurtainMode(_))
        .Times(0);
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(*connection_.get(), Disconnect())
      .RetiresOnSaturation();
  EXPECT_CALL(*connection2_.get(), Disconnect())
      .RetiresOnSaturation();

  SimulateClientConnection(0, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, CurtainModeFail) {
  host_->Start();

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());

  // Ensure that curtain mode is not activated if a connection does not
  // authenticate.
  EXPECT_CALL(*curtain_, EnableCurtainMode(_))
      .Times(0);
  EXPECT_CALL(*disconnect_window_, Show(_, _))
      .Times(0);
  EXPECT_CALL(*connection_.get(), Disconnect())
      .WillOnce(QuitMainMessageLoop(&message_loop_));
  SimulateClientConnection(0, false);
  RemoveClientConnection();
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, CurtainModeFailSecond) {
  host_->Start();

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());

  EXPECT_CALL(client_stub2_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());


  // When a video packet is received we connect the second mock
  // connection.
  {
    InSequence s;
    EXPECT_CALL(*curtain_, EnableCurtainMode(true))
        .WillOnce(QuitMainMessageLoop(&message_loop_));
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                CreateFunctor(
                    this,
                    &ChromotingHostTest::SimulateClientConnection, 1, false)),
            RunDoneTask()))
        .RetiresOnSaturation();
    // Check that the second connection does not affect curtain mode.
    EXPECT_CALL(*curtain_, EnableCurtainMode(_))
        .Times(0);
    EXPECT_CALL(*disconnect_window_, Show(_, _))
        .Times(0);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(video_stub2_, ProcessVideoPacket(_, _))
        .Times(0);
  }

  SimulateClientConnection(0, true);
  message_loop_.Run();
}

ACTION_P(SetBool, var) { *var = true; }

TEST_F(ChromotingHostTest, CurtainModeIT2Me) {
  host_->Start();
  host_->set_it2me(true);

  EXPECT_CALL(client_stub_, BeginSessionResponse(_, _))
      .WillOnce(RunDoneTask());

  // When the video packet is received we first shutdown ChromotingHost
  // then execute the done task.
  bool curtain_activated = false;
  {
    Sequence s1, s2;
    // Can't just expect Times(0) because if it fails then the host will
    // not be shut down and the message loop will never exit.
    EXPECT_CALL(*curtain_, EnableCurtainMode(_))
        .Times(AnyNumber())
        .WillRepeatedly(SetBool(&curtain_activated));
    EXPECT_CALL(*disconnect_window_, Show(_, "user@domain"))
        .Times(1)
        .InSequence(s1);
    EXPECT_CALL(*local_input_monitor_, Start(_))
        .Times(1)
        .InSequence(s2);
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .InSequence(s1, s2)
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub_, ProcessVideoPacket(_, _))
        .Times(AnyNumber())
        .InSequence(s1, s2);
    EXPECT_CALL(*connection_.get(), Disconnect())
        .InSequence(s1, s2)
        .RetiresOnSaturation();
  }
  SimulateClientConnection(0, true);
  message_loop_.Run();
  host_->set_it2me(false);
  EXPECT_THAT(curtain_activated, false);
}
}  // namespace remoting
