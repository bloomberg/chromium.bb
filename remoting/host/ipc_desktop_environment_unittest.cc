// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "media/video/capture/screen/screen_capturer_fake.h"
#include "media/video/capture/screen/screen_capturer_mock_objects.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRegion.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Return;

namespace remoting {

namespace {

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

class IpcDesktopEnvironmentTest
    : public testing::Test,
      public DesktopSessionConnector {
 public:
  IpcDesktopEnvironmentTest();
  virtual ~IpcDesktopEnvironmentTest();

  virtual void SetUp() OVERRIDE;

  // DesktopSessionConnector implementation.
  virtual void ConnectTerminal(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy) OVERRIDE;
  virtual void DisconnectTerminal(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy) OVERRIDE;
  virtual void OnDesktopSessionAgentAttached(
      int terminal_id,
      base::ProcessHandle desktop_process,
      IPC::PlatformFileForTransit desktop_pipe) OVERRIDE;
  virtual void OnTerminalDisconnected(int terminal_id) OVERRIDE;

  // Creates a DesktopEnvironment with a fake media::ScreenCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Creates a dummy EventExecutor, to mock
  // DesktopEnvironment::CreateEventExecutor().
  EventExecutor* CreateEventExecutor();

  // Creates a fake media::ScreenCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  media::ScreenCapturer* CreateVideoCapturer();

  void DeleteDesktopEnvironment();

  // Forwards |event| to |clipboard_stub_|.
  void ReflectClipboardEvent(const protocol::ClipboardEvent& event);

 protected:
  // Creates and starts an instance of desktop process object.
  void CreateDesktopProcess();

  void OnDisconnectCallback();

  // Invoked when ChromotingDesktopDaemonMsg_DesktopAttached message is
  // received.
  void OnDesktopAttached(IPC::PlatformFileForTransit desktop_pipe);

  // Invoked when the daemon-to-desktop channel is closed.
  void OnDesktopSessionClosed();

  // The main message loop.
  MessageLoop message_loop_;

  // Runs until |desktop_session_proxy_| is connected to the desktop.
  scoped_ptr<base::RunLoop> setup_run_loop_;

  // Runs until there are references to |task_runner_|.
  base::RunLoop main_run_loop_;

  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Clipboard stub that receives clipboard events from the desktop process.
  protocol::ClipboardStub* clipboard_stub_;

  // Factory for weak pointers to DesktopSessionConnector interface.
  base::WeakPtrFactory<DesktopSessionConnector> connector_factory_;

  // The daemons's end of the daemon-to-desktop channel.
  scoped_ptr<IPC::ChannelProxy> daemon_channel_;

  // Name of the daemon-to-desktop channel.
  std::string daemon_channel_name_;

  // Delegate that is passed to |daemon_channel_|.
  MockDaemonListener daemon_listener_;

  scoped_ptr<IpcDesktopEnvironment> desktop_environment_;

  // Event executor created by |desktop_environment_|.
  scoped_ptr<EventExecutor> event_executor_;

  // Screen capturer created by |desktop_environment_|.
  scoped_ptr<media::ScreenCapturer> video_capturer_;

  // Represents the desktop process running in a user session.
  scoped_ptr<DesktopProcess> desktop_process_;

  // Event executor owned by |desktop_process_|.
  MockEventExecutor* remote_event_executor_;

  // Points to the DesktopSessionProxy instance created by
  // IpdDesktopEnvironment.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  media::MockScreenCapturerDelegate screen_capturer_delegate_;
};

IpcDesktopEnvironmentTest::IpcDesktopEnvironmentTest()
    : message_loop_(MessageLoop::TYPE_UI),
      clipboard_stub_(NULL),
      connector_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      remote_event_executor_(NULL) {
}

IpcDesktopEnvironmentTest::~IpcDesktopEnvironmentTest() {
}

void IpcDesktopEnvironmentTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), main_run_loop_.QuitClosure());

  io_task_runner_ = AutoThread::CreateWithType("IPC thread", task_runner_,
                                               MessageLoop::TYPE_IO);

  setup_run_loop_.reset(new base::RunLoop());

  // Set expectation that the DaemonProcess will send DesktopAttached message
  // once it is ready.
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_))
      .Times(AnyNumber());
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &IpcDesktopEnvironmentTest::OnDesktopAttached));
  EXPECT_CALL(daemon_listener_, OnChannelError())
      .Times(AnyNumber())
      .WillOnce(Invoke(this,
                       &IpcDesktopEnvironmentTest::OnDesktopSessionClosed));

  // Create an IpcDesktopEnvironment instance.
  desktop_environment_.reset(new IpcDesktopEnvironment(
      task_runner_, io_task_runner_, "user@domain/rest-of-jid",
      base::Bind(&IpcDesktopEnvironmentTest::OnDisconnectCallback,
                 base::Unretained(this)),
      connector_factory_.GetWeakPtr()));

  // Create the event executor.
  event_executor_ =
      desktop_environment_->CreateEventExecutor(task_runner_, task_runner_);

  // Create the screen capturer.
  video_capturer_ =
      desktop_environment_->CreateVideoCapturer(task_runner_, task_runner_);
}

