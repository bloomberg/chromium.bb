// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_screen_capturer.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

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
using testing::AtMost;
using testing::CreateFunctor;
using testing::DeleteArg;
using testing::DoAll;
using testing::Expectation;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;
using testing::StrEq;
using testing::StrictMock;

namespace {

const char kDefaultTestCapability[] = "default";

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
  client_session->OnLocalMouseMoved(
      webrtc::DesktopVector(event.x(), event.y()));
}

ACTION_P2(SetGnubbyAuthHandlerForTesting, client_session, gnubby_auth_handler) {
  client_session->SetGnubbyAuthHandlerForTesting(gnubby_auth_handler);
}

ACTION_P2(DeliverClientMessage, client_session, message) {
  client_session->DeliverClientMessage(message);
}

ACTION_P2(AddHostCapabilities, client_session, capability) {
  client_session->AddHostCapabilities(capability);
}

// Matches a |protocol::Capabilities| argument against a list of capabilities
// formatted as a space-separated string.
MATCHER_P(EqCapabilities, expected_capabilities, "") {
  if (!arg.has_capabilities())
    return false;

  std::vector<std::string> words_args;
  std::vector<std::string> words_expected;
  Tokenize(arg.capabilities(), " ", &words_args);
  Tokenize(expected_capabilities, " ", &words_expected);
  std::sort(words_args.begin(), words_args.end());
  std::sort(words_expected.begin(), words_expected.end());
  return words_args == words_expected;
}

// |HostExtension| implementation that can handle an extension message type and
// provide capabilities.
class FakeExtension : public HostExtension {
 public:
  FakeExtension(const std::string& message_type,
                const std::string& capabilities);
  virtual ~FakeExtension();

  virtual std::string GetCapabilities() OVERRIDE;
  virtual scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSession* client_session) OVERRIDE;

  bool message_handled() {
    return message_handled_;
  }

 private:
  class FakeExtensionSession : public HostExtensionSession {
   public:
    FakeExtensionSession(FakeExtension* extension);
    virtual ~FakeExtensionSession();

    virtual bool OnExtensionMessage(
        ClientSession* client_session,
        const protocol::ExtensionMessage& message) OVERRIDE;

   private:
    FakeExtension* extension_;
  };

  std::string message_type_;
  std::string capabilities_;
  bool message_handled_;
};

typedef std::vector<HostExtension*> HostExtensionList;

void CreateExtensionSessions(const HostExtensionList& extensions,
                             ClientSession* client_session) {
  for (HostExtensionList::const_iterator extension = extensions.begin();
       extension != extensions.end(); ++extension) {
    scoped_ptr<HostExtensionSession> extension_session =
        (*extension)->CreateExtensionSession(client_session);
    if (extension_session)
      client_session->AddExtensionSession(extension_session.Pass());
  }
}

}  // namespace

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() : client_jid_("user@domain/rest-of-jid") {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Disconnects the client session.
  void DisconnectClientSession();

  // Stops and releases the ClientSession, allowing the MessageLoop to quit.
  void StopClientSession();

 protected:
  // Creates a DesktopEnvironment with a fake webrtc::ScreenCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Returns |input_injector_| created and initialized by SetUp(), to mock
  // DesktopEnvironment::CreateInputInjector().
  InputInjector* CreateInputInjector();

  // Creates a fake webrtc::ScreenCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  webrtc::ScreenCapturer* CreateVideoCapturer();

  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected. This effectively enables
  // the input pipe line and starts video capturing.
  void ConnectClientSession();

  // Creates expectations to send an extension message and to disconnect
  // afterwards.
  void SetSendMessageAndDisconnectExpectation(const std::string& message_type);

  // Invoked when the last reference to the AutoThreadTaskRunner has been
  // released and quits the message loop to finish the test.
  void QuitMainMessageLoop();

  // Message loop passed to |client_session_| to perform all functions on.
  base::MessageLoop message_loop_;

  // ClientSession instance under test.
  scoped_ptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Storage for values to be returned by the protocol::Session mock.
  SessionConfig session_config_;
  const std::string client_jid_;

  // Stubs returned to |client_session_| components by |connection_|.
  MockClientStub client_stub_;
  MockVideoStub video_stub_;

  // DesktopEnvironment owns |input_injector_|, but input injection tests need
  // to express expectations on it.
  scoped_ptr<MockInputInjector> input_injector_;

  // ClientSession owns |connection_| but tests need it to inject fake events.
  MockConnectionToClient* connection_;

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory_;
};

FakeExtension::FakeExtension(const std::string& message_type,
                             const std::string& capabilities)
    : message_type_(message_type),
      capabilities_(capabilities),
      message_handled_(false) {
}

