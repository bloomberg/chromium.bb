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

void DoNothing() {
}

}  // namespace

class ChromotingHostTest : public testing::Test {
 public:
  ChromotingHostTest() {
  }

  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();

    EXPECT_CALL(context_, ui_task_runner())
        .Times(AnyNumber())
        .WillRepeatedly(Return(message_loop_proxy_.get()));
    EXPECT_CALL(context_, capture_task_runner())
        .Times(AnyNumber())
        .WillRepeatedly(Return(message_loop_proxy_.get()));
    EXPECT_CALL(context_, encode_task_runner())
        .Times(AnyNumber())
        .WillRepeatedly(Return(message_loop_proxy_.get()));
    EXPECT_CALL(context_, network_task_runner())
        .Times(AnyNumber())
        .WillRepeatedly(Return(message_loop_proxy_.get()));

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

    session1_ = new MockSession();
    session2_ = new MockSession();
    session_unowned1_.reset(new MockSession());
    session_unowned2_.reset(new MockSession());
    session_config1_ = SessionConfig::GetDefault();
    session_jid1_ = "user@domain/rest-of-jid";
    session_config2_ = SessionConfig::GetDefault();
    session_jid2_ = "user2@domain/rest-of-jid";
    session_unowned_config1_ = SessionConfig::GetDefault();
    session_unowned_jid1_ = "user3@doman/rest-of-jid";
    session_unowned_config2_ = SessionConfig::GetDefault();
    session_unowned_jid2_ = "user4@doman/rest-of-jid";

    EXPECT_CALL(*session1_, jid())
        .WillRepeatedly(ReturnRef(session_jid1_));
    EXPECT_CALL(*session2_, jid())
        .WillRepeatedly(ReturnRef(session_jid2_));
    EXPECT_CALL(*session_unowned1_, jid())
        .WillRepeatedly(ReturnRef(session_unowned_jid1_));
    EXPECT_CALL(*session_unowned2_, jid())
        .WillRepeatedly(ReturnRef(session_unowned_jid2_));
    EXPECT_CALL(*session1_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_unowned1_, SetStateChangeCallback(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            this, &ChromotingHostTest::SetSessionStateChangeCallback));
    EXPECT_CALL(*session_unowned2_, SetStateChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session1_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session2_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_unowned1_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session_unowned2_, SetRouteChangeCallback(_))
        .Times(AnyNumber());
    EXPECT_CALL(*session1_, config())
        .WillRepeatedly(ReturnRef(session_config1_));
    EXPECT_CALL(*session2_, config())
        .WillRepeatedly(ReturnRef(session_config2_));

    owned_connection1_.reset(new MockConnectionToClient(
        session1_, &host_stub1_, desktop_environment_->event_executor()));
    connection1_ = owned_connection1_.get();
    owned_connection2_.reset(new MockConnectionToClient(
        session2_, &host_stub2_, desktop_environment_->event_executor()));
    connection2_ = owned_connection2_.get();

