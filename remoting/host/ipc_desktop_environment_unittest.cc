// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/fake_desktop_capturer.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::Return;
using testing::ReturnRef;

namespace remoting {

namespace {

// Receives messages sent from the network process to the daemon.
class FakeDaemonSender : public IPC::Sender {
 public:
  FakeDaemonSender() {}
  virtual ~FakeDaemonSender() {}

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  MOCK_METHOD3(ConnectTerminal, void(int, const ScreenResolution&, bool));
  MOCK_METHOD1(DisconnectTerminal, void(int));
  MOCK_METHOD2(SetScreenResolution, void(int, const ScreenResolution&));

 private:
  void OnMessageReceived(const IPC::Message& message);

  DISALLOW_COPY_AND_ASSIGN(FakeDaemonSender);
};

// Receives messages sent from the desktop process to the daemon.
class MockDaemonListener : public IPC::Listener {
 public:
  MockDaemonListener() {}
  virtual ~MockDaemonListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  MOCK_METHOD1(OnDesktopAttached, void(IPC::PlatformFileForTransit));
  MOCK_METHOD1(OnChannelConnected, void(int32));
  MOCK_METHOD0(OnChannelError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemonListener);
};

bool FakeDaemonSender::Send(IPC::Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

void FakeDaemonSender::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FakeDaemonSender, message)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_ConnectTerminal,
                        ConnectTerminal)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_DisconnectTerminal,
                        DisconnectTerminal)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_SetScreenResolution,
                        SetScreenResolution)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);
}

bool MockDaemonListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockDaemonListener, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_DesktopAttached,
                        OnDesktopAttached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);
  return handled;
}

}  // namespace

class IpcDesktopEnvironmentTest : public testing::Test {
 public:
  IpcDesktopEnvironmentTest();
  virtual ~IpcDesktopEnvironmentTest();

  virtual void SetUp() OVERRIDE;

  void ConnectTerminal(int terminal_id,
                       const ScreenResolution& resolution,
                       bool virtual_terminal);
  void DisconnectTerminal(int terminal_id);

  // Creates a DesktopEnvironment with a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Creates a dummy InputInjector, to mock
  // DesktopEnvironment::CreateInputInjector().
  InputInjector* CreateInputInjector();

  // Creates a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  webrtc::DesktopCapturer* CreateVideoCapturer();

  // Creates a MockMouseCursorMonitor, to mock
  // DesktopEnvironment::CreateMouseCursorMonitor
  webrtc::MouseCursorMonitor* CreateMouseCursorMonitor();

  void DeleteDesktopEnvironment();

  // Forwards |event| to |clipboard_stub_|.
  void ReflectClipboardEvent(const protocol::ClipboardEvent& event);

 protected:
  // Creates and starts an instance of desktop process object.
  void CreateDesktopProcess();

  // Destroys the desktop process object created by CreateDesktopProcess().
  void DestoyDesktopProcess();

  void OnDisconnectCallback();

  // Invoked when ChromotingDesktopDaemonMsg_DesktopAttached message is
  // received.
  void OnDesktopAttached(IPC::PlatformFileForTransit desktop_pipe);

  // The main message loop.
  base::MessageLoopForUI message_loop_;

  // Runs until |desktop_session_proxy_| is connected to the desktop.
  scoped_ptr<base::RunLoop> setup_run_loop_;

  // Runs until there are references to |task_runner_|.
  base::RunLoop main_run_loop_;

  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  std::string client_jid_;

  // Clipboard stub that receives clipboard events from the desktop process.
  protocol::ClipboardStub* clipboard_stub_;

  // The daemons's end of the daemon-to-desktop channel.
  scoped_ptr<IPC::ChannelProxy> desktop_channel_;

  // Name of the daemon-to-desktop channel.
  std::string desktop_channel_name_;

  // Delegate that is passed to |desktop_channel_|.
  MockDaemonListener desktop_listener_;

  FakeDaemonSender daemon_channel_;

  scoped_ptr<IpcDesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;

  // The IPC input injector.
  scoped_ptr<InputInjector> input_injector_;

  // The IPC screen controls.
  scoped_ptr<ScreenControls> screen_controls_;

  // The IPC screen capturer.
  scoped_ptr<webrtc::DesktopCapturer> video_capturer_;

  // Represents the desktop process running in a user session.
  scoped_ptr<DesktopProcess> desktop_process_;

  // Input injector owned by |desktop_process_|.
  MockInputInjector* remote_input_injector_;

  // The last |terminal_id| passed to ConnectTermina();
  int terminal_id_;

  webrtc::MockScreenCapturerCallback desktop_capturer_callback_;

  MockClientSessionControl client_session_control_;
  base::WeakPtrFactory<ClientSessionControl> client_session_control_factory_;
};

