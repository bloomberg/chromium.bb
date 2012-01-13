// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::WithArg;

namespace remoting {
namespace protocol {

namespace {

// Send 100 messages 1024 bytes each. UDP messages are sent with 10ms delay
// between messages (about 1 second for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const int kUdpWriteDelayMs = 10;
const char kChannelName[] = "test_channel";

const char kHostJid[] = "host1@gmail.com/123";
const char kClientJid[] = "host2@gmail.com/321";

void QuitCurrentThread() {
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

ACTION(QuitThread) {
  QuitCurrentThread();
}

ACTION_P(QuitThreadOnCounter, counter) {
  (*counter)--;
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

class MockSessionCallback {
 public:
  MOCK_METHOD1(OnStateChange, void(Session::State));
};

class MockStreamChannelCallback {
 public:
  MOCK_METHOD1(OnDone, void(net::StreamSocket* socket));
};

class MockDatagramChannelCallback {
 public:
  MOCK_METHOD1(OnDone, void(net::Socket* socket));
};

}  // namespace

class JingleSessionTest : public testing::Test {
 public:
  JingleSessionTest()
      : message_loop_(talk_base::Thread::Current()) {
  }

  // Helper method that handles OnIncomingSession().
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_.reset(session);
    host_session_->SetStateChangeCallback(
        base::Bind(&MockSessionCallback::OnStateChange,
                   base::Unretained(&host_connection_callback_)));

    session->set_config(SessionConfig::GetDefault());
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    CloseSessions();
    CloseSessionManager();
  }

  void CloseSessions() {
    host_socket_.reset();
    client_socket_.reset();
    host_session_.reset();
    client_session_.reset();
  }

  void CreateSessionManagers(int auth_round_trips,
                        FakeAuthenticator::Action auth_action) {
    host_signal_strategy_.reset(new FakeSignalStrategy(kHostJid));
    client_signal_strategy_.reset(new FakeSignalStrategy(kClientJid));
    FakeSignalStrategy::Connect(host_signal_strategy_.get(),
                                client_signal_strategy_.get());

    EXPECT_CALL(host_server_listener_, OnSessionManagerReady())
        .Times(1);
    host_server_.reset(new JingleSessionManager(
        base::MessageLoopProxy::current()));
    host_server_->Init(host_signal_strategy_.get(), &host_server_listener_,
                       NetworkSettings());

    scoped_ptr<AuthenticatorFactory> factory(
        new FakeHostAuthenticatorFactory(auth_round_trips, auth_action, true));
    host_server_->set_authenticator_factory(factory.Pass());

    EXPECT_CALL(client_server_listener_, OnSessionManagerReady())
        .Times(1);
    client_server_.reset(new JingleSessionManager(
        base::MessageLoopProxy::current()));
    client_server_->Init(client_signal_strategy_.get(),
                         &client_server_listener_, NetworkSettings());
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
            WithArg<0>(Invoke(
                this, &JingleSessionTest::SetHostSession)),
            SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

    {
      InSequence dummy;

      EXPECT_CALL(host_connection_callback_,
                  OnStateChange(Session::CONNECTED))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::FAILED))
            .Times(1);
      } else {
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::AUTHENTICATED))
            .Times(1);
      }
    }

    {
      InSequence dummy;

      EXPECT_CALL(client_connection_callback_,
                  OnStateChange(Session::CONNECTING))
          .Times(1);
      EXPECT_CALL(client_connection_callback_,
                  OnStateChange(Session::CONNECTED))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::FAILED))
            .Times(1);
      } else {
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::AUTHENTICATED))
            .Times(1);
      }
    }

    Authenticator* authenticator = new FakeAuthenticator(
        FakeAuthenticator::CLIENT, auth_round_trips, auth_action, true);

    client_session_.reset(client_server_->Connect(
        kHostJid, authenticator,
        CandidateSessionConfig::CreateDefault(),
        base::Bind(&MockSessionCallback::OnStateChange,
                   base::Unretained(&client_connection_callback_))));

    message_loop_.RunAllPending();

    Mock::VerifyAndClearExpectations(&host_connection_callback_);
    Mock::VerifyAndClearExpectations(&client_connection_callback_);

    if (!expect_fail) {
      // Expect that the connection will be closed eventually.
      EXPECT_CALL(host_connection_callback_,
                  OnStateChange(Session::CLOSED))
          .Times(AtMost(1));
    }

    if (!expect_fail) {
      // Expect that the connection will be closed eventually.
      EXPECT_CALL(client_connection_callback_,
                  OnStateChange(Session::CLOSED))
          .Times(AtMost(1));
    }
  }

  void CreateChannel() {
    MockStreamChannelCallback client_callback;
    MockStreamChannelCallback host_callback;

    client_session_->CreateStreamChannel(kChannelName, base::Bind(
        &MockStreamChannelCallback::OnDone,
        base::Unretained(&host_callback)));
    host_session_->CreateStreamChannel(kChannelName, base::Bind(
        &MockStreamChannelCallback::OnDone,
        base::Unretained(&client_callback)));

    int counter = 2;
    net::StreamSocket* client_socket = NULL;
    net::StreamSocket* host_socket = NULL;
    EXPECT_CALL(client_callback, OnDone(_))
        .WillOnce(DoAll(SaveArg<0>(&client_socket),
                        QuitThreadOnCounter(&counter)));
    EXPECT_CALL(host_callback, OnDone(_))
        .WillOnce(DoAll(SaveArg<0>(&host_socket),
                        QuitThreadOnCounter(&counter)));
    message_loop_.Run();

    ASSERT_TRUE(client_socket != NULL);
    ASSERT_TRUE(host_socket != NULL);

    client_socket_.reset(client_socket);
    host_socket_.reset(host_socket);
  }

  JingleThreadMessageLoop message_loop_;

  scoped_ptr<FakeSignalStrategy> host_signal_strategy_;
  scoped_ptr<FakeSignalStrategy> client_signal_strategy_;

  scoped_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  scoped_ptr<JingleSessionManager> client_server_;
  MockSessionManagerListener client_server_listener_;

  scoped_ptr<Session> host_session_;
  MockSessionCallback host_connection_callback_;
  scoped_ptr<Session> client_session_;
  MockSessionCallback client_connection_callback_;

  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;
};

