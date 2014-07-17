// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/network_settings.h"
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

// Send 100 messages 1024 bytes each. UDP messages are sent with 10ms delay
// between messages (about 1 second for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const char kChannelName[] = "test_channel";

void QuitCurrentThread() {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

ACTION(QuitThread) {
  QuitCurrentThread();
}

ACTION_P(QuitThreadOnCounter, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0)
    QuitCurrentThread();
}

class MockSessionManagerListener : public SessionManager::Listener {
 public:
  MOCK_METHOD0(OnSessionManagerReady, void());
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

class MockStreamChannelCallback {
 public:
  MOCK_METHOD1(OnDone, void(net::StreamSocket* socket));
};

}  // namespace

class JingleSessionTest : public testing::Test {
 public:
  JingleSessionTest() {
    message_loop_.reset(new base::MessageLoopForIO());
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  }

  // Helper method that handles OnIncomingSession().
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_.reset(session);
    host_session_->SetEventHandler(&host_session_event_handler_);

    session->set_config(SessionConfig::ForTest());
  }

  void DeleteSession() {
    host_session_.reset();
  }

  void OnClientChannelCreated(scoped_ptr<net::StreamSocket> socket) {
    client_channel_callback_.OnDone(socket.get());
    client_socket_ = socket.Pass();
  }

  void OnHostChannelCreated(scoped_ptr<net::StreamSocket> socket) {
    host_channel_callback_.OnDone(socket.get());
    host_socket_ = socket.Pass();
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    CloseSessions();
    CloseSessionManager();
    base::RunLoop().RunUntilIdle();
  }

  void CloseSessions() {
    host_socket_.reset();
    host_session_.reset();
    client_socket_.reset();
    client_session_.reset();
  }

  void CreateSessionManagers(int auth_round_trips, int messages_till_start,
                        FakeAuthenticator::Action auth_action) {
    host_signal_strategy_.reset(new FakeSignalStrategy(kHostJid));
    client_signal_strategy_.reset(new FakeSignalStrategy(kClientJid));
    FakeSignalStrategy::Connect(host_signal_strategy_.get(),
                                client_signal_strategy_.get());

    EXPECT_CALL(host_server_listener_, OnSessionManagerReady())
        .Times(1);

    NetworkSettings network_settings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    scoped_ptr<TransportFactory> host_transport(new LibjingleTransportFactory(
        NULL,
        ChromiumPortAllocator::Create(NULL, network_settings)
            .PassAs<cricket::HttpPortAllocatorBase>(),
        network_settings));
    host_server_.reset(new JingleSessionManager(host_transport.Pass()));
    host_server_->Init(host_signal_strategy_.get(), &host_server_listener_);

    scoped_ptr<AuthenticatorFactory> factory(
        new FakeHostAuthenticatorFactory(auth_round_trips,
          messages_till_start, auth_action, true));
    host_server_->set_authenticator_factory(factory.Pass());

    EXPECT_CALL(client_server_listener_, OnSessionManagerReady())
        .Times(1);
    scoped_ptr<TransportFactory> client_transport(new LibjingleTransportFactory(
        NULL,
        ChromiumPortAllocator::Create(NULL, network_settings)
            .PassAs<cricket::HttpPortAllocatorBase>(),
        network_settings));
    client_server_.reset(
        new JingleSessionManager(client_transport.Pass()));
    client_server_->Init(client_signal_strategy_.get(),
                         &client_server_listener_);
  }

  void CreateSessionManagers(int auth_round_trips,
                             FakeAuthenticator::Action auth_action) {
    CreateSessionManagers(auth_round_trips, 0, auth_action);
  }

  void CloseSessionManager() {
    if (host_server_.get()) {
      host_server_->Close();
      host_server_.reset();
    }
    if (client_server_.get()) {
      client_server_->Close();
      client_server_.reset();
    }
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
                  OnSessionStateChange(Session::CONNECTED))
          .Times(AtMost(1));
      EXPECT_CALL(host_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATING))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::FAILED))
            .Times(1);
      } else {
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
                  OnSessionStateChange(Session::CONNECTED))
          .Times(AtMost(1));
      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATING))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(client_session_event_handler_,
                    OnSessionStateChange(Session::FAILED))
            .Times(1);
      } else {
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

    client_session_ = client_server_->Connect(
        kHostJid, authenticator.Pass(),
        CandidateSessionConfig::CreateDefault());
    client_session_->SetEventHandler(&client_session_event_handler_);

    base::RunLoop().RunUntilIdle();
  }

  void CreateChannel() {
    client_session_->GetTransportChannelFactory()->CreateStreamChannel(
        kChannelName, base::Bind(&JingleSessionTest::OnClientChannelCreated,
                                 base::Unretained(this)));
    host_session_->GetTransportChannelFactory()->CreateStreamChannel(
        kChannelName, base::Bind(&JingleSessionTest::OnHostChannelCreated,
                                 base::Unretained(this)));

    int counter = 2;
    ExpectRouteChange(kChannelName);
    EXPECT_CALL(client_channel_callback_, OnDone(_))
        .WillOnce(QuitThreadOnCounter(&counter));
    EXPECT_CALL(host_channel_callback_, OnDone(_))
        .WillOnce(QuitThreadOnCounter(&counter));
    message_loop_->Run();

    EXPECT_TRUE(client_socket_.get());
    EXPECT_TRUE(host_socket_.get());
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

  scoped_ptr<FakeSignalStrategy> host_signal_strategy_;
  scoped_ptr<FakeSignalStrategy> client_signal_strategy_;

  scoped_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  scoped_ptr<JingleSessionManager> client_server_;
  MockSessionManagerListener client_server_listener_;

  scoped_ptr<Session> host_session_;
  MockSessionEventHandler host_session_event_handler_;
  scoped_ptr<Session> client_session_;
  MockSessionEventHandler client_session_event_handler_;

  MockStreamChannelCallback client_channel_callback_;
  MockStreamChannelCallback host_channel_callback_;

  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;
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
  client_session_ = client_server_->Connect(
      kHostJid, authenticator.Pass(), CandidateSessionConfig::CreateDefault());
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
      initiate_xml->FirstNamed(buzz::QName(kJingleNamespace, "jingle"));
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

// Verify that data can be sent over stream channel.
TEST_F(JingleSessionTest, TestStreamChannel) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  ASSERT_NO_FATAL_FAILURE(CreateChannel());

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_->Run();
  tester.CheckResults();
}