void IpcDesktopEnvironmentTest::ConnectTerminal(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy) {
  EXPECT_TRUE(!desktop_session_proxy_.get());

  desktop_session_proxy_ = desktop_session_proxy;

  CreateDesktopProcess();
}

void IpcDesktopEnvironmentTest::DisconnectTerminal(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy) {
  EXPECT_TRUE(desktop_session_proxy_.get());
  EXPECT_EQ(desktop_session_proxy_.get(), desktop_session_proxy.get());

  desktop_session_proxy_ = NULL;
}

void IpcDesktopEnvironmentTest::OnDesktopSessionAgentAttached(
    int terminal_id,
    base::ProcessHandle desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  NOTIMPLEMENTED();
}

void IpcDesktopEnvironmentTest::OnTerminalDisconnected(int terminal_id) {
  NOTIMPLEMENTED();
}

DesktopEnvironment* IpcDesktopEnvironmentTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr(_))
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateEventExecutorPtr(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          InvokeWithoutArgs(this,
                            &IpcDesktopEnvironmentTest::CreateEventExecutor));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          InvokeWithoutArgs(this,
                            &IpcDesktopEnvironmentTest::CreateVideoCapturer));

  // Let tests know that the remote desktop environment is created.
  message_loop_.PostTask(FROM_HERE, setup_run_loop_->QuitClosure());

  return desktop_environment;
}

EventExecutor* IpcDesktopEnvironmentTest::CreateEventExecutor() {
  EXPECT_TRUE(remote_event_executor_ == NULL);
  remote_event_executor_ = new MockEventExecutor();

  EXPECT_CALL(*remote_event_executor_, StartPtr(_));
  return remote_event_executor_;
}

media::ScreenCapturer* IpcDesktopEnvironmentTest::CreateVideoCapturer() {
  return new media::ScreenCapturerFake();
}

void IpcDesktopEnvironmentTest::DeleteDesktopEnvironment() {
  desktop_environment_.reset();
  event_executor_.reset();
  video_capturer_.reset();
}

void IpcDesktopEnvironmentTest::ReflectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  clipboard_stub_->InjectClipboardEvent(event);
}

