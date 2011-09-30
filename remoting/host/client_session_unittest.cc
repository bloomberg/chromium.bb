// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

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
using protocol::MockSession;

using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;
using testing::ReturnRef;

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() {}

  virtual void SetUp() {
    client_jid_ = "user@domain/rest-of-jid";
    EXPECT_CALL(session_, jid()).WillRepeatedly(ReturnRef(client_jid_));

    connection_ = new MockConnectionToClient(
        &connection_event_handler_, &host_stub_, &input_stub_);

    EXPECT_CALL(*connection_, session()).WillRepeatedly(Return(&session_));

    // Set up a large default screen size that won't affect most tests.
    default_screen_size_.SetSize(1000, 1000);
    ON_CALL(capturer_, size_most_recent()).WillByDefault(ReturnRef(
        default_screen_size_));

    user_authenticator_ = new MockUserAuthenticator();
    client_session_ = new ClientSession(
        &session_event_handler_,
        user_authenticator_,
        connection_,
        &input_stub_,
        &capturer_);
  }

 protected:
  gfx::Size default_screen_size_;
  MessageLoop message_loop_;
  std::string client_jid_;
  MockSession session_;
  MockConnectionToClientEventHandler connection_event_handler_;
  MockHostStub host_stub_;
  MockInputStub input_stub_;
  MockCapturer capturer_;
  MockClientSessionEventHandler session_event_handler_;
  MockUserAuthenticator* user_authenticator_;
  scoped_refptr<MockConnectionToClient> connection_;
  scoped_refptr<ClientSession> client_session_;
};

MATCHER_P2(EqualsKeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

MATCHER_P2(EqualsMouseEvent, x, y, "") {
  return arg.x() == x && arg.y() == y;
}

MATCHER_P(EqualsMouseUpEvent, button, "") {
  return arg.button() == button && !arg.button_down();
}

TEST_F(ClientSessionTest, InputStubFilter) {
  protocol::KeyEvent key_event1;
  key_event1.set_pressed(true);
  key_event1.set_keycode(1);

  protocol::KeyEvent key_event2_down;
  key_event2_down.set_pressed(true);
  key_event2_down.set_keycode(2);

  protocol::KeyEvent key_event2_up;
  key_event2_up.set_pressed(false);
  key_event2_up.set_keycode(2);

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

  protocol::LocalLoginCredentials credentials;
  credentials.set_type(protocol::PASSWORD);
  credentials.set_username("user");
  credentials.set_credential("password");

  InSequence s;
  EXPECT_CALL(*user_authenticator_, Authenticate(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(session_event_handler_, LocalLoginSucceeded(_));
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(2, true)));
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(2, false)));
  EXPECT_CALL(input_stub_, InjectMouseEvent(EqualsMouseEvent(200, 201)));

  // These events should not get through to the input stub,
  // because the client isn't authenticated yet.
  client_session_->InjectKeyEvent(key_event1);
  client_session_->InjectMouseEvent(mouse_event1);
  client_session_->BeginSessionRequest(&credentials, new DummyTask());
  // These events should get through to the input stub.
  client_session_->InjectKeyEvent(key_event2_down);
  client_session_->InjectKeyEvent(key_event2_up);
  client_session_->InjectMouseEvent(mouse_event2);
  client_session_->OnDisconnected();
  // These events should not get through to the input stub,
  // because the client has disconnected.
  client_session_->InjectKeyEvent(key_event3);
  client_session_->InjectMouseEvent(mouse_event3);
}

TEST_F(ClientSessionTest, LocalInputTest) {
  protocol::MouseEvent mouse_event1;
  mouse_event1.set_x(100);
  mouse_event1.set_y(101);
  protocol::MouseEvent mouse_event2;
  mouse_event2.set_x(200);
  mouse_event2.set_y(201);
  protocol::MouseEvent mouse_event3;
  mouse_event3.set_x(300);
  mouse_event3.set_y(301);

  protocol::LocalLoginCredentials credentials;
  credentials.set_type(protocol::PASSWORD);
  credentials.set_username("user");
  credentials.set_credential("password");

  InSequence s;
  EXPECT_CALL(*user_authenticator_, Authenticate(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(session_event_handler_, LocalLoginSucceeded(_));
  EXPECT_CALL(input_stub_, InjectMouseEvent(EqualsMouseEvent(100, 101)));
  EXPECT_CALL(input_stub_, InjectMouseEvent(EqualsMouseEvent(200, 201)));

  client_session_->BeginSessionRequest(&credentials, new DummyTask());
  // This event should get through to the input stub.
  client_session_->InjectMouseEvent(mouse_event1);
  // This one should too because the local event echoes the remote one.
  client_session_->LocalMouseMoved(gfx::Point(mouse_event1.x(),
                                              mouse_event1.y()));
  client_session_->InjectMouseEvent(mouse_event2);
  // This one should not.
  client_session_->LocalMouseMoved(gfx::Point(mouse_event1.x(),
                                              mouse_event1.y()));
  client_session_->InjectMouseEvent(mouse_event3);
  // TODO(jamiewalch): Verify that remote inputs are re-enabled eventually
  // (via dependency injection, not sleep!)
  client_session_->OnDisconnected();
}

TEST_F(ClientSessionTest, RestoreEventState) {
  protocol::KeyEvent key1;
  key1.set_pressed(true);
  key1.set_keycode(1);

  protocol::KeyEvent key2;
  key2.set_pressed(true);
  key2.set_keycode(2);

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);

  client_session_->RecordKeyEvent(key1);
  client_session_->RecordKeyEvent(key2);
  client_session_->RecordMouseButtonState(mousedown);

  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(1, false)));
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(2, false)));
  EXPECT_CALL(input_stub_, InjectMouseEvent(EqualsMouseUpEvent(
      protocol::MouseEvent::BUTTON_LEFT)));

  client_session_->RestoreEventState();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  gfx::Size screen(200, 100);
  EXPECT_CALL(capturer_, size_most_recent())
      .WillRepeatedly(ReturnRef(screen));

  protocol::LocalLoginCredentials credentials;
  credentials.set_type(protocol::PASSWORD);
  credentials.set_username("user");
  credentials.set_credential("password");
  EXPECT_CALL(*user_authenticator_, Authenticate(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(session_event_handler_, LocalLoginSucceeded(_));
  client_session_->BeginSessionRequest(&credentials, new DummyTask());

  int input_x[3] = { -999, 100, 999 };
  int expected_x[3] = { 0, 100, 199 };
  int input_y[3] = { -999, 50, 999 };
  int expected_y[3] = { 0, 50, 99 };

  protocol::MouseEvent event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      event.set_x(input_x[i]);
      event.set_y(input_y[j]);
      EXPECT_CALL(input_stub_, InjectMouseEvent(EqualsMouseEvent(
          expected_x[i], expected_y[j])));
      client_session_->InjectMouseEvent(event);
    }
  }
}

}  // namespace remoting
