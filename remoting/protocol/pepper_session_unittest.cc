// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_session.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/test/test_timeouts.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/pepper_session_manager.h"
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
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::WithArg;

namespace remoting {
namespace protocol {

namespace {

const char kHostJid[] = "host1@gmail.com/123";
const char kClientJid[] = "host2@gmail.com/321";

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

}  // namespace

class PepperSessionTest : public testing::Test {
 public:
  PepperSessionTest()
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
    host_server_->Init(
        host_signal_strategy_.get(), &host_server_listener_, false);

    host_server_->set_authenticator_factory(
        new FakeHostAuthenticatorFactory(auth_round_trips, auth_action, true));

    EXPECT_CALL(client_server_listener_, OnSessionManagerReady())
        .Times(1);
    client_server_.reset(new PepperSessionManager(NULL));
    client_server_->Init(
        client_signal_strategy_.get(), &client_server_listener_, false);
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
            WithArg<0>(Invoke(this, &PepperSessionTest::SetHostSession)),
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
        // Expect that the connection will be closed eventually.
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CLOSED))
            .Times(AtMost(1));
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
        // Expect that the connection will be closed eventually.
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::CLOSED))
            .Times(AtMost(1));
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
  }

  JingleThreadMessageLoop message_loop_;

  scoped_ptr<FakeSignalStrategy> host_signal_strategy_;
  scoped_ptr<FakeSignalStrategy> client_signal_strategy_;

  scoped_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  scoped_ptr<PepperSessionManager> client_server_;
  MockSessionManagerListener client_server_listener_;

  scoped_ptr<Session> host_session_;
  MockSessionCallback host_connection_callback_;
  scoped_ptr<Session> client_session_;
  MockSessionCallback client_connection_callback_;
};


// Verify that we can create and destroy session managers without a
// connection.
TEST_F(PepperSessionTest, CreateAndDestoy) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
}

// Verify that an incoming session can be rejected, and that the
// status of the connection is set to FAILED in this case.
TEST_F(PepperSessionTest, RejectConnection) {
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
TEST_F(PepperSessionTest, Connect) {
  CreateSessionManagers(1, FakeAuthenticator::ACCEPT);
  InitiateConnection(1, FakeAuthenticator::ACCEPT, false);
}

// Verify that we can connect two endpoints with multi-step authentication.
TEST_F(PepperSessionTest, ConnectWithMultistep) {
  CreateSessionManagers(3, FakeAuthenticator::ACCEPT);
  InitiateConnection(3, FakeAuthenticator::ACCEPT, false);
}

// Verify that connection is terminated when single-step auth fails.
TEST_F(PepperSessionTest, ConnectWithBadAuth) {
  CreateSessionManagers(1, FakeAuthenticator::REJECT);
  InitiateConnection(1, FakeAuthenticator::ACCEPT, true);
}

// Verify that connection is terminated when multi-step auth fails.
TEST_F(PepperSessionTest, ConnectWithBadMultistepAuth) {
  CreateSessionManagers(3, FakeAuthenticator::REJECT);
  InitiateConnection(3, FakeAuthenticator::ACCEPT, true);
}

}  // namespace protocol
}  // namespace remoting