void IpcDesktopEnvironmentTest::CreateDesktopProcess() {
  EXPECT_TRUE(task_runner_);
  EXPECT_TRUE(io_task_runner_);

  daemon_channel_.reset();
  if (desktop_process_) {
    desktop_process_->OnChannelError();
    desktop_process_.reset();
    remote_event_executor_ = NULL;
  }

  // Create the daemon end of the daemon-to-desktop channel.
  daemon_channel_name_ = IPC::Channel::GenerateUniqueRandomChannelID();
  daemon_channel_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(daemon_channel_name_),
      IPC::Channel::MODE_SERVER,
      &daemon_listener_,
      io_task_runner_));

  // Create and start the desktop process.
  desktop_process_.reset(new DesktopProcess(task_runner_,
                                            daemon_channel_name_));

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory(
      new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(
          this, &IpcDesktopEnvironmentTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  // TODO(alexeypa): Fix DesktopProcess to use the desktop environment
  // to create the disconnect window instead of directly calling
  // DisconnectWindow::Create(). This will take care of "Uninteresting mock
  // function call" warnings printed when DisconnectWindow::Show() and
  // DisconnectWindow::Hide() are called.
  EXPECT_TRUE(desktop_process_->Start(
      desktop_environment_factory.PassAs<DesktopEnvironmentFactory>()));
}

void IpcDesktopEnvironmentTest::OnDisconnectCallback() {
  NOTIMPLEMENTED();
}

void IpcDesktopEnvironmentTest::OnDesktopAttached(
    IPC::PlatformFileForTransit desktop_pipe) {
  // Instruct DesktopSessionProxy to connect to the network-to-desktop pipe.
  EXPECT_TRUE(desktop_session_proxy_->AttachToDesktop(
      base::GetCurrentProcessHandle(), desktop_pipe));
}

void IpcDesktopEnvironmentTest::OnDesktopSessionClosed() {
  daemon_channel_.reset();
  desktop_process_.reset();
  remote_event_executor_ = NULL;
}

// Runs until the desktop is attached and exits immediately after that.
TEST_F(IpcDesktopEnvironmentTest, Basic) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Event executor should receive no events.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

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

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Event executor should receive no events.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

  // Stop the test when the first frame is captured.
  EXPECT_CALL(screen_capturer_delegate_, OnCaptureCompleted(_))
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Capture a single frame.
  video_capturer_->CaptureFrame();

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

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Create and start a new desktop process object.
  setup_run_loop_.reset(new base::RunLoop());
  CreateDesktopProcess();
  setup_run_loop_->Run();

  // Event executor should receive no events.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

  // Stop the test.
  DeleteDesktopEnvironment();

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

// Tests InvalidateRegion().
TEST_F(IpcDesktopEnvironmentTest, InvalidateRegion) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Event executor should receive no events.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

  // Stop the test when the first frame is captured.
  EXPECT_CALL(screen_capturer_delegate_, OnCaptureCompleted(_))
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Invalidate a region that is larger than the screen.
  SkIRect horizontal_rect = SkIRect::MakeXYWH(
      -100,
      media::ScreenCapturerFake::kHeight / 4,
      media::ScreenCapturerFake::kWidth + 200,
      media::ScreenCapturerFake::kHeight / 2);
  SkIRect vertical_rect = SkIRect::MakeXYWH(
      media::ScreenCapturerFake::kWidth / 4,
      -100,
      media::ScreenCapturerFake::kWidth / 2,
      media::ScreenCapturerFake::kHeight + 200);

  SkRegion invalid_region;
  invalid_region.op(horizontal_rect, SkRegion::kUnion_Op);
  invalid_region.op(vertical_rect, SkRegion::kUnion_Op);
  video_capturer_->InvalidateRegion(invalid_region);

  // Capture a single frame.
  video_capturer_->CaptureFrame();

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

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single clipboard event.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(1)
      .WillOnce(Invoke(this,
                       &IpcDesktopEnvironmentTest::ReflectClipboardEvent));
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

  // Send a clipboard event.
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("a");
  event_executor_->InjectClipboardEvent(event);

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

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single key event.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(AtLeast(1))
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(0);

  // Send a key event.
  protocol::KeyEvent event;
  event.set_usb_keycode(0x070004);
  event.set_pressed(true);
  event_executor_->InjectKeyEvent(event);

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

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_->Run();

  // Expect a single mouse event.
  EXPECT_CALL(*remote_event_executor_, InjectClipboardEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectKeyEvent(_))
      .Times(0);
  EXPECT_CALL(*remote_event_executor_, InjectMouseEvent(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Send a mouse event.
  protocol::MouseEvent event;
  event.set_x(0);
  event.set_y(0);
  event_executor_->InjectMouseEvent(event);

  task_runner_ = NULL;
  io_task_runner_ = NULL;
  main_run_loop_.Run();
}

}  // namespace remoting