IpcDesktopEnvironmentTest::IpcDesktopEnvironmentTest()
    : client_jid_("user@domain/rest-of-jid"),
      clipboard_stub_(NULL),
      remote_input_injector_(NULL),
      terminal_id_(-1),
      client_session_control_factory_(&client_session_control_) {
}

IpcDesktopEnvironmentTest::~IpcDesktopEnvironmentTest() {
}

void IpcDesktopEnvironmentTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), main_run_loop_.QuitClosure());

  io_task_runner_ = AutoThread::CreateWithType(
      "IPC thread", task_runner_, base::MessageLoop::TYPE_IO);

  setup_run_loop_.reset(new base::RunLoop());

  // Set expectation that the DaemonProcess will send DesktopAttached message
  // once it is ready.
  EXPECT_CALL(desktop_listener_, OnChannelConnected(_))
      .Times(AnyNumber());
  EXPECT_CALL(desktop_listener_, OnDesktopAttached(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &IpcDesktopEnvironmentTest::OnDesktopAttached));
  EXPECT_CALL(desktop_listener_, OnChannelError())
      .Times(AnyNumber())
      .WillOnce(Invoke(this,
                       &IpcDesktopEnvironmentTest::DestoyDesktopProcess));

  // Intercept requests to connect and disconnect a terminal.
  EXPECT_CALL(daemon_channel_, ConnectTerminal(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &IpcDesktopEnvironmentTest::ConnectTerminal));
  EXPECT_CALL(daemon_channel_, DisconnectTerminal(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &IpcDesktopEnvironmentTest::DisconnectTerminal));

  EXPECT_CALL(client_session_control_, client_jid())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(client_jid_));
  EXPECT_CALL(client_session_control_, DisconnectSession())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));
  EXPECT_CALL(client_session_control_, OnLocalMouseMoved(_))
      .Times(0);
  EXPECT_CALL(client_session_control_, SetDisableInputs(_))
      .Times(0);

  // Create a desktop environment instance.
  desktop_environment_factory_.reset(new IpcDesktopEnvironmentFactory(
      task_runner_,
      task_runner_,
      task_runner_,
      io_task_runner_,
      &daemon_channel_));
  desktop_environment_ = desktop_environment_factory_->Create(
      client_session_control_factory_.GetWeakPtr());

  screen_controls_ = desktop_environment_->CreateScreenControls();

  // Create the input injector.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Create the screen capturer.
  video_capturer_ =
      desktop_environment_->CreateVideoCapturer();

  desktop_environment_->SetCapabilities(std::string());
}

void IpcDesktopEnvironmentTest::ConnectTerminal(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  EXPECT_NE(terminal_id_, terminal_id);

  terminal_id_ = terminal_id;
  CreateDesktopProcess();
}

void IpcDesktopEnvironmentTest::DisconnectTerminal(int terminal_id) {
  EXPECT_EQ(terminal_id_, terminal_id);

  // The IPC desktop environment is fully destroyed now. Release the remaining
  // task runners.
  desktop_environment_factory_.reset();
}

DesktopEnvironment* IpcDesktopEnvironmentTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr())
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjectorPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(
          this, &IpcDesktopEnvironmentTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateScreenControlsPtr())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(
          this, &IpcDesktopEnvironmentTest::CreateVideoCapturer));
  EXPECT_CALL(*desktop_environment, CreateMouseCursorMonitorPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(
          this, &IpcDesktopEnvironmentTest::CreateMouseCursorMonitor));
  EXPECT_CALL(*desktop_environment, GetCapabilities())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_))
      .Times(AtMost(1));

  // Let tests know that the remote desktop environment is created.
  message_loop_.PostTask(FROM_HERE, setup_run_loop_->QuitClosure());

  return desktop_environment;
}

InputInjector* IpcDesktopEnvironmentTest::CreateInputInjector() {
  EXPECT_TRUE(remote_input_injector_ == NULL);
  remote_input_injector_ = new testing::StrictMock<MockInputInjector>();

  EXPECT_CALL(*remote_input_injector_, StartPtr(_));
  return remote_input_injector_;
}

webrtc::DesktopCapturer* IpcDesktopEnvironmentTest::CreateVideoCapturer() {
  return new FakeDesktopCapturer();
}

webrtc::MouseCursorMonitor*
IpcDesktopEnvironmentTest::CreateMouseCursorMonitor() {
  return new FakeMouseCursorMonitor();
}

void IpcDesktopEnvironmentTest::DeleteDesktopEnvironment() {
  input_injector_.reset();
  screen_controls_.reset();
  video_capturer_.reset();

  // Trigger DisconnectTerminal().
  desktop_environment_.reset();
}

void IpcDesktopEnvironmentTest::ReflectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  clipboard_stub_->InjectClipboardEvent(event);
}

