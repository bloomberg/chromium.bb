// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::WithArg;

namespace remoting {
namespace protocol {

namespace {

const char kHostJid[] = "host1@gmail.com/123";
const char kClientJid[] = "host2@gmail.com/321";

class MockSessionManagerListener {
 public:
  MOCK_METHOD2(OnIncomingSession,
               void(Session*,
                    SessionManager::IncomingSessionResponse*));
};

class MockSessionEventHandler : public Session::EventHandler {
 public:
  MOCK_METHOD1(OnSessionStateChange, void(Session::State));
  MOCK_METHOD2(OnSessionRouteChange, void(const std::string& channel_name,
                                          const TransportRoute& route));
};

class MockTransport : public Transport {
 public:
  MOCK_METHOD2(Start,
               void(Authenticator* authenticator,
                    SendTransportInfoCallback send_transport_info_callback));
  MOCK_METHOD1(ProcessTransportInfo, bool(buzz::XmlElement* transport_info));
};

}  // namespace

class JingleSessionTest : public testing::Test {
 public:
  JingleSessionTest() {
    message_loop_.reset(new base::MessageLoopForIO());
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  // Helper method that handles OnIncomingSession().
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_.reset(session);
    host_session_->SetEventHandler(&host_session_event_handler_);
    host_session_->SetTransport(&host_transport_);
  }

  void DeleteSession() {
    host_session_.reset();
  }

 protected:
  void TearDown() override {
    CloseSessions();
    CloseSessionManager();
    base::RunLoop().RunUntilIdle();
  }

  void CloseSessions() {
    host_session_.reset();
    client_session_.reset();
  }

  void CreateSessionManagers(int auth_round_trips, int messages_till_start,
                             FakeAuthenticator::Action auth_action) {
    host_signal_strategy_.reset(new FakeSignalStrategy(kHostJid));
    client_signal_strategy_.reset(new FakeSignalStrategy(kClientJid));
    FakeSignalStrategy::Connect(host_signal_strategy_.get(),
                                client_signal_strategy_.get());

    host_server_.reset(new JingleSessionManager(host_signal_strategy_.get()));
    host_server_->AcceptIncoming(
        base::Bind(&MockSessionManagerListener::OnIncomingSession,
                   base::Unretained(&host_server_listener_)));

    scoped_ptr<AuthenticatorFactory> factory(
        new FakeHostAuthenticatorFactory(auth_round_trips,
          messages_till_start, auth_action, true));
    host_server_->set_authenticator_factory(std::move(factory));

    client_server_.reset(
        new JingleSessionManager(client_signal_strategy_.get()));
  }

  void CreateSessionManagers(int auth_round_trips,
                             FakeAuthenticator::Action auth_action) {
    CreateSessionManagers(auth_round_trips, 0, auth_action);
  }

  void CloseSessionManager() {
    host_server_.reset();
    client_server_.reset();
    host_signal_strategy_.reset();
    client_signal_strategy_.reset();
  }

  void InitiateConnection(int auth_round_trips,
                          FakeAuthenticator::Action auth_action,
                          bool expect_fail) {
    EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
        .WillOnce(DoAll(
            WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
            SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

    {
      InSequence dummy;

      EXPECT_CALL(host_session_event_handler_,
                  OnSessionStateChange(Session::ACCEPTED))
          .Times(AtMost(1));
      EXPECT_CALL(host_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATING))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::FAILED))
            .Times(1);
      } else {
        EXPECT_CALL(host_transport_, Start(_, _)).Times(1);
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::AUTHENTICATED))
            .Times(1);

        // Expect that the connection will be closed eventually.
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::CLOSED))
            .Times(AtMost(1));
      }
    }

    {
      InSequence dummy;

      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::ACCEPTED))
          .Times(AtMost(1));
      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATING))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(client_session_event_handler_,
                    OnSessionStateChange(Session::FAILED))
            .Times(1);
      } else {
        EXPECT_CALL(client_transport_, Start(_, _)).Times(1);
        EXPECT_CALL(client_session_event_handler_,
                    OnSessionStateChange(Session::AUTHENTICATED))
            .Times(1);

        // Expect that the connection will be closed eventually.
        EXPECT_CALL(client_session_event_handler_,
                    OnSessionStateChange(Session::CLOSED))
            .Times(AtMost(1));
      }
    }

    scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
        FakeAuthenticator::CLIENT, auth_round_trips, auth_action, true));

    client_session_ =
        client_server_->Connect(kHostJid, std::move(authenticator));
    client_session_->SetEventHandler(&client_session_event_handler_);
    client_session_->SetTransport(&client_transport_);

    base::RunLoop().RunUntilIdle();
  }

  void ExpectRouteChange(const std::string& channel_name) {
    EXPECT_CALL(host_session_event_handler_,
                OnSessionRouteChange(channel_name, _))
        .Times(AtLeast(1));
    EXPECT_CALL(client_session_event_handler_,
                OnSessionRouteChange(channel_name, _))
        .Times(AtLeast(1));
  }

  scoped_ptr<base::MessageLoopForIO> message_loop_;

  NetworkSettings network_settings_;

  scoped_ptr<FakeSignalStrategy> host_signal_strategy_;
  scoped_ptr<FakeSignalStrategy> client_signal_strategy_;

  scoped_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  scoped_ptr<JingleSessionManager> client_server_;

  scoped_ptr<Session> host_session_;
  MockSessionEventHandler host_session_event_handler_;
  MockTransport host_transport_;
  scoped_ptr<Session> client_session_;
  MockSessionEventHandler client_session_event_handler_;
  MockTransport client_transport_;
};


