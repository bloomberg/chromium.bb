// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "remoting/host/client_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/user_authenticator_fake.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

UserAuthenticator* MakeUserAuthenticator() {
  return new UserAuthenticatorFake();
}

// A task that does nothing.
class DummyTask : public Task {
 public:
  void Run() {}
};

}  // namespace

using protocol::MockConnectionToClient;
using protocol::MockConnectionToClientEventHandler;
using protocol::MockHostStub;
using protocol::MockInputStub;

using testing::_;
using testing::InSequence;

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() {
  }

  virtual void SetUp() {
    connection_ = new MockConnectionToClient(&message_loop_,
                                             &connection_event_handler_,
                                             &host_stub_,
                                             &input_stub_);
    client_session_ = new ClientSession(&session_event_handler_,
                                        base::Bind(MakeUserAuthenticator),
                                        connection_,
                                        &input_stub_);
    credentials_.set_type(protocol::PASSWORD);
    credentials_.set_username("user");
    credentials_.set_credential("password");
  }

  virtual void TearDown() {
  }

 protected:
  MessageLoop message_loop_;
  MockConnectionToClientEventHandler connection_event_handler_;
  MockHostStub host_stub_;
  MockInputStub input_stub_;
  MockClientSessionEventHandler session_event_handler_;
  scoped_refptr<MockConnectionToClient> connection_;
  scoped_refptr<ClientSession> client_session_;
  protocol::LocalLoginCredentials credentials_;
};

TEST_F(ClientSessionTest, InputStubFilter) {
  protocol::KeyEvent key_event1;
  key_event1.set_pressed(true);
  key_event1.set_keycode(1);

  protocol::KeyEvent key_event2;
  key_event2.set_pressed(true);
  key_event2.set_keycode(2);

  protocol::KeyEvent key_event3;
  key_event3.set_pressed(true);
  key_event3.set_keycode(3);

  protocol::MouseEvent mouse_event1;
  mouse_event1.set_x(100);
  mouse_event1.set_y(101);

  protocol::MouseEvent mouse_event2;
  mouse_event2.set_x(200);
  mouse_event2.set_y(201);

  protocol::MouseEvent mouse_event3;
  mouse_event3.set_x(300);
  mouse_event3.set_y(301);

  credentials_.set_type(protocol::PASSWORD);
  credentials_.set_username("user");
  credentials_.set_credential("password");

  InSequence s;
  EXPECT_CALL(session_event_handler_, LocalLoginSucceeded(_));
  EXPECT_CALL(input_stub_, InjectKeyEvent(&key_event2, _));
  EXPECT_CALL(input_stub_, InjectMouseEvent(&mouse_event2, _));
  EXPECT_CALL(*connection_.get(), Disconnect());

  // These events should not get through to the input stub,
  // because the client isn't authenticated yet.
  client_session_->InjectKeyEvent(&key_event1, new DummyTask());
  client_session_->InjectMouseEvent(&mouse_event1, new DummyTask());
  client_session_->BeginSessionRequest(&credentials_, new DummyTask());
  // These events should get through to the input stub.
  client_session_->InjectKeyEvent(&key_event2, new DummyTask());
  client_session_->InjectMouseEvent(&mouse_event2, new DummyTask());
  client_session_->Disconnect();
  // These events should not get through to the input stub,
  // because the client has disconnected.
  client_session_->InjectKeyEvent(&key_event3, new DummyTask());
  client_session_->InjectMouseEvent(&mouse_event3, new DummyTask());
}

}  // namespace remoting
