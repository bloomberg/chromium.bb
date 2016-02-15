// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/fake_host_extension.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

using protocol::MockClientStub;
using protocol::MockHostStub;
using protocol::MockInputStub;
using protocol::MockSession;
using protocol::MockVideoStub;
using protocol::SessionConfig;
using protocol::test::EqualsClipboardEvent;
using protocol::test::EqualsMouseButtonEvent;
using protocol::test::EqualsMouseMoveEvent;
using protocol::test::EqualsKeyEvent;

using testing::_;
using testing::AtLeast;
using testing::ReturnRef;
using testing::StrictMock;

namespace {

// Matches a |protocol::Capabilities| argument against a list of capabilities
// formatted as a space-separated string.
MATCHER_P(EqCapabilities, expected_capabilities, "") {
  if (!arg.has_capabilities())
    return false;

  std::vector<std::string> words_args = base::SplitString(
      arg.capabilities(), " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> words_expected = base::SplitString(
      expected_capabilities, " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::sort(words_args.begin(), words_args.end());
  std::sort(words_expected.begin(), words_expected.end());
  return words_args == words_expected;
}

protocol::MouseEvent MakeMouseMoveEvent(int x, int y) {
  protocol::MouseEvent result;
  result.set_x(x);
  result.set_y(y);
  return result;
}

protocol::KeyEvent MakeKeyEvent(bool pressed, uint32_t keycode) {
  protocol::KeyEvent result;
  result.set_pressed(pressed);
  result.set_usb_keycode(keycode);
  return result;
}

protocol::ClipboardEvent MakeClipboardEvent(const std::string& text) {
  protocol::ClipboardEvent result;
  result.set_mime_type(kMimeTypeTextUtf8);
  result.set_data(text);
  return result;
}

}  // namespace

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() : client_jid_("user@domain/rest-of-jid") {}

  void SetUp() override;
  void TearDown() override;

  // Creates the client session.
  void CreateClientSession();

 protected:
  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected. This effectively enables
  // the input pipe line and starts video capturing.
  void ConnectClientSession();

  // Fakes video size notification from the VideoStream.
  void NotifyVideoSize();

  // Creates expectations to send an extension message and to disconnect
  // afterwards.
  void SetSendMessageAndDisconnectExpectation(const std::string& message_type);

  // Message loop that will process all ClientSession tasks.
  base::MessageLoop message_loop_;

  // AutoThreadTaskRunner on which |client_session_| will be run.
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Used to run |message_loop_| after each test, until no objects remain that
  // require it.
  base::RunLoop run_loop_;

  // HostExtensions to pass when creating the ClientSession. Caller retains
  // ownership of the HostExtensions themselves.
  std::vector<HostExtension*> extensions_;

  // ClientSession instance under test.
  scoped_ptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Storage for values to be returned by the protocol::Session mock.
  scoped_ptr<SessionConfig> session_config_;
  const std::string client_jid_;

  // Stubs returned to |client_session_| components by |connection_|.
  MockClientStub client_stub_;

  // ClientSession owns |connection_| but tests need it to inject fake events.
  protocol::FakeConnectionToClient* connection_;

  scoped_ptr<FakeDesktopEnvironmentFactory> desktop_environment_factory_;
};

void ClientSessionTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.task_runner(), run_loop_.QuitClosure());

  desktop_environment_factory_.reset(new FakeDesktopEnvironmentFactory());
  session_config_ = SessionConfig::ForTest();
}

void ClientSessionTest::TearDown() {
  if (client_session_) {
    client_session_->DisconnectSession(protocol::OK);
    client_session_.reset();
    desktop_environment_factory_.reset();
  }

  // Clear out |task_runner_| reference so the loop can quit, and run it until
  // it does.
  task_runner_ = nullptr;
  run_loop_.Run();
}

void ClientSessionTest::CreateClientSession() {
  // Mock protocol::Session APIs called directly by ClientSession.
  scoped_ptr<protocol::MockSession> session(new MockSession());
  EXPECT_CALL(*session, config()).WillRepeatedly(ReturnRef(*session_config_));
  EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(client_jid_));

  // Mock protocol::ConnectionToClient APIs called directly by ClientSession.
  // HostStub is not touched by ClientSession, so we can safely pass nullptr.
  scoped_ptr<protocol::FakeConnectionToClient> connection(
      new protocol::FakeConnectionToClient(std::move(session)));
  connection->set_client_stub(&client_stub_);
  connection_ = connection.get();

  client_session_.reset(new ClientSession(
      &session_event_handler_,
      task_runner_,  // Audio thread.
      std::move(connection), desktop_environment_factory_.get(),
      base::TimeDelta(), nullptr, extensions_));
}