FakeExtension::~FakeExtension() {}

std::string FakeExtension::GetCapabilities() {
  return capabilities_;
}

scoped_ptr<HostExtensionSession> FakeExtension::CreateExtensionSession(
    ClientSession* client_session) {
  return scoped_ptr<HostExtensionSession>(new FakeExtensionSession(this));
}

FakeExtension::FakeExtensionSession::FakeExtensionSession(
    FakeExtension* extension)
    : extension_(extension) {
}

FakeExtension::FakeExtensionSession::~FakeExtensionSession() {}

bool FakeExtension::FakeExtensionSession::OnExtensionMessage(
    ClientSession* client_session,
    const protocol::ExtensionMessage& message) {
  if (message.type() == extension_->message_type_) {
    extension_->message_handled_ = true;
    return true;
  }
  return false;
}

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
  EXPECT_CALL(*desktop_environment_factory_, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  input_injector_.reset(new MockInputInjector());

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

  client_session_.reset(new ClientSession(
      &session_event_handler_,
      ui_task_runner, // Audio thread.
      ui_task_runner, // Input thread.
      ui_task_runner, // Capture thread.
      ui_task_runner, // Encode thread.
      ui_task_runner, // Network thread.
      ui_task_runner, // UI thread.
      connection.PassAs<protocol::ConnectionToClient>(),
      desktop_environment_factory_.get(),
      base::TimeDelta(),
      NULL));

  // By default, client will report the same capabilities as the host.
  EXPECT_CALL(client_stub_, SetCapabilities(_))
      .Times(AtMost(1))
      .WillOnce(Invoke(client_session_.get(), &ClientSession::SetCapabilities));
}

void ClientSessionTest::TearDown() {
  // Verify that the client session has been stopped.
  EXPECT_TRUE(!client_session_);
}

void ClientSessionTest::DisconnectClientSession() {
  client_session_->DisconnectSession();
  // MockSession won't trigger OnConnectionClosed, so fake it.
  client_session_->OnConnectionClosed(client_session_->connection(),
                                      protocol::OK);
}

void ClientSessionTest::StopClientSession() {
  client_session_.reset();

  desktop_environment_factory_.reset();
}

DesktopEnvironment* ClientSessionTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr())
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjectorPtr())
      .WillOnce(Invoke(this, &ClientSessionTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateScreenControlsPtr())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr())
      .WillOnce(Invoke(this, &ClientSessionTest::CreateVideoCapturer));
  EXPECT_CALL(*desktop_environment, GetCapabilities())
      .Times(AtMost(1))
       .WillOnce(Return(kDefaultTestCapability));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_))
      .Times(AtMost(1));

  return desktop_environment;
}

InputInjector* ClientSessionTest::CreateInputInjector() {
  EXPECT_TRUE(input_injector_);
  return input_injector_.release();
}

webrtc::ScreenCapturer* ClientSessionTest::CreateVideoCapturer() {
  return new FakeScreenCapturer();
}

void ClientSessionTest::ConnectClientSession() {
  client_session_->OnConnectionAuthenticated(client_session_->connection());
  client_session_->OnConnectionChannelsConnected(client_session_->connection());
}

void ClientSessionTest::SetSendMessageAndDisconnectExpectation(
    const std::string& message_type) {
  protocol::ExtensionMessage message;
  message.set_type(message_type);
  message.set_data("data");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
      .WillOnce(Return(true));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
          DeliverClientMessage(client_session_.get(), message),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
}

void ClientSessionTest::QuitMainMessageLoop() {
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
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

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
      .WillOnce(DoAll(
          // This event should get through to the clipboard stub.
          InjectClipboardEvent(connection_, clipboard_event2),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          // This event should not get through to the clipboard stub,
          // because the client has disconnected.
          InjectClipboardEvent(connection_, clipboard_event3),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*input_injector_, InjectClipboardEvent(EqualsClipboardEvent(
      kMimeTypeTextUtf8, "b")))
      .InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_))
      .InSequence(s);

  // This event should not get through to the clipboard stub,
  // because the client isn't authenticated yet.
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event1);

  ConnectClientSession();
  message_loop_.Run();
}

