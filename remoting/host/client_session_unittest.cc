// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/capturer/video_capturer_mock_objects.h"
#include "remoting/capturer/video_frame_capturer_fake.h"
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
using testing::DoAll;
using testing::Expectation;
using testing::InSequence;
using testing::Return;
using testing::ReturnRef;

namespace {

ACTION_P2(InjectClipboardEvent, connection, event) {
  connection->clipboard_stub()->InjectClipboardEvent(event);
}

ACTION_P2(InjectKeyEvent, connection, event) {
  connection->input_stub()->InjectKeyEvent(event);
}

ACTION_P2(InjectMouseEvent, connection, event) {
  connection->input_stub()->InjectMouseEvent(event);
}

ACTION_P2(LocalMouseMoved, client_session, event) {
  client_session->LocalMouseMoved(SkIPoint::Make(event.x(), event.y()));
}

}  // namespace

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest()
      : client_jid_("user@domain/rest-of-jid"),
        event_executor_(NULL) {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Disconnects the client session.
  void DisconnectClientSession();

  // Asynchronously stops the client session. OnClientStopped() will be called
  // once the client session is fully stopped.
  void StopClientSession();

 protected:
  // Creates a DesktopEnvironment with a fake VideoFrameCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected. This effectively enables
  // the input pipe line and starts video capturing.
  void ConnectClientSession();

  // Invoked when the last reference to the AutoThreadTaskRunner has been
  // released and quits the message loop to finish the test.
  void QuitMainMessageLoop();

  // Releases the ClientSession when it has been fully stopped, allowing
  // the MessageLoop to quit.
  void OnClientStopped();

  // Message loop passed to |client_session_| to perform all functions on.
  MessageLoop message_loop_;

  // ClientSession instance under test.
  scoped_refptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Storage for values to be returned by the protocol::Session mock.
  SessionConfig session_config_;
  const std::string client_jid_;

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

void ClientSessionTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(),
      base::Bind(&ClientSessionTest::QuitMainMessageLoop,
                 base::Unretained(this)));

  desktop_environment_factory_.reset(new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory_, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &ClientSessionTest::CreateDesktopEnvironment));

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
      ui_task_runner, // Audio thread.
      ui_task_runner, // Capture thread.
      ui_task_runner, // Encode thread.
      ui_task_runner, // Network thread.
      connection.PassAs<protocol::ConnectionToClient>(),
      desktop_environment_factory_.get(),
      base::TimeDelta());
}

void ClientSessionTest::TearDown() {
  // Verify that the client session has been stopped.
  EXPECT_TRUE(client_session_.get() == NULL);
}

void ClientSessionTest::DisconnectClientSession() {
  client_session_->Disconnect();
  // MockSession won't trigger OnConnectionClosed, so fake it.
  client_session_->OnConnectionClosed(client_session_->connection(),
                                      protocol::OK);
}

void ClientSessionTest::StopClientSession() {
  // MockClientSessionEventHandler won't trigger Stop, so fake it.
  client_session_->Stop(base::Bind(
      &ClientSessionTest::OnClientStopped, base::Unretained(this)));
}

DesktopEnvironment* ClientSessionTest::CreateDesktopEnvironment() {
  scoped_ptr<VideoFrameCapturer> video_capturer(new VideoFrameCapturerFake());

  EXPECT_TRUE(!event_executor_);
  event_executor_ = new MockEventExecutor();
  return new DesktopEnvironment(scoped_ptr<AudioCapturer>(NULL),
                                scoped_ptr<EventExecutor>(event_executor_),
                                video_capturer.Pass());
}

void ClientSessionTest::ConnectClientSession() {
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
}

void ClientSessionTest::QuitMainMessageLoop() {
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void ClientSessionTest::OnClientStopped() {
  client_session_ = NULL;
}

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

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          // This event should get through to the clipboard stub.
          InjectClipboardEvent(connection_, clipboard_event2),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          // This event should not get through to the clipboard stub,
          // because the client has disconnected.
          InjectClipboardEvent(connection_, clipboard_event3),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*event_executor_, InjectClipboardEvent(EqualsClipboardEvent(
      kMimeTypeTextUtf8, "b")));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // This event should not get through to the clipboard stub,
  // because the client isn't authenticated yet.
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event1);

  ConnectClientSession();
  message_loop_.Run();
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

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          // These events should get through to the input stub.
          InjectKeyEvent(connection_, key_event2_down),
          InjectKeyEvent(connection_, key_event2_up),
          InjectMouseEvent(connection_, mouse_event2),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          // These events should not get through to the input stub,
          // because the client has disconnected.
          InjectKeyEvent(connection_, key_event3),
          InjectMouseEvent(connection_, mouse_event3),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, false)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  // These events should not get through to the input stub,
  // because the client isn't authenticated yet.
  connection_->input_stub()->InjectKeyEvent(key_event1);
  connection_->input_stub()->InjectMouseEvent(mouse_event1);

  ConnectClientSession();
  message_loop_.Run();
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

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          // This event should get through to the input stub.
          InjectMouseEvent(connection_, mouse_event1),
          // This one should too because the local event echoes the remote one.
          LocalMouseMoved(client_session_.get(), mouse_event1),
          InjectMouseEvent(connection_, mouse_event2),
          // This one should not.
          LocalMouseMoved(client_session_.get(), mouse_event1),
          InjectMouseEvent(connection_, mouse_event3),
          // TODO(jamiewalch): Verify that remote inputs are re-enabled
          // eventually (via dependency injection, not sleep!)
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(100, 101)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseEvent(200, 201)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
  message_loop_.Run();
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

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          InjectKeyEvent(connection_, key1),
          InjectKeyEvent(connection_, key2),
          InjectMouseEvent(connection_, mousedown),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(1, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, true)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, true)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(1, false)));
  EXPECT_CALL(*event_executor_, InjectKeyEvent(EqualsUsbEvent(2, false)));
  EXPECT_CALL(*event_executor_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, false)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
  message_loop_.Run();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(*event_executor_, StartPtr(_));
  Expectation connected =
      EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  int input_x[3] = { -999, 100, 999 };
  int expected_x[3] = { 0, 100, VideoFrameCapturerFake::kWidth - 1 };
  int input_y[3] = { -999, 50, 999 };
  int expected_y[3] = { 0, 50, VideoFrameCapturerFake::kHeight - 1 };

  // Inject the 1st event once a video packet has been received.
  protocol::MouseEvent injected_event;
  injected_event.set_x(input_x[0]);
  injected_event.set_y(input_y[0]);
  connected =
      EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
          .After(connected)
          .WillOnce(InjectMouseEvent(connection_, injected_event));

  protocol::MouseEvent expected_event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      // Skip the first iteration since the 1st event has been injected already.
      if (i > 0 || j > 0) {
        injected_event.set_x(input_x[i]);
        injected_event.set_y(input_y[j]);
        connected =
            EXPECT_CALL(*event_executor_,
                        InjectMouseEvent(EqualsMouseEvent(expected_event.x(),
                                                          expected_event.y())))
                .After(connected)
                .WillOnce(InjectMouseEvent(connection_, injected_event));
      }

      expected_event.set_x(expected_x[i]);
      expected_event.set_y(expected_y[j]);
    }
  }

  // Shutdown the connection once the last event has been received.
  EXPECT_CALL(*event_executor_,
              InjectMouseEvent(EqualsMouseEvent(expected_event.x(),
                                                expected_event.y())))
      .After(connected)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  ConnectClientSession();
  message_loop_.Run();
}

}  // namespace remoting