void IpcDesktopEnvironmentTest::CreateDesktopProcess() {
  EXPECT_TRUE(task_runner_.get());
  EXPECT_TRUE(io_task_runner_.get());

  // Create the daemon end of the daemon-to-desktop channel.
  desktop_channel_name_ = IPC::Channel::GenerateUniqueRandomChannelID();
  desktop_channel_ =
      IPC::ChannelProxy::Create(IPC::ChannelHandle(desktop_channel_name_),
                                IPC::Channel::MODE_SERVER,
                                &desktop_listener_,
                                io_task_runner_.get());

  // Create and start the desktop process.
  desktop_process_.reset(new DesktopProcess(task_runner_,
                                            io_task_runner_,
                                            desktop_channel_name_));

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory(
      new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(
          this, &IpcDesktopEnvironmentTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(desktop_process_->Start(
      desktop_environment_factory.PassAs<DesktopEnvironmentFactory>()));
}

void IpcDesktopEnvironmentTest::DestoyDesktopProcess() {
  desktop_channel_.reset();
  if (desktop_process_) {
    desktop_process_->OnChannelError();
    desktop_process_.reset();
  }
  remote_input_injector_ = NULL;
}

void IpcDesktopEnvironmentTest::OnDisconnectCallback() {
  DeleteDesktopEnvironment();
}

void IpcDesktopEnvironmentTest::OnDesktopAttached(
    IPC::PlatformFileForTransit desktop_pipe) {

  // Instruct DesktopSessionProxy to connect to the network-to-desktop pipe.
  desktop_environment_factory_->OnDesktopSessionAgentAttached(
      terminal_id_, base::GetCurrentProcessHandle(), desktop_pipe);
}

// Runs until the desktop is attached and exits immediately after that.
TEST_F(IpcDesktopEnvironmentTest, Basic) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Stop the test.
  DeleteDesktopEnvironment();

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests that the video capturer receives a frame over IPC.
TEST_F(IpcDesktopEnvironmentTest, CaptureFrame) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Stop the test when the first frame is captured.
  EXPECT_CALL(desktop_capturer_callback_, OnCaptureCompleted(_))
      .WillOnce(DoAll(
          DeleteArg<0>(),
          InvokeWithoutArgs(
              this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment)));

  // Capture a single frame.
  video_capturer_->Capture(webrtc::DesktopRegion());

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests that attaching to a new desktop works.
TEST_F(IpcDesktopEnvironmentTest, Reattach) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Create and start a new desktop process object.
  setup_run_loop_.reset(new base::RunLoop());
  DestoyDesktopProcess();
  CreateDesktopProcess();
  setup_run_loop_->Run();

  // Stop the test.
  DeleteDesktopEnvironment();

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests injection of clipboard events.
TEST_F(IpcDesktopEnvironmentTest, InjectClipboardEvent) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  clipboard_stub_ = clipboard_stub.get();

  // Stop the test when a clipboard event is received from the desktop process.
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single clipboard event.
  EXPECT_CALL(*remote_input_injector_, InjectClipboardEvent(_))
      .Times(1)
      .WillOnce(Invoke(this,
                       &IpcDesktopEnvironmentTest::ReflectClipboardEvent));

  // Send a clipboard event.
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("a");
  input_injector_->InjectClipboardEvent(event);

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests injection of key events.
TEST_F(IpcDesktopEnvironmentTest, InjectKeyEvent) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single key event.
  EXPECT_CALL(*remote_input_injector_, InjectKeyEvent(_))
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Send a key event.
  protocol::KeyEvent event;
  event.set_usb_keycode(0x070004);
  event.set_pressed(true);
  input_injector_->InjectKeyEvent(event);

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests injection of text events.
TEST_F(IpcDesktopEnvironmentTest, InjectTextEvent) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single text event.
  EXPECT_CALL(*remote_input_injector_, InjectTextEvent(_))
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Send a text event.
  protocol::TextEvent event;
  event.set_text("hello");
  input_injector_->InjectTextEvent(event);

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests injection of mouse events.
TEST_F(IpcDesktopEnvironmentTest, InjectMouseEvent) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single mouse event.
  EXPECT_CALL(*remote_input_injector_, InjectMouseEvent(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Send a mouse event.
  protocol::MouseEvent event;
  event.set_x(0);
  event.set_y(0);
  input_injector_->InjectMouseEvent(event);

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests that setting the desktop resolution works.
TEST_F(IpcDesktopEnvironmentTest, SetScreenResolution) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the input injector and screen capturer.
  input_injector_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&desktop_capturer_callback_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  EXPECT_CALL(daemon_channel_, SetScreenResolution(_, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Change the desktop resolution.
  screen_controls_->SetScreenResolution(ScreenResolution(
      webrtc::DesktopSize(100, 100),
      webrtc::DesktopVector(96, 96)));

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

}  // namespace remoting