namespace {

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

}  // namespace

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

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
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
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(2, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(2, false)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseEvent(200, 201)))
      .InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_))
      .InSequence(s);

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

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
      .WillOnce(DoAll(
          // This event should get through to the input stub.
          InjectMouseEvent(connection_, mouse_event1),
#if !defined(OS_WIN)
          // The OS echoes the injected event back.
          LocalMouseMoved(client_session_.get(), mouse_event1),
#endif  // !defined(OS_WIN)
          // This one should get throught as well.
          InjectMouseEvent(connection_, mouse_event2),
          // Now this is a genuine local event.
          LocalMouseMoved(client_session_.get(), mouse_event1),
          // This one should be blocked because of the previous  local input
          // event.
          InjectMouseEvent(connection_, mouse_event3),
          // TODO(jamiewalch): Verify that remote inputs are re-enabled
          // eventually (via dependency injection, not sleep!)
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseEvent(100, 101)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseEvent(200, 201)))
      .InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_))
      .InSequence(s);

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

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
      .WillOnce(DoAll(
          InjectKeyEvent(connection_, key1),
          InjectKeyEvent(connection_, key2),
          InjectMouseEvent(connection_, mousedown),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(1, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(2, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(1, false)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsUsbEvent(2, false)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, false)))
      .InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_))
      .InSequence(s);

  ConnectClientSession();
  message_loop_.Run();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_))
      .After(authenticated);

  Expectation connected = authenticated;

  int input_x[3] = { -999, 100, 999 };
  int expected_x[3] = { 0, 100, FakeScreenCapturer::kWidth - 1 };
  int input_y[3] = { -999, 50, 999 };
  int expected_y[3] = { 0, 50, FakeScreenCapturer::kHeight - 1 };

  protocol::MouseEvent expected_event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      protocol::MouseEvent injected_event;
      injected_event.set_x(input_x[i]);
      injected_event.set_y(input_y[j]);

      if (i == 0 && j == 0) {
        // Inject the 1st event once a video packet has been received.
        connected =
            EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
                .After(connected)
                .WillOnce(InjectMouseEvent(connection_, injected_event));
      } else {
        // Every next event is injected once the previous event has been
        // received.
        connected =
            EXPECT_CALL(*input_injector_,
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
  EXPECT_CALL(*input_injector_,
              InjectMouseEvent(EqualsMouseEvent(expected_event.x(),
                                                expected_event.y())))
      .After(connected)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  ConnectClientSession();
  message_loop_.Run();
}

TEST_F(ClientSessionTest, NoGnubbyAuth) {
  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
           DeliverClientMessage(client_session_.get(), message),
           InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
           InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
  message_loop_.Run();
}

TEST_F(ClientSessionTest, EnableGnubbyAuth) {
  // Lifetime controlled by object under test.
  MockGnubbyAuthHandler* gnubby_auth_handler = new MockGnubbyAuthHandler();

  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
           SetGnubbyAuthHandlerForTesting(client_session_.get(),
                                          gnubby_auth_handler),
           DeliverClientMessage(client_session_.get(), message),
           InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
           InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*gnubby_auth_handler, DeliverClientMessage(_));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
  message_loop_.Run();
}

// Verifies that messages can be handled by extensions.
TEST_F(ClientSessionTest, ExtensionMessages_MessageHandled) {
  FakeExtension extension1("ext1", "cap1");
  FakeExtension extension2("ext2", "cap2");
  FakeExtension extension3("ext3", "cap3");
  HostExtensionList extensions;
  extensions.push_back(&extension1);
  extensions.push_back(&extension2);
  extensions.push_back(&extension3);

  EXPECT_CALL(session_event_handler_, OnSessionClientCapabilities(_))
      .WillOnce(Invoke(CreateFunctor(&CreateExtensionSessions, extensions)));

  SetSendMessageAndDisconnectExpectation("ext2");
  ConnectClientSession();
  message_loop_.Run();

  EXPECT_FALSE(extension1.message_handled());
  EXPECT_TRUE(extension2.message_handled());
  EXPECT_FALSE(extension3.message_handled());
}

// Verifies that extension messages not handled by extensions don't result in a
// crash.
TEST_F(ClientSessionTest, ExtensionMessages_MessageNotHandled) {
  FakeExtension extension1("ext1", "cap1");
  HostExtensionList extensions;
  extensions.push_back(&extension1);

  EXPECT_CALL(session_event_handler_, OnSessionClientCapabilities(_))
      .WillOnce(Invoke(CreateFunctor(&CreateExtensionSessions, extensions)));

  SetSendMessageAndDisconnectExpectation("extX");
  ConnectClientSession();
  message_loop_.Run();

  EXPECT_FALSE(extension1.message_handled());
}

TEST_F(ClientSessionTest, ReportCapabilities) {
  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
      .WillOnce(DoAll(
          AddHostCapabilities(client_session_.get(), "capX capZ"),
          AddHostCapabilities(client_session_.get(), ""),
          AddHostCapabilities(client_session_.get(), "capY"),
          Return(true)));
  EXPECT_CALL(client_stub_,
              SetCapabilities(EqCapabilities("capX capY capZ default")));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  ConnectClientSession();
  message_loop_.Run();
}

}  // namespace remoting