void ClientSessionTest::ConnectClientSession() {
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));

  // Stubs should be set only after connection is authenticated.
  EXPECT_FALSE(connection_->clipboard_stub());
  EXPECT_FALSE(connection_->input_stub());

  client_session_->OnConnectionAuthenticated(client_session_->connection());

  EXPECT_TRUE(connection_->clipboard_stub());
  EXPECT_TRUE(connection_->input_stub());

  client_session_->OnConnectionChannelsConnected(client_session_->connection());
}

void ClientSessionTest::NotifyVideoSize() {
  connection_->last_video_stream()->size_callback().Run(
      webrtc::DesktopSize(protocol::FakeDesktopCapturer::kWidth,
                          protocol::FakeDesktopCapturer::kHeight));
}

TEST_F(ClientSessionTest, DisableInputs) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::KeyEvent> key_events;
  input_injector->set_key_events(&key_events);
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);
  std::vector<protocol::ClipboardEvent> clipboard_events;
  input_injector->set_clipboard_events(&clipboard_events);

  // Inject test events that are expected to be injected.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("a"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(100, 101));

  // Disable input.
  client_session_->SetDisableInputs(true);

  // These event shouldn't get though to the input injector.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("b"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(false, 2));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(200, 201));

  // Enable input again.
  client_session_->SetDisableInputs(false);
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("c"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 3));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(300, 301));

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(300, 301));

  EXPECT_EQ(4U, key_events.size());
  EXPECT_THAT(key_events[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events[1], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events[2], EqualsKeyEvent(3, true));
  EXPECT_THAT(key_events[3], EqualsKeyEvent(3, false));

  EXPECT_EQ(2U, clipboard_events.size());
  EXPECT_THAT(clipboard_events[0],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "a"));
  EXPECT_THAT(clipboard_events[1],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "c"));
}

TEST_F(ClientSessionTest, LocalInputTest) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();


  std::vector<protocol::MouseEvent> mouse_events;
  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events);

  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(100, 101));

#if !defined(OS_WIN)
  // The OS echoes the injected event back.
  client_session_->OnLocalMouseMoved(webrtc::DesktopVector(100, 101));
#endif  // !defined(OS_WIN)

  // This one should get throught as well.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(200, 201));

  // Now this is a genuine local event.
  client_session_->OnLocalMouseMoved(webrtc::DesktopVector(100, 101));

  // This one should be blocked because of the previous local input event.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(300, 301));

  // Verify that we've received correct set of mouse events.
  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(200, 201));

  // TODO(jamiewalch): Verify that remote inputs are re-enabled
  // eventually (via dependency injection, not sleep!)
}

TEST_F(ClientSessionTest, RestoreEventState) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::KeyEvent> key_events;
  input_injector->set_key_events(&key_events);
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);

  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);
  connection_->input_stub()->InjectMouseEvent(mousedown);

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, true));
  EXPECT_THAT(mouse_events[1],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, false));

  EXPECT_EQ(4U, key_events.size());
  EXPECT_THAT(key_events[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events[1], EqualsKeyEvent(2, true));
  EXPECT_THAT(key_events[2], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events[3], EqualsKeyEvent(2, false));
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  std::vector<protocol::MouseEvent> mouse_events;
  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events);

  int input_x[3] = {-999, 100, 999};
  int expected_x[3] = {0, 100, protocol::FakeDesktopCapturer::kWidth - 1};
  int input_y[3] = {-999, 50, 999};
  int expected_y[3] = {0, 50, protocol::FakeDesktopCapturer::kHeight - 1};

  protocol::MouseEvent expected_event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      mouse_events.clear();
      connection_->input_stub()->InjectMouseEvent(
          MakeMouseMoveEvent(input_x[i], input_y[j]));

      EXPECT_EQ(1U, mouse_events.size());
      EXPECT_THAT(mouse_events[0],
                  EqualsMouseMoveEvent(expected_x[i], expected_y[j]));
    }
  }
}

TEST_F(ClientSessionTest, NoGnubbyAuth) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  // Host should ignore gnubby messages when gnubby is disabled.
  client_session_->DeliverClientMessage(message);
}

TEST_F(ClientSessionTest, EnableGnubbyAuth) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  // Lifetime controlled by object under test.
  MockGnubbyAuthHandler* gnubby_auth_handler = new MockGnubbyAuthHandler();
  client_session_->SetGnubbyAuthHandlerForTesting(gnubby_auth_handler);

  // Host should ignore gnubby messages when gnubby is disabled.
  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  EXPECT_CALL(*gnubby_auth_handler, DeliverClientMessage(_)).Times(1);
  client_session_->DeliverClientMessage(message);
}