    ON_CALL(video_stub1_, ProcessVideoPacketPtr(_, _))
        .WillByDefault(DeleteArg<0>());
    ON_CALL(video_stub2_, ProcessVideoPacketPtr(_, _))
        .WillByDefault(DeleteArg<0>());
    ON_CALL(*connection1_, video_stub())
        .WillByDefault(Return(&video_stub1_));
    ON_CALL(*connection1_, client_stub())
        .WillByDefault(Return(&client_stub1_));
    ON_CALL(*connection1_, session())
        .WillByDefault(Return(session1_));
    ON_CALL(*connection2_, video_stub())
        .WillByDefault(Return(&video_stub2_));
    ON_CALL(*connection2_, client_stub())
        .WillByDefault(Return(&client_stub2_));
    ON_CALL(*connection2_, session())
        .WillByDefault(Return(session2_));
    EXPECT_CALL(*connection1_, video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection1_, client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection1_, session())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, video_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, client_stub())
        .Times(AnyNumber());
    EXPECT_CALL(*connection2_, session())
        .Times(AnyNumber());

    empty_candidate_config_ =
        protocol::CandidateSessionConfig::CreateEmpty();
    default_candidate_config_ =
        protocol::CandidateSessionConfig::CreateDefault();
  }

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index, bool authenticate,
                                bool reject) {
    scoped_ptr<protocol::ConnectionToClient> connection =
        ((connection_index == 0) ? owned_connection1_ : owned_connection2_).
        PassAs<protocol::ConnectionToClient>();
    protocol::ConnectionToClient* connection_ptr = connection.get();
    ClientSession* client = new ClientSession(
        host_.get(), connection.Pass(), desktop_environment_->event_executor(),
        desktop_environment_->capturer(), base::TimeDelta());
    connection_ptr->set_host_stub(client);

    context_.network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHostTest::AddClientToHost,
                              host_, client));

    if (authenticate) {
      context_.network_task_runner()->PostTask(
          FROM_HERE, base::Bind(&ClientSession::OnConnectionAuthenticated,
                                base::Unretained(client), connection_ptr));
      if (!reject) {
        context_.network_task_runner()->PostTask(
            FROM_HERE,
            base::Bind(&ClientSession::OnConnectionChannelsConnected,
                       base::Unretained(client), connection_ptr));
      }
    } else {
      context_.network_task_runner()->PostTask(
          FROM_HERE, base::Bind(&ClientSession::OnConnectionClosed,
                                base::Unretained(client), connection_ptr,
                                protocol::AUTHENTICATION_FAILED));
    }

    get_client(connection_index) = client;
  }

  // Change the session route for |client1_|.
  void ChangeSessionRoute(const std::string& channel_name,
                          const protocol::TransportRoute& route) {
    host_->OnSessionRouteChange(get_client(0), channel_name, route);
  }

  void DisconnectAllClients() {
    host_->DisconnectAllClients();
  }

  // Helper method to disconnect client 1 from the host.
  void DisconnectClient1() {
    client1_->OnConnectionClosed(connection1_, protocol::OK);
  }

  // Notify |host_| that the authenticating client has been rejected.
  void RejectAuthenticatingClient() {
    host_->RejectAuthenticatingClient();
  }

  // Notify |host_| that a client session has closed.
  void NotifyClientSessionClosed(int connection_index) {
    host_->OnSessionClosed(get_client(connection_index));
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

  // Expect the host to start.
  void ExpectHostStart() {
    EXPECT_CALL(*disconnect_window_, Hide());
    EXPECT_CALL(*continue_window_, Hide());
  }

  // Expect the host and session manager to start, and return the expectation
  // that the session manager has started.
  Expectation ExpectHostAndSessionManagerStart() {
    ExpectHostStart();
    return EXPECT_CALL(*session_manager_, Init(_, host_.get()));
  }

  // Expect a client to connect.
  // Return an expectation that a session has started, and that the first
  // video packet has been sent to the client.
  // Do |action| when that happens.
  template <class A>
  Expectation ExpectClientConnected(int connection_index, A action) {
    const std::string& session_jid = get_session_jid(connection_index);
    MockVideoStub& video_stub = get_video_stub(connection_index);

    Expectation client_authenticated =
        EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid));
    EXPECT_CALL(host_status_observer_, OnClientConnected(session_jid))
        .After(client_authenticated);
    Expectation session_started =
        EXPECT_CALL(*event_executor_, OnSessionStartedPtr(_))
        .After(client_authenticated);
    Expectation video_packet_sent =
        EXPECT_CALL(video_stub, ProcessVideoPacketPtr(_, _))
        .After(session_started)
        .WillOnce(DoAll(
            action,
            RunDoneTask()))
        .RetiresOnSaturation();
    EXPECT_CALL(video_stub, ProcessVideoPacketPtr(_, _))
        .Times(AnyNumber())
        .After(video_packet_sent)
        .WillRepeatedly(RunDoneTask());
    return video_packet_sent;
  }

  // Return an expectation that a client will disconnect after a given
  // expectation. The given action will be done after the event executor is
  // notified that the session has finished.
  template <class A>
  Expectation ExpectClientDisconnected(int connection_index,
                                       bool expect_host_status_change,
                                       Expectation after,
                                       A action) {
    MockConnectionToClient* connection = get_connection(connection_index);

    Expectation client_disconnected =
        EXPECT_CALL(*connection, Disconnect())
            .After(after)
            .WillOnce(InvokeWithoutArgs(CreateFunctor(
                this, &ChromotingHostTest::NotifyClientSessionClosed,
                connection_index)))
            .RetiresOnSaturation();
    ExpectClientDisconnectEffects(connection_index,
                                  expect_host_status_change,
                                  after,
                                  action);
    return client_disconnected;
  }

  // Expect the side-effects of a client disconnection, after a given
  // expectation. The given action will be done after the event executor is
  // notifed that the session has finished.
  template <class A>
  void ExpectClientDisconnectEffects(int connection_index,
                                     bool expect_host_status_change,
                                     Expectation after,
                                     A action) {
    const std::string& session_jid = get_session_jid(connection_index);

    EXPECT_CALL(*event_executor_, OnSessionFinished())
        .After(after)
        .WillOnce(action);
    if (expect_host_status_change) {
      EXPECT_CALL(host_status_observer_, OnClientDisconnected(session_jid))
          .After(after)
          .RetiresOnSaturation();
    }
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
  MockConnectionToClient* connection1_;
  scoped_ptr<MockConnectionToClient> owned_connection1_;
  ClientSession* client1_;
  std::string session_jid1_;
  MockSession* session1_;  // Owned by |connection_|.
  SessionConfig session_config1_;
  MockVideoStub video_stub1_;
  MockClientStub client_stub1_;
  MockHostStub host_stub1_;
  MockConnectionToClient* connection2_;
  scoped_ptr<MockConnectionToClient> owned_connection2_;
  ClientSession* client2_;
  std::string session_jid2_;
  MockSession* session2_;  // Owned by |connection2_|.
  SessionConfig session_config2_;
  MockVideoStub video_stub2_;
  MockClientStub client_stub2_;
  MockHostStub host_stub2_;
  scoped_ptr<MockSession> session_unowned1_;  // Not owned by a connection.
  SessionConfig session_unowned_config1_;
  std::string session_unowned_jid1_;
  scoped_ptr<MockSession> session_unowned2_;  // Not owned by a connection.
  SessionConfig session_unowned_config2_;
  std::string session_unowned_jid2_;
  protocol::Session::StateChangeCallback session_state_change_callback_;
  scoped_ptr<protocol::CandidateSessionConfig> empty_candidate_config_;
  scoped_ptr<protocol::CandidateSessionConfig> default_candidate_config_;

  // Owned by |host_|.
  MockDisconnectWindow* disconnect_window_;
  MockContinueWindow* continue_window_;
  MockLocalInputMonitor* local_input_monitor_;

  MockConnectionToClient*& get_connection(int connection_index) {
    return (connection_index == 0) ? connection1_ : connection2_;
  }

  ClientSession*& get_client(int connection_index) {
    return (connection_index == 0) ? client1_ : client2_;
  }

  const std::string& get_session_jid(int connection_index) {
    return (connection_index == 0) ? session_jid1_ : session_jid2_;
  }

  MockVideoStub& get_video_stub(int connection_index) {
    return (connection_index == 0) ? video_stub1_ : video_stub2_;
  }
};