// Verify that we can create and destory server objects without a connection.
TEST_F(JingleSessionTest, CreateAndDestoy) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
}

// Verify that incoming session can be rejected, and that the status
// of the connection is set to FAILED in this case.
TEST_F(JingleSessionTest, RejectConnection) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);

  // Reject incoming session.
  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(SetArgumentPointee<1>(protocol::SessionManager::DECLINE));

  {
    InSequence dummy;

    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CONNECTING))
        .Times(1);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::FAILED))
        .Times(1);
  }

  Authenticator* authenticator = new FakeAuthenticator(
      FakeAuthenticator::CLIENT, 1, FakeAuthenticator::ACCEPT, true);
  client_session_.reset(client_server_->Connect(
      kHostJid, authenticator,
      CandidateSessionConfig::CreateDefault(),
      base::Bind(&MockSessionCallback::OnStateChange,
                 base::Unretained(&client_connection_callback_))));

  message_loop_.RunAllPending();
}

// Verify that we can connect two endpoints with single-step authentication.
TEST_F(JingleSessionTest, Connect) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  InitiateConnection(1, FakeAuthenticator::ACCEPT, false);
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

TEST_F(JingleSessionTest, ConnectWithBadChannelAuth) {
  CreateSessionManagers(1, FakeAuthenticator::REJECT_CHANNEL);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  MockStreamChannelCallback client_callback;
  MockStreamChannelCallback host_callback;

  client_session_->CreateStreamChannel(kChannelName, base::Bind(
      &MockStreamChannelCallback::OnDone,
      base::Unretained(&client_callback)));
  host_session_->CreateStreamChannel(kChannelName, base::Bind(
      &MockStreamChannelCallback::OnDone,
      base::Unretained(&host_callback)));

  EXPECT_CALL(client_callback, OnDone(_))
      .Times(AtMost(1))
      .WillOnce(DeleteArg<0>());
  EXPECT_CALL(host_callback, OnDone(NULL))
      .WillOnce(QuitThread());

  message_loop_.Run();

  client_session_->CancelChannelCreation(kChannelName);
  host_session_->CancelChannelCreation(kChannelName);
}

// Verify that data can be transmitted over the event channel.
TEST_F(JingleSessionTest, TestTcpChannel) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  ASSERT_NO_FATAL_FAILURE(CreateChannel());

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

// Verify that we can connect channels with multistep auth.
TEST_F(JingleSessionTest, TestMultistepAuthTcpChannel) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(3, FakeAuthenticator::ACCEPT, false));

  ASSERT_NO_FATAL_FAILURE(CreateChannel());

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

// Verify that data can be transmitted over the video RTP channel.
TEST_F(JingleSessionTest, TestUdpChannel) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  ASSERT_NO_FATAL_FAILURE(
      InitiateConnection(1, FakeAuthenticator::ACCEPT, false));

  MockDatagramChannelCallback client_callback;
  MockDatagramChannelCallback host_callback;

  int counter = 2;
  net::Socket* client_socket = NULL;
  net::Socket* host_socket = NULL;
  EXPECT_CALL(client_callback, OnDone(_))
      .WillOnce(DoAll(SaveArg<0>(&client_socket),
                      QuitThreadOnCounter(&counter)));
  EXPECT_CALL(host_callback, OnDone(_))
      .WillOnce(DoAll(SaveArg<0>(&host_socket),
                      QuitThreadOnCounter(&counter)));

  client_session_->CreateDatagramChannel(kChannelName, base::Bind(
      &MockDatagramChannelCallback::OnDone,
      base::Unretained(&host_callback)));
  host_session_->CreateDatagramChannel(kChannelName, base::Bind(
      &MockDatagramChannelCallback::OnDone,
      base::Unretained(&client_callback)));

  message_loop_.Run();

  scoped_ptr<net::Socket> client_socket_ptr(client_socket);
  scoped_ptr<net::Socket> host_socket_ptr(host_socket);

  ASSERT_TRUE(client_socket != NULL);
  ASSERT_TRUE(host_socket != NULL);

  DatagramConnectionTester tester(
      client_socket, host_socket, kMessageSize, kMessages, kUdpWriteDelayMs);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

}  // namespace protocol
}  // namespace remoting