TEST_F(JingleSessionTest, DeleteSessionOnIncomingConnection) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(DoAll(
          WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
          SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::CONNECTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteSession));

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 3, FakeAuthenticator::ACCEPT, true));

  client_session_ = client_server_->Connect(
      kHostJid, authenticator.Pass(),
      CandidateSessionConfig::CreateDefault());

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
      OnSessionStateChange(Session::CONNECTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteSession));

  scoped_ptr<Authenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 3, FakeAuthenticator::ACCEPT, true));

  client_session_ = client_server_->Connect(
      kHostJid, authenticator.Pass(),
      CandidateSessionConfig::CreateDefault());
  base::RunLoop().RunUntilIdle();
}

// Verify that data can be sent over a multiplexed channel.
TEST_F(JingleSessionTest, TestMuxStreamChannel) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  client_session_->GetMultiplexedChannelFactory()->CreateStreamChannel(
      kChannelName, base::Bind(&JingleSessionTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_session_->GetMultiplexedChannelFactory()->CreateStreamChannel(
      kChannelName, base::Bind(&JingleSessionTest::OnHostChannelCreated,
                               base::Unretained(this)));

  int counter = 2;
  ExpectRouteChange("mux");
  EXPECT_CALL(client_channel_callback_, OnDone(_))
      .WillOnce(QuitThreadOnCounter(&counter));
  EXPECT_CALL(host_channel_callback_, OnDone(_))
      .WillOnce(QuitThreadOnCounter(&counter));
  message_loop_->Run();

  EXPECT_TRUE(client_socket_.get());
  EXPECT_TRUE(host_socket_.get());

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_->Run();
  tester.CheckResults();
}

// Verify that we can connect channels with multistep auth.
TEST_F(JingleSessionTest, TestMultistepAuthStreamChannel) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(3, FakeAuthenticator::ACCEPT, false));

  ASSERT_NO_FATAL_FAILURE(CreateChannel());

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_->Run();
  tester.CheckResults();
}

// Verify that we shutdown properly when channel authentication fails.
TEST_F(JingleSessionTest, TestFailedChannelAuth) {
  CreateSessionManagers(1, FakeAuthenticator::REJECT_CHANNEL);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  client_session_->GetTransportChannelFactory()->CreateStreamChannel(
      kChannelName, base::Bind(&JingleSessionTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_session_->GetTransportChannelFactory()->CreateStreamChannel(
      kChannelName, base::Bind(&JingleSessionTest::OnHostChannelCreated,
                               base::Unretained(this)));

  // Terminate the message loop when we get rejection notification
  // from the host.
  EXPECT_CALL(host_channel_callback_, OnDone(NULL))
      .WillOnce(QuitThread());
  EXPECT_CALL(client_channel_callback_, OnDone(_))
      .Times(AtMost(1));
  ExpectRouteChange(kChannelName);

  message_loop_->Run();

  EXPECT_TRUE(!host_socket_.get());
}

}  // namespace protocol
}  // namespace remoting
