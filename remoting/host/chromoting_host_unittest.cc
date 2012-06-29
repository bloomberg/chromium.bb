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
#include "remoting/protocol/errors.h"
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
using testing::Expectation;
using testing::InSequence;
using testing::Invoke;
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

void NullTask() {
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
    event_executor_ = new MockEventExecutor();
    desktop_environment_ = DesktopEnvironment::CreateFake(
        &context_,
        capturer.Pass(),
        scoped_ptr<EventExecutor>(event_executor_));
    session_manager_ = new protocol::MockSessionManager();

    host_ = new ChromotingHost(
        &context_, &signal_strategy_, desktop_environment_.get(),
        scoped_ptr<protocol::SessionManager>(session_manager_));
    host_->AddStatusObserver(&host_status_observer_);

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
    session_unowned_.reset(new MockSession());
    session2_unowned_.reset(new MockSession());
    session_config_ = SessionConfig::GetDefault();
    session_jid_ = "user@domain/rest-of-jid";
    session_config2_ = SessionConfig::GetDefault();
    session2_jid_ = "user2@domain/rest-of-jid";
    session_unowned_config_ = SessionConfig::GetDefault();
    session_unowned_jid_ = "user3@doman/rest-of-jid";
    session2_unowned_config_ = SessionConfig::GetDefault();
    session2_unowned_jid_ = "user4@doman/rest-of-jid";

    EXPECT_CALL(*session_, jid())
        .WillRepeatedly(ReturnRef(session_jid_));
    EXPECT_CALL(*session2_, jid())
        .WillRepeatedly(ReturnRef(session2_jid_));
    EXPECT_CALL(*session_unowned_, jid())
        .WillRepeatedly(ReturnRef(session_unowned_jid_));
    EXPECT_CALL(*session2_unowned_, jid())
        .WillRepeatedly(ReturnRef(session2_unowned_jid_));
    EXPECT_CALL(*session_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_unowned_, SetStateChangeCallback(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            this, &ChromotingHostTest::SetSessionStateChangeCallback));
    EXPECT_CALL(*session2_unowned_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_unowned_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_unowned_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_, config())
        .WillRepeatedly(ReturnRef(session_config_));
    EXPECT_CALL(*session2_, config())
        .WillRepeatedly(ReturnRef(session_config2_));

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

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index, bool authenticate,
                                bool reject) {
    scoped_ptr<protocol::ConnectionToClient> connection =
        ((connection_index == 0) ? owned_connection_ : owned_connection2_).
        PassAs<protocol::ConnectionToClient>();
    protocol::ConnectionToClient* connection_ptr = connection.get();
    ClientSession* client = new ClientSession(
        host_.get(), connection.Pass(), desktop_environment_->event_executor(),
        desktop_environment_->capturer(), base::TimeDelta());
    connection_ptr->set_host_stub(client);

    context_.network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHostTest::AddClientToHost,
                              host_, client));

    if (authenticate) {
      context_.network_message_loop()->PostTask(
          FROM_HERE, base::Bind(&ClientSession::OnConnectionAuthenticated,
                                base::Unretained(client), connection_ptr));
      if (!reject) {
        context_.network_message_loop()->PostTask(
            FROM_HERE,
            base::Bind(&ClientSession::OnConnectionChannelsConnected,
                       base::Unretained(client), connection_ptr));
      }
    } else {
      context_.network_message_loop()->PostTask(
          FROM_HERE, base::Bind(&ClientSession::OnConnectionClosed,
                                base::Unretained(client), connection_ptr,
                                protocol::AUTHENTICATION_FAILED));
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

  // Notify |host_| that the authenticating client has been rejected.
  void RejectAuthenticatingClient() {
    host_->RejectAuthenticatingClient();
  }

  // Notify |host_| that |client_| has closed.
  void ClientSessionClosed() {
    host_->OnSessionClosed(client_);
  }

  // Notify |host_| that |client2_| has closed.
  void ClientSession2Closed() {
    host_->OnSessionClosed(client2_);
  }

  void SetSessionStateChangeCallback(
      const protocol::Session::StateChangeCallback& callback) {
    session_state_change_callback_ = callback;
  }

  void NotifyConnectionClosed() {
    if (!session_state_change_callback_.is_null()) {
      session_state_change_callback_.Run(protocol::Session::CLOSED);
    }
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

  void QuitMainMessageLoop() {
    PostQuitTask(&message_loop_);
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockConnectionToClientEventHandler handler_;
  MockSignalStrategy signal_strategy_;
  MockEventExecutor* event_executor_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface_;
  scoped_refptr<ChromotingHost> host_;
  MockHostStatusObserver host_status_observer_;
  MockChromotingHostContext context_;
  protocol::MockSessionManager* session_manager_;
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
  scoped_ptr<MockSession> session_unowned_;  // Not owned by a connection.
  SessionConfig session_unowned_config_;
  std::string session_unowned_jid_;
  scoped_ptr<MockSession> session2_unowned_;  // Not owned by a connection.
  SessionConfig session2_unowned_config_;
  std::string session2_unowned_jid_;
  protocol::Session::StateChangeCallback session_state_change_callback_;

  // Owned by |host_|.
  MockDisconnectWindow* disconnect_window_;
  MockContinueWindow* continue_window_;
  MockLocalInputMonitor* local_input_monitor_;
};

TEST_F(ChromotingHostTest, StartAndShutdown) {
  Expectation start = EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide()).After(start);
  EXPECT_CALL(*continue_window_, Hide()).After(start);
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(start);

  host_->Start();

  message_loop_.PostTask(
      FROM_HERE, base::Bind(
          &ChromotingHost::Shutdown, host_.get(),
          base::Bind(&PostQuitTask, &message_loop_)));
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Connect) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  host_->Start();

  // When the video packet is received we first shut down ChromotingHost,
  // then execute the done task.
  Expectation status_authenticated =
      EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid_));
  EXPECT_CALL(host_status_observer_, OnClientConnected(session_jid_))
      .After(status_authenticated);
  Expectation start =
      EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_))
      .After(status_authenticated);
  Expectation stop =
      EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .After(start)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .Times(AnyNumber())
      .After(stop)
      .WillRepeatedly(RunDoneTask());
  EXPECT_CALL(*connection_, Disconnect())
      .After(stop)
      .WillOnce(
          InvokeWithoutArgs(this, &ChromotingHostTest::ClientSessionClosed))
      .RetiresOnSaturation();
  Expectation status_disconnected =
      EXPECT_CALL(host_status_observer_, OnClientDisconnected(session_jid_))
      .After(stop);
  EXPECT_CALL(*event_executor_, OnSessionFinished())
      .After(stop);
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(status_disconnected);

  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, RejectAuthenticatingClient) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  host_->Start();

  {
    InSequence s;
    EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid_))
        .WillOnce(InvokeWithoutArgs(
        this, &ChromotingHostTest::RejectAuthenticatingClient));
    EXPECT_CALL(*connection_, Disconnect())
        .WillOnce(
            InvokeWithoutArgs(this, &ChromotingHostTest::ClientSessionClosed))
        .RetiresOnSaturation();
    EXPECT_CALL(host_status_observer_, OnClientDisconnected(session_jid_));
    EXPECT_CALL(*event_executor_, OnSessionFinished())
        .WillOnce(InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
    EXPECT_CALL(host_status_observer_, OnShutdown());
  }

  SimulateClientConnection(0, true, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, AuthenticationFailed) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  host_->Start();

  {
    InSequence s;
    EXPECT_CALL(host_status_observer_, OnAccessDenied(session_jid_))
        .WillOnce(InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
    EXPECT_CALL(host_status_observer_, OnShutdown());
  }

  SimulateClientConnection(0, false, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Reconnect) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  host_->Start();

  // When the video packet is received we first disconnect the mock
  // connection, then run the done task, then quit the message loop.
  Expectation status_authenticated1 =
      EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid_));
  EXPECT_CALL(host_status_observer_, OnClientConnected(session_jid_))
      .After(status_authenticated1);
  Expectation start1 =
      EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_))
      .After(status_authenticated1);
  Expectation stop1 = EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .After(start1)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ChromotingHostTest::RemoveClientSession),
          RunDoneTask(),
          InvokeWithoutArgs(this, &ChromotingHostTest::QuitMainMessageLoop)))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .Times(AnyNumber())
      .After(stop1)
      .WillRepeatedly(RunDoneTask());
  EXPECT_CALL(*event_executor_, OnSessionFinished())
      .After(stop1);
  EXPECT_CALL(host_status_observer_, OnClientDisconnected(session_jid_))
      .After(stop1);

  SimulateClientConnection(0, true, false);
  message_loop_.Run();

  Expectation status_authenticated2 =
      EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session2_jid_));
  EXPECT_CALL(host_status_observer_, OnClientConnected(session2_jid_))
      .After(status_authenticated2);
  Expectation start2 =
      EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_))
      .After(status_authenticated2);
  Expectation stop2 = EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
      .After(start2)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
      .Times(AnyNumber())
      .After(stop2)
      .WillRepeatedly(RunDoneTask());
  EXPECT_CALL(*connection2_, Disconnect())
      .After(stop2)
      .WillOnce(
          InvokeWithoutArgs(this, &ChromotingHostTest::ClientSession2Closed))
      .RetiresOnSaturation();
  EXPECT_CALL(*event_executor_, OnSessionFinished())
      .After(stop2);
  Expectation status_disconnected2 =
      EXPECT_CALL(host_status_observer_, OnClientDisconnected(session2_jid_))
      .After(stop2);
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(status_disconnected2);

  SimulateClientConnection(1, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, ConnectWhenAnotherClientIsConnected) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  host_->Start();

  // When a video packet is received we connect the second mock
  // connection.
  Expectation status_authenticated1 =
      EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid_));
  EXPECT_CALL(host_status_observer_, OnClientConnected(session_jid_))
      .After(status_authenticated1);
  Expectation start1 =
      EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_))
      .After(status_authenticated1);
  Expectation start2 = EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .After(start1)
      .WillOnce(DoAll(
          InvokeWithoutArgs(
              CreateFunctor(
                  this,
                  &ChromotingHostTest::SimulateClientConnection, 1, true,
                  false)),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .Times(AnyNumber())
      .After(start2)
      .WillRepeatedly(RunDoneTask());
  EXPECT_CALL(*event_executor_, OnSessionFinished()).After(start2);
  EXPECT_CALL(host_status_observer_, OnClientDisconnected(session_jid_))
      .After(start2);
  EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session2_jid_))
      .After(start2);
  EXPECT_CALL(host_status_observer_, OnClientConnected(session2_jid_))
      .After(start2);
  EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_)).After(start2);
  EXPECT_CALL(*connection_, Disconnect())
      .After(start2)
      .WillOnce(
          InvokeWithoutArgs(this, &ChromotingHostTest::ClientSessionClosed))
      .RetiresOnSaturation();
  Expectation stop2 = EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
      .After(start2)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost),
          RunDoneTask()))
      .RetiresOnSaturation();
  EXPECT_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
      .Times(AnyNumber())
      .After(stop2)
      .WillRepeatedly(RunDoneTask());
  Expectation status_disconnected2 =
      EXPECT_CALL(host_status_observer_, OnClientDisconnected(session2_jid_))
      .After(stop2);
  EXPECT_CALL(*event_executor_, OnSessionFinished()).After(stop2);
  EXPECT_CALL(*connection2_, Disconnect())
      .After(stop2)
      .WillOnce(
          InvokeWithoutArgs(this, &ChromotingHostTest::ClientSession2Closed))
      .RetiresOnSaturation();
  EXPECT_CALL(host_status_observer_, OnShutdown())
      .After(status_disconnected2);

  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, IncomingSessionDeclined) {
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::ACCEPT;
  host_->OnIncomingSession(session_, &response);
  EXPECT_EQ(protocol::SessionManager::DECLINE, response);
}

