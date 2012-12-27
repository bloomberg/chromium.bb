// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/capturer/video_capturer_mock_objects.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::MockConnectionToClient;
using protocol::MockClientStub;
using protocol::MockHostStub;
using protocol::MockInputStub;
using protocol::MockSession;
using protocol::MockVideoStub;
using protocol::SessionConfig;

using testing::_;
using testing::AnyNumber;
using testing::DeleteArg;
using testing::Expectation;
using testing::InSequence;
using testing::Return;
using testing::ReturnRef;

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() : event_executor_(NULL) {}

  virtual void SetUp() OVERRIDE {
    ui_task_runner_ = new AutoThreadTaskRunner(
        message_loop_.message_loop_proxy(),
        base::Bind(&ClientSessionTest::QuitMainMessageLoop,
                   base::Unretained(this)));

    client_jid_ = "user@domain/rest-of-jid";

    desktop_environment_factory_.reset(new MockDesktopEnvironmentFactory());
    EXPECT_CALL(*desktop_environment_factory_, CreatePtr())
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(this,
                               &ClientSessionTest::CreateDesktopEnvironment));

    // Set up a large default screen size that won't affect most tests.
    screen_size_.set(1000, 1000);

    session_config_ = SessionConfig::ForTest();

    // Mock protocol::Session APIs called directly by ClientSession.
    protocol::MockSession* session = new MockSession();
    EXPECT_CALL(*session, config()).WillRepeatedly(ReturnRef(session_config_));
    EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(client_jid_));
    EXPECT_CALL(*session, SetEventHandler(_));

    // Mock protocol::ConnectionToClient APIs called directly by ClientSession.
    // HostStub is not touched by ClientSession, so we can safely pass NULL.
    scoped_ptr<MockConnectionToClient> connection(
        new MockConnectionToClient(session, NULL));
    EXPECT_CALL(*connection, session()).WillRepeatedly(Return(session));
    EXPECT_CALL(*connection, client_stub())
        .WillRepeatedly(Return(&client_stub_));
    EXPECT_CALL(*connection, video_stub()).WillRepeatedly(Return(&video_stub_));
    EXPECT_CALL(*connection, Disconnect());
    connection_ = connection.get();

    client_session_ = new ClientSession(
        &session_event_handler_,
        ui_task_runner_, // Audio thread.
        ui_task_runner_, // Capture thread.
        ui_task_runner_, // Encode thread.
        ui_task_runner_, // Network thread.
        connection.PassAs<protocol::ConnectionToClient>(),
        desktop_environment_factory_.get(),
        base::TimeDelta());
  }

  virtual void TearDown() OVERRIDE {
    // MockClientSessionEventHandler won't trigger Stop, so fake it.
    client_session_->Stop(base::Bind(
        &ClientSessionTest::OnClientStopped, base::Unretained(this)));

    // Run message loop before destroying because the session is destroyed
    // asynchronously.
    ui_task_runner_ = NULL;
    message_loop_.Run();

    // Verify that the client session has been stopped.
    EXPECT_TRUE(client_session_.get() == NULL);
  }

 protected:
  DesktopEnvironment* CreateDesktopEnvironment() {
    MockVideoFrameCapturer* capturer = new MockVideoFrameCapturer();
    EXPECT_CALL(*capturer, Start(_));
    EXPECT_CALL(*capturer, Stop());
    EXPECT_CALL(*capturer, InvalidateRegion(_)).Times(AnyNumber());
    EXPECT_CALL(*capturer, CaptureFrame()).Times(AnyNumber());
    EXPECT_CALL(*capturer, size_most_recent())
        .WillRepeatedly(ReturnRef(screen_size_));

    EXPECT_TRUE(!event_executor_);
    event_executor_ = new MockEventExecutor();
    return new DesktopEnvironment(scoped_ptr<AudioCapturer>(NULL),
                                  scoped_ptr<EventExecutor>(event_executor_),
                                  scoped_ptr<VideoFrameCapturer>(capturer));
  }

  void DisconnectClientSession() {
    client_session_->Disconnect();
    // MockSession won't trigger OnConnectionClosed, so fake it.
    client_session_->OnConnectionClosed(client_session_->connection(),
                                        protocol::OK);
  }

  void QuitMainMessageLoop() {
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  void OnClientStopped() {
    client_session_ = NULL;
  }

  // Message loop passed to |client_session_| to perform all functions on.
  MessageLoop message_loop_;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  // ClientSession instance under test.
  scoped_refptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Screen size that the fake VideoFrameCapturer should report.
  SkISize screen_size_;

  // Storage for values to be returned by the protocol::Session mock.
  SessionConfig session_config_;
  std::string client_jid_;

  // Stubs returned to |client_session_| components by |connection_|.
  MockClientStub client_stub_;
  MockVideoStub video_stub_;

  // DesktopEnvironment owns |event_executor_|, but input injection tests need
  // to express expectations on it.
  MockEventExecutor* event_executor_;

  // ClientSession owns |connection_| but tests need it to inject fake events.
  MockConnectionToClient* connection_;

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory_;
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
  EXPECT_CALL(*event_executor_, StartPtr(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(*event_executor_, InjectClipboardEvent(EqualsClipboardEvent(
      kMimeTypeTextUtf8, "b")));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // This event should not get through to the clipboard stub,
  // because the client isn't authenticated yet.
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event1);
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // This event should get through to the clipboard stub.
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event2);
  DisconnectClientSession();
  // This event should not get through to the clipboard stub,
  // because the client has disconnected.
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event3);
}

