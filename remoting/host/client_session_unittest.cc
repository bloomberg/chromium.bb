// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/constants.h"
#include "remoting/host/client_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::MockConnectionToClient;
using protocol::MockConnectionToClientEventHandler;
using protocol::MockHostStub;
using protocol::MockHostEventStub;
using protocol::MockSession;

using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;
using testing::ReturnRef;

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() {}

  virtual void SetUp() OVERRIDE {
    client_jid_ = "user@domain/rest-of-jid";

    // Set up a large default screen size that won't affect most tests.
    default_screen_size_.set(1000, 1000);
    EXPECT_CALL(capturer_, size_most_recent())
        .WillRepeatedly(ReturnRef(default_screen_size_));

    protocol::MockSession* session = new MockSession();
    EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(client_jid_));
    EXPECT_CALL(*session, SetStateChangeCallback(_));
    EXPECT_CALL(*session, SetRouteChangeCallback(_));
    EXPECT_CALL(*session, Close());
    scoped_ptr<protocol::ConnectionToClient> connection(
        new protocol::ConnectionToClient(session));
    client_session_.reset(new ClientSession(
        &session_event_handler_, connection.Pass(),
        &host_event_stub_, &capturer_, base::TimeDelta()));
  }

  virtual void TearDown() OVERRIDE {
    client_session_.reset();
    // Run message loop before destroying because protocol::Session is
    // destroyed asynchronously.
    message_loop_.RunAllPending();
  }

 protected:
  void DisconnectClientSession() {
    client_session_->Disconnect();
    // MockSession won't trigger OnConnectionClosed, so fake it.
    client_session_->OnConnectionClosed(client_session_->connection(),
                                        protocol::OK);
  }

  SkISize default_screen_size_;
  MessageLoop message_loop_;
  std::string client_jid_;
  MockHostStub host_stub_;
  MockHostEventStub host_event_stub_;
  MockCapturer capturer_;
  MockClientSessionEventHandler session_event_handler_;
  scoped_ptr<ClientSession> client_session_;
};

MATCHER_P2(EqualsClipboardEvent, m, d, "") {
  return (strcmp(arg.mime_type().c_str(), m) == 0 &&
      memcmp(arg.data().data(), d, arg.data().size()) == 0);
}

TEST_F(ClientSessionTest, ClipboardStubFilter) {
  protocol::ClipboardEvent clipboard_event1;
  clipboard_event1.set_mime_type(kMimeTypeTextUtf8);
  clipboard_event1.set_data("a");

  protocol::ClipboardEvent clipboard_event2;
  clipboard_event2.set_mime_type(kMimeTypeTextUtf8);
  clipboard_event2.set_data("b");

  protocol::ClipboardEvent clipboard_event3;
  clipboard_event3.set_mime_type(kMimeTypeTextUtf8);
  clipboard_event3.set_data("c");

  InSequence s;
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(host_event_stub_, InjectClipboardEvent(EqualsClipboardEvent(
      kMimeTypeTextUtf8, "b")));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // This event should not get through to the clipboard stub,
  // because the client isn't authenticated yet.
  client_session_->InjectClipboardEvent(clipboard_event1);
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // This event should get through to the clipboard stub.
  client_session_->InjectClipboardEvent(clipboard_event2);
  DisconnectClientSession();
  // This event should not get through to the clipboard stub,
  // because the client has disconnected.
  client_session_->InjectClipboardEvent(clipboard_event3);
}

MATCHER_P2(EqualsKeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

MATCHER_P2(EqualsMouseEvent, x, y, "") {
  return arg.x() == x && arg.y() == y;
}

MATCHER_P2(EqualsMouseButtonEvent, button, down, "") {
  return arg.button() == button && arg.button_down() == down;
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

  InSequence s;
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(2, true)));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(2, false)));
  EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // These events should not get through to the input stub,
  // because the client isn't authenticated yet.
  client_session_->InjectKeyEvent(key_event1);
  client_session_->InjectMouseEvent(mouse_event1);
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // These events should get through to the input stub.
  client_session_->InjectKeyEvent(key_event2_down);
  client_session_->InjectKeyEvent(key_event2_up);
  client_session_->InjectMouseEvent(mouse_event2);
  DisconnectClientSession();
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

  InSequence s;
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseEvent(100, 101)));
  EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // This event should get through to the input stub.
  client_session_->InjectMouseEvent(mouse_event1);
  // This one should too because the local event echoes the remote one.
  client_session_->LocalMouseMoved(SkIPoint::Make(mouse_event1.x(),
                                                  mouse_event1.y()));
  client_session_->InjectMouseEvent(mouse_event2);
  // This one should not.
  client_session_->LocalMouseMoved(SkIPoint::Make(mouse_event1.x(),
                                                  mouse_event1.y()));
  client_session_->InjectMouseEvent(mouse_event3);
  // TODO(jamiewalch): Verify that remote inputs are re-enabled eventually
  // (via dependency injection, not sleep!)
  DisconnectClientSession();
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

  InSequence s;
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(1, true)));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(2, true)));
  EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, true)));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(1, false)));
  EXPECT_CALL(host_event_stub_, InjectKeyEvent(EqualsKeyEvent(2, false)));
  EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, false)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());

  client_session_->InjectKeyEvent(key1);
  client_session_->InjectKeyEvent(key2);
  client_session_->InjectMouseEvent(mousedown);

  DisconnectClientSession();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  SkISize screen(SkISize::Make(200, 100));
  EXPECT_CALL(capturer_, size_most_recent())
      .WillRepeatedly(ReturnRef(screen));

  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());

  int input_x[3] = { -999, 100, 999 };
  int expected_x[3] = { 0, 100, 199 };
  int input_y[3] = { -999, 50, 999 };
  int expected_y[3] = { 0, 50, 99 };

  protocol::MouseEvent event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      event.set_x(input_x[i]);
      event.set_y(input_y[j]);
      EXPECT_CALL(host_event_stub_, InjectMouseEvent(EqualsMouseEvent(
          expected_x[i], expected_y[j])));
      client_session_->InjectMouseEvent(event);
    }
  }

  DisconnectClientSession();
}

}  // namespace remoting