TEST_F(ChromotingHostTest, IncomingSessionIncompatible) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());
  EXPECT_CALL(*session_unowned_, candidate_config()).WillOnce(Return(
      protocol::CandidateSessionConfig::CreateEmpty().release()));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::ACCEPT;
  host_->OnIncomingSession(session_unowned_.get(), &response);
  EXPECT_EQ(protocol::SessionManager::INCOMPATIBLE, response);

  host_->Shutdown(base::Bind(&NullTask));
}

TEST_F(ChromotingHostTest, IncomingSessionAccepted) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());
  EXPECT_CALL(*session_unowned_, candidate_config()).WillOnce(Return(
      protocol::CandidateSessionConfig::CreateDefault().release()));
  EXPECT_CALL(*session_unowned_, set_config(_));
  EXPECT_CALL(*session_unowned_, Close()).WillOnce(InvokeWithoutArgs(
      this, &ChromotingHostTest::NotifyConnectionClosed));
  EXPECT_CALL(host_status_observer_, OnAccessDenied(_));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  host_->OnIncomingSession(session_unowned_.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  host_->Shutdown(base::Bind(&NullTask));
}

TEST_F(ChromotingHostTest, IncomingSessionOverload) {
  EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  EXPECT_CALL(*disconnect_window_, Hide());
  EXPECT_CALL(*continue_window_, Hide());
  EXPECT_CALL(*session_unowned_, candidate_config()).WillOnce(Return(
      protocol::CandidateSessionConfig::CreateDefault().release()));
  EXPECT_CALL(*session_unowned_, set_config(_));
  EXPECT_CALL(*session_unowned_, Close()).WillOnce(InvokeWithoutArgs(
      this, &ChromotingHostTest::NotifyConnectionClosed));
  EXPECT_CALL(host_status_observer_, OnAccessDenied(_));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  host_->OnIncomingSession(session_unowned_.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  host_->OnIncomingSession(session2_unowned_.get(), &response);
  EXPECT_EQ(protocol::SessionManager::OVERLOAD, response);

  host_->Shutdown(base::Bind(&NullTask));
}

}  // namespace remoting