// Verify that we can create and destroy session managers without a
// connection.
TEST_F(JingleSessionTest, CreateAndDestoy) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
}

// Verify that an incoming session can be rejected, and that the
// status of the connection is set to FAILED in this case.
TEST_F(JingleSessionTest, RejectConnection) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);

  // Reject incoming session.
  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(SetArgumentPointee<1>(protocol::SessionManager::DECLINE));

  {
    InSequence dummy;
    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::FAILED))
        .Times(1);
  }

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 1, FakeAuthenticator::ACCEPT, true));
  client_session_ = client_server_->Connect(kHostJid, std::move(authenticator));
  client_session_->SetEventHandler(&client_session_event_handler_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can connect two endpoints with single-step authentication.
TEST_F(JingleSessionTest, Connect) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  InitiateConnection(1, FakeAuthenticator::ACCEPT, false);

  // Verify that the client specified correct initiator value.
  ASSERT_GT(host_signal_strategy_->received_messages().size(), 0U);
  const buzz::XmlElement* initiate_xml =
      host_signal_strategy_->received_messages().front();
  const buzz::XmlElement* jingle_element =
      initiate_xml->FirstNamed(buzz::QName("urn:xmpp:jingle:1", "jingle"));
  ASSERT_TRUE(jingle_element);
  ASSERT_EQ(kClientJid,
            jingle_element->Attr(buzz::QName(std::string(), "initiator")));
}

// Verify that we can connect two endpoints with multi-step authentication.
TEST_F(JingleSessionTest, ConnectWithMultistep) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);
  InitiateConnection(3, FakeAuthenticator::ACCEPT, false);
}

// Verify that connection is terminated when single-step auth fails.
TEST_F(JingleSessionTest, ConnectWithBadAuth) {
  CreateSessionManagers(1, FakeAuthenticator::REJECT);
  InitiateConnection(1, FakeAuthenticator::ACCEPT, true);
}

// Verify that connection is terminated when multi-step auth fails.
TEST_F(JingleSessionTest, ConnectWithBadMultistepAuth) {
  CreateSessionManagers(3, FakeAuthenticator::REJECT);
  InitiateConnection(3, FakeAuthenticator::ACCEPT, true);
}

// Verify that incompatible protocol configuration is handled properly.
TEST_F(JingleSessionTest, TestIncompatibleProtocol) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _)).Times(0);

  EXPECT_CALL(client_session_event_handler_,
              OnSessionStateChange(Session::FAILED))
      .Times(1);

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 1, FakeAuthenticator::ACCEPT, true));

  scoped_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  // Disable all video codecs so the host will reject connection.
  config->mutable_video_configs()->clear();
  client_server_->set_protocol_config(std::move(config));
  client_session_ = client_server_->Connect(kHostJid, std::move(authenticator));
  client_session_->SetEventHandler(&client_session_event_handler_);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(INCOMPATIBLE_PROTOCOL, client_session_->error());
  EXPECT_FALSE(host_session_);
}

// Verify that GICE-only client is rejected with an appropriate error code.
TEST_F(JingleSessionTest, TestLegacyIceConnection) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _)).Times(0);

  EXPECT_CALL(client_session_event_handler_,
              OnSessionStateChange(Session::FAILED))
      .Times(1);

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 1, FakeAuthenticator::ACCEPT, true));

  scoped_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  config->set_ice_supported(false);
  client_server_->set_protocol_config(std::move(config));
  client_session_ = client_server_->Connect(kHostJid, std::move(authenticator));
  client_session_->SetEventHandler(&client_session_event_handler_);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(INCOMPATIBLE_PROTOCOL, client_session_->error());
  EXPECT_FALSE(host_session_);
}

TEST_F(JingleSessionTest, DeleteSessionOnIncomingConnection) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(DoAll(
          WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
          SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::ACCEPTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteSession));

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 3, FakeAuthenticator::ACCEPT, true));

  client_session_ = client_server_->Connect(kHostJid, std::move(authenticator));

  base::RunLoop().RunUntilIdle();
}

TEST_F(JingleSessionTest, DeleteSessionOnAuth) {
  // Same as the previous test, but set messages_till_started to 2 in
  // CreateSessionManagers so that the session will goes into the
  // AUTHENTICATING state after two message exchanges.
  CreateSessionManagers(3, 2, FakeAuthenticator::ACCEPT);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(DoAll(
          WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
          SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::ACCEPTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteSession));

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 3, FakeAuthenticator::ACCEPT, true));

  client_session_ = client_server_->Connect(kHostJid, std::move(authenticator));
  base::RunLoop().RunUntilIdle();
}

// Verify that we can connect with multistep authentication.
TEST_F(JingleSessionTest, TestMultistepAuth) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(3, FakeAuthenticator::ACCEPT, false));
}

}  // namespace protocol
}  // namespace remoting