TEST_F(ChromotingHostTest, StartAndShutdown) {
  Expectation start = ExpectHostAndSessionManagerStart();
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(start);

  host_->Start();
  message_loop_.PostTask(
      FROM_HERE, base::Bind(
          &ChromotingHost::Shutdown, host_.get(),
          base::Bind(&PostQuitTask, &message_loop_)));
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Connect) {
  ExpectHostAndSessionManagerStart();

  // Shut down the host when the first video packet is received.
  Expectation video_packet_sent = ExpectClientConnected(
      0, InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  Expectation client_disconnected = ExpectClientDisconnected(
      0, true, video_packet_sent, InvokeWithoutArgs(DoNothing));
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(client_disconnected);

  host_->Start();
  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, RejectAuthenticatingClient) {
  Expectation start = ExpectHostAndSessionManagerStart();
  EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid1_))
      .WillOnce(InvokeWithoutArgs(
      this, &ChromotingHostTest::RejectAuthenticatingClient));
  ExpectClientDisconnected(
      0, true, start,
      InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->Start();
  SimulateClientConnection(0, true, true);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, AuthenticationFailed) {
  ExpectHostAndSessionManagerStart();
  EXPECT_CALL(host_status_observer_, OnAccessDenied(session_jid1_))
      .WillOnce(InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->Start();
  SimulateClientConnection(0, false, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, Reconnect) {
  ExpectHostAndSessionManagerStart();

  // When a video packet is received on the first connection, disconnect it,
  // then quit the message loop.
  Expectation video_packet_sent1 = ExpectClientConnected(0, DoAll(
      InvokeWithoutArgs(this, &ChromotingHostTest::DisconnectClient1),
      InvokeWithoutArgs(this, &ChromotingHostTest::QuitMainMessageLoop)));
  ExpectClientDisconnectEffects(
      0, true, video_packet_sent1, InvokeWithoutArgs(DoNothing));

  // When a video packet is received on the second connection, shut down the
  // host.
  Expectation video_packet_sent2 = ExpectClientConnected(
      1, InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  Expectation client_disconnected2 = ExpectClientDisconnected(
      1, true, video_packet_sent2, InvokeWithoutArgs(DoNothing));
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(client_disconnected2);

  host_->Start();
  SimulateClientConnection(0, true, false);
  message_loop_.Run();
  SimulateClientConnection(1, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, ConnectWhenAnotherClientIsConnected) {
  ExpectHostAndSessionManagerStart();

  // When a video packet is received, connect the second connection.
  // This should disconnect the first connection.
  Expectation video_packet_sent1 = ExpectClientConnected(
      0,
      InvokeWithoutArgs(
          CreateFunctor(
              this,
              &ChromotingHostTest::SimulateClientConnection, 1, true, false)));
  ExpectClientDisconnected(
      0, true, video_packet_sent1, InvokeWithoutArgs(DoNothing));
  Expectation video_packet_sent2 = ExpectClientConnected(
      1, InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  Expectation client_disconnected2 = ExpectClientDisconnected(
      1, true, video_packet_sent2, InvokeWithoutArgs(DoNothing));
  EXPECT_CALL(host_status_observer_, OnShutdown()).After(client_disconnected2);

  host_->Start();
  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, IncomingSessionDeclined) {
  ExpectHostStart();
  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::ACCEPT;
  host_->OnIncomingSession(session1_, &response);
  EXPECT_EQ(protocol::SessionManager::DECLINE, response);
}

TEST_F(ChromotingHostTest, IncomingSessionIncompatible) {
  ExpectHostAndSessionManagerStart();
  EXPECT_CALL(*session_unowned1_, candidate_config()).WillOnce(Return(
      empty_candidate_config_.get()));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::ACCEPT;
  host_->OnIncomingSession(session_unowned1_.get(), &response);
  EXPECT_EQ(protocol::SessionManager::INCOMPATIBLE, response);

  host_->Shutdown(base::Bind(&DoNothing));
}

TEST_F(ChromotingHostTest, IncomingSessionAccepted) {
  ExpectHostAndSessionManagerStart();
  EXPECT_CALL(*session_unowned1_, candidate_config()).WillOnce(Return(
      default_candidate_config_.get()));
  EXPECT_CALL(*session_unowned1_, set_config(_));
  EXPECT_CALL(*session_unowned1_, Close()).WillOnce(InvokeWithoutArgs(
      this, &ChromotingHostTest::NotifyConnectionClosed));
  EXPECT_CALL(host_status_observer_, OnAccessDenied(_));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  host_->OnIncomingSession(session_unowned1_.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  host_->Shutdown(base::Bind(&DoNothing));
}

TEST_F(ChromotingHostTest, IncomingSessionOverload) {
  ExpectHostAndSessionManagerStart();
  EXPECT_CALL(*session_unowned1_, candidate_config()).WillOnce(Return(
      default_candidate_config_.get()));
  EXPECT_CALL(*session_unowned1_, set_config(_));
  EXPECT_CALL(*session_unowned1_, Close()).WillOnce(InvokeWithoutArgs(
      this, &ChromotingHostTest::NotifyConnectionClosed));
  EXPECT_CALL(host_status_observer_, OnAccessDenied(_));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->set_protocol_config(
      protocol::CandidateSessionConfig::CreateDefault().release());
  host_->Start();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  host_->OnIncomingSession(session_unowned1_.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  host_->OnIncomingSession(session_unowned2_.get(), &response);
  EXPECT_EQ(protocol::SessionManager::OVERLOAD, response);

  host_->Shutdown(base::Bind(&DoNothing));
}

TEST_F(ChromotingHostTest, OnSessionRouteChange) {
  std::string channel_name("ChannelName");
  protocol::TransportRoute route;

  ExpectHostAndSessionManagerStart();
  Expectation video_packet_sent = ExpectClientConnected(
      0, InvokeWithoutArgs(CreateFunctor(
          this, &ChromotingHostTest::ChangeSessionRoute, channel_name, route)));
  Expectation route_change =
      EXPECT_CALL(host_status_observer_, OnClientRouteChange(
          session_jid1_, channel_name, _))
      .After(video_packet_sent)
      .WillOnce(InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  ExpectClientDisconnected(0, true, route_change, InvokeWithoutArgs(DoNothing));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->Start();
  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

TEST_F(ChromotingHostTest, DisconnectAllClients) {
  ExpectHostAndSessionManagerStart();
  Expectation video_packet_sent = ExpectClientConnected(
      0, InvokeWithoutArgs(this, &ChromotingHostTest::DisconnectAllClients));
  ExpectClientDisconnected(0, true, video_packet_sent,
      InvokeWithoutArgs(this, &ChromotingHostTest::ShutdownHost));
  EXPECT_CALL(host_status_observer_, OnShutdown());

  host_->Start();
  SimulateClientConnection(0, true, false);
  message_loop_.Run();
}

}  // namespace remoting