MATCHER_P2(EqualsUsbEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == (unsigned int)usb_keycode &&
         arg.pressed() == pressed;
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
  key_event1.set_usb_keycode(1);

  protocol::KeyEvent key_event2_down;
  key_event2_down.set_pressed(true);
  key_event2_down.set_usb_keycode(2);

  protocol::KeyEvent key_event2_up;
  key_event2_up.set_pressed(false);
  key_event2_up.set_usb_keycode(2);

  protocol::KeyEvent key_event3;
  key_event3.set_pressed(true);
  key_event3.set_usb_keycode(3);

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
  EXPECT_CALL(*event_executor_, StartPtr(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, false)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // These events should not get through to the input stub,
  // because the client isn't authenticated yet.
  connection_->input_stub()->InjectKeyEvent(key_event1);
  connection_->input_stub()->InjectMouseEvent(mouse_event1);
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // These events should get through to the input stub.
  connection_->input_stub()->InjectKeyEvent(key_event2_down);
  connection_->input_stub()->InjectKeyEvent(key_event2_up);
  connection_->input_stub()->InjectMouseEvent(mouse_event2);
  DisconnectClientSession();
  // These events should not get through to the input stub,
  // because the client has disconnected.
  connection_->input_stub()->InjectKeyEvent(key_event3);
  connection_->input_stub()->InjectMouseEvent(mouse_event3);
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
  EXPECT_CALL(*event_executor_, StartPtr(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(100, 101)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
  // This event should get through to the input stub.
  connection_->input_stub()->InjectMouseEvent(mouse_event1);
  // This one should too because the local event echoes the remote one.
  client_session_->LocalMouseMoved(SkIPoint::Make(mouse_event1.x(),
                                                  mouse_event1.y()));
  connection_->input_stub()->InjectMouseEvent(mouse_event2);
  // This one should not.
  client_session_->LocalMouseMoved(SkIPoint::Make(mouse_event1.x(),
                                                  mouse_event1.y()));
  connection_->input_stub()->InjectMouseEvent(mouse_event3);
  // TODO(jamiewalch): Verify that remote inputs are re-enabled eventually
  // (via dependency injection, not sleep!)
  DisconnectClientSession();
}

TEST_F(ClientSessionTest, RestoreEventState) {
  protocol::KeyEvent key1;
  key1.set_pressed(true);
  key1.set_usb_keycode(1);

  protocol::KeyEvent key2;
  key2.set_pressed(true);
  key2.set_usb_keycode(2);

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);

  InSequence s;
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(*event_executor_, StartPtr(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(1, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, true)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(1, false)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, false)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, false)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());

  connection_->input_stub()->InjectKeyEvent(key1);
  connection_->input_stub()->InjectKeyEvent(key2);
  connection_->input_stub()->InjectMouseEvent(mousedown);

  DisconnectClientSession();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  screen_size_.set(200, 100);

  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(*event_executor_, StartPtr(_));
  Expectation connected =
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
      connected =
          EXPECT_CALL(*event_executor_,
                      InjectMouseEvent(EqualsMouseEvent(expected_x[i],
                                                        expected_y[j])))
              .After(connected);
      connection_->input_stub()->InjectMouseEvent(event);
    }
  }

  DisconnectClientSession();
}

}  // namespace remoting