// Verifies that the client's video pipeline can be reset mid-session.
TEST_F(ClientSessionTest, ResetVideoPipeline) {
  CreateClientSession();
  ConnectClientSession();
  NotifyVideoSize();

  client_session_->ResetVideoPipeline();
}

// Verifies that clients can have extensions registered, resulting in the
// correct capabilities being reported, and messages delivered correctly.
// The extension system is tested more extensively in the
// HostExtensionSessionManager unit-tests.
TEST_F(ClientSessionTest, Extensions) {
  // Configure fake extensions for testing.
  FakeExtension extension1("ext1", "cap1");
  extensions_.push_back(&extension1);
  FakeExtension extension2("ext2", "");
  extensions_.push_back(&extension2);
  FakeExtension extension3("ext3", "cap3");
  extensions_.push_back(&extension3);

  // Set the second extension to request to modify the video pipeline.
  extension2.set_steal_video_capturer(true);

  // Verify that the ClientSession reports the correct capabilities.
  EXPECT_CALL(client_stub_, SetCapabilities(EqCapabilities("cap1 cap3")));

  CreateClientSession();
  ConnectClientSession();

  testing::Mock::VerifyAndClearExpectations(&client_stub_);

  // Mimic the client reporting an overlapping set of capabilities.
  protocol::Capabilities capabilities_message;
  capabilities_message.set_capabilities("cap1 cap4 default");
  client_session_->SetCapabilities(capabilities_message);

  // Simulate OnCreateVideoEncoder() which is normally called by the
  // ConnectionToClient when creating the video stream.
  scoped_ptr<VideoEncoder> encoder(new VideoEncoderVerbatim());
  connection_->event_handler()->OnCreateVideoEncoder(&encoder);

  // Verify that the correct extension messages are delivered, and dropped.
  protocol::ExtensionMessage message1;
  message1.set_type("ext1");
  message1.set_data("data");
  client_session_->DeliverClientMessage(message1);
  protocol::ExtensionMessage message3;
  message3.set_type("ext3");
  message3.set_data("data");
  client_session_->DeliverClientMessage(message3);
  protocol::ExtensionMessage message4;
  message4.set_type("ext4");
  message4.set_data("data");
  client_session_->DeliverClientMessage(message4);

  base::RunLoop().RunUntilIdle();

  // ext1 was instantiated and sent a message, and did not wrap anything.
  EXPECT_TRUE(extension1.was_instantiated());
  EXPECT_TRUE(extension1.has_handled_message());
  EXPECT_FALSE(extension1.has_wrapped_video_encoder());

  // ext2 was instantiated but not sent a message, and wrapped video encoder.
  EXPECT_TRUE(extension2.was_instantiated());
  EXPECT_FALSE(extension2.has_handled_message());
  EXPECT_TRUE(extension2.has_wrapped_video_encoder());

  // ext3 was sent a message but not instantiated.
  EXPECT_FALSE(extension3.was_instantiated());
}

// Verifies that an extension can "steal" the video capture, in which case no
// VideoFramePump is instantiated.
TEST_F(ClientSessionTest, StealVideoCapturer) {
  FakeExtension extension("ext1", "cap1");
  extensions_.push_back(&extension);

  // Verify that the ClientSession reports the correct capabilities.
  EXPECT_CALL(client_stub_, SetCapabilities(EqCapabilities("cap1")));

  CreateClientSession();
  ConnectClientSession();

  // Mimic the client reporting an overlapping set of capabilities.
  protocol::Capabilities capabilities_message;
  capabilities_message.set_capabilities("cap1");
  client_session_->SetCapabilities(capabilities_message);

  extension.set_steal_video_capturer(true);
  client_session_->ResetVideoPipeline();

  base::RunLoop().RunUntilIdle();

  // Verify that video control messages received while there is no video
  // scheduler active won't crash things.
  protocol::VideoControl video_control;
  video_control.set_enable(false);
  video_control.set_lossless_encode(true);
  video_control.set_lossless_color(true);
  client_session_->ControlVideo(video_control);

  // TODO(wez): Find a way to verify that the ClientSession never captures any
  // frames in this case.

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  // ext1 was instantiated and wrapped the video capturer.
  EXPECT_TRUE(extension.was_instantiated());
  EXPECT_TRUE(extension.has_wrapped_video_capturer());
}

}  // namespace remoting
