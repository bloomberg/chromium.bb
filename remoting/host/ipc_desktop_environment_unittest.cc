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
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
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

 protected:
  void OnDisconnectCallback();

  // Invoked when ChromotingDesktopDaemonMsg_DesktopAttached message is
  // received.
  void OnDesktopAttached(IPC::PlatformFileForTransit desktop_pipe);

  // Invoked when the daemon-to-desktop channel is closed.
  void OnDesktopSessionClosed();

  // The main message loop.
  MessageLoop message_loop_;

  // Runs until |desktop_session_proxy_| is connected to the desktop.
  base::RunLoop setup_run_loop_;

  // Runs until there are references to |task_runner_|.
  base::RunLoop main_run_loop_;

  scoped_refptr<AutoThreadTaskRunner> task_runner_;

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

  // Points to the DesktopSessionProxy instance created by
  // IpdDesktopEnvironment.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  media::MockScreenCapturerDelegate screen_capturer_delegate_;
};

IpcDesktopEnvironmentTest::IpcDesktopEnvironmentTest()
    : message_loop_(MessageLoop::TYPE_UI),
      connector_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

IpcDesktopEnvironmentTest::~IpcDesktopEnvironmentTest() {
}

void IpcDesktopEnvironmentTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), main_run_loop_.QuitClosure());

  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType("IPC thread", task_runner_,
                                 MessageLoop::TYPE_IO);

  // Set expectation that the DaemonProcess will send DesktopAttached message
  // once it is ready.
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(Invoke(this, &IpcDesktopEnvironmentTest::OnDesktopAttached));
  EXPECT_CALL(daemon_listener_, OnChannelError())
      .Times(AnyNumber())
      .WillOnce(Invoke(this,
                       &IpcDesktopEnvironmentTest::OnDesktopSessionClosed));

  // Create the daemon end of the daemon-to-desktop channel.
  daemon_channel_name_ = IPC::Channel::GenerateUniqueRandomChannelID();
  daemon_channel_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(daemon_channel_name_),
      IPC::Channel::MODE_SERVER,
      &daemon_listener_,
      io_task_runner));

  // Create an IpcDesktopEnvironment instance.
  desktop_environment_.reset(new IpcDesktopEnvironment(
      task_runner_, io_task_runner, "user@domain/rest-of-jid",
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
  EXPECT_TRUE(!desktop_process_);
  EXPECT_TRUE(!desktop_session_proxy_.get());
  EXPECT_TRUE(task_runner_.get());

  desktop_session_proxy_ = desktop_session_proxy;

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

  return desktop_environment;
}

EventExecutor* IpcDesktopEnvironmentTest::CreateEventExecutor() {
  MockEventExecutor* event_executor = new MockEventExecutor();
  EXPECT_CALL(*event_executor, StartPtr(_));
  return event_executor;
}

media::ScreenCapturer* IpcDesktopEnvironmentTest::CreateVideoCapturer() {
  return new media::ScreenCapturerFake();
}

void IpcDesktopEnvironmentTest::DeleteDesktopEnvironment() {
  desktop_environment_.reset();
  event_executor_.reset();
  video_capturer_.reset();
}

void IpcDesktopEnvironmentTest::OnDisconnectCallback() {
  NOTIMPLEMENTED();
}

void IpcDesktopEnvironmentTest::OnDesktopAttached(
    IPC::PlatformFileForTransit desktop_pipe) {
  // Instruct DesktopSessionProxy to connect to the network-to-desktop pipe.
  EXPECT_TRUE(desktop_session_proxy_->AttachToDesktop(
      base::GetCurrentProcessHandle(), desktop_pipe));

  // Let the test know that |desktop_session_proxy_| is attahced to the desktop.
  setup_run_loop_.Quit();
}

void IpcDesktopEnvironmentTest::OnDesktopSessionClosed() {
  daemon_channel_.reset();
  desktop_process_.reset();
}

TEST_F(IpcDesktopEnvironmentTest, Basic) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_.Run();

  // Stop the test.
  DeleteDesktopEnvironment();

  task_runner_ = NULL;
  main_run_loop_.Run();
}

TEST_F(IpcDesktopEnvironmentTest, CaptureFrame) {
  scoped_ptr<protocol::MockClipboardStub> clipboard_stub(
      new protocol::MockClipboardStub());
  EXPECT_CALL(*clipboard_stub, InjectClipboardEvent(_))
      .Times(0);

  // Start the event executor and screen capturer.
  event_executor_->Start(clipboard_stub.PassAs<protocol::ClipboardStub>());
  video_capturer_->Start(&screen_capturer_delegate_);

  // Run the message loop until the desktop is attached.
  setup_run_loop_.Run();

  // Stop the test when the first frame is captured.
  EXPECT_CALL(screen_capturer_delegate_, OnCaptureCompleted(_))
      .WillOnce(InvokeWithoutArgs(
          this, &IpcDesktopEnvironmentTest::DeleteDesktopEnvironment));

  // Capture a single frame.
  video_capturer_->CaptureFrame();

  task_runner_ = NULL;
  main_run_loop_.Run();
}

}  // namespace remoting
