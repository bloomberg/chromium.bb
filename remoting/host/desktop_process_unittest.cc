// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_process.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/fake_screen_capturer.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::InSequence;
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

class MockNetworkListener : public IPC::Listener {
 public:
  MockNetworkListener() {}
  virtual ~MockNetworkListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  MOCK_METHOD1(OnChannelConnected, void(int32));
  MOCK_METHOD0(OnChannelError, void());

  MOCK_METHOD0(OnDesktopEnvironmentCreated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkListener);
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

bool MockNetworkListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  // TODO(alexeypa): handle received messages here.

  EXPECT_TRUE(handled);
  return handled;
}

}  // namespace

class DesktopProcessTest : public testing::Test {
 public:
  DesktopProcessTest();
  virtual ~DesktopProcessTest();

  // testing::Test overrides
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // MockDaemonListener mocks
  void ConnectNetworkChannel(IPC::PlatformFileForTransit desktop_process);
  void OnDesktopAttached(IPC::PlatformFileForTransit desktop_process);

  // Creates a DesktopEnvironment with a fake webrtc::ScreenCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Creates a dummy InputInjector, to mock
  // DesktopEnvironment::CreateInputInjector().
  InputInjector* CreateInputInjector();

  // Creates a fake webrtc::ScreenCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  webrtc::ScreenCapturer* CreateVideoCapturer();

  // Disconnects the daemon-to-desktop channel causing the desktop process to
  // exit.
  void DisconnectChannels();

  // Posts DisconnectChannels() to |message_loop_|.
  void PostDisconnectChannels();

  // Runs the desktop process code in a separate thread.
  void RunDesktopProcess();

  // Creates the desktop process and sends a crash request to it.
  void RunDeathTest();

  // Sends a crash request to the desktop process.
  void SendCrashRequest();

  // Requests the desktop process to start the desktop session agent.
  void SendStartSessionAgent();

 protected:
  // The daemon's end of the daemon-to-desktop channel.
  scoped_ptr<IPC::ChannelProxy> daemon_channel_;

  // Delegate that is passed to |daemon_channel_|.
  MockDaemonListener daemon_listener_;

  // Runs the daemon's end of the channel.
  base::MessageLoopForUI message_loop_;

  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // The network's end of the network-to-desktop channel.
  scoped_ptr<IPC::ChannelProxy> network_channel_;

  // Delegate that is passed to |network_channel_|.
  MockNetworkListener network_listener_;
};

DesktopProcessTest::DesktopProcessTest() {}

DesktopProcessTest::~DesktopProcessTest() {
}

void DesktopProcessTest::SetUp() {
}

void DesktopProcessTest::TearDown() {
}

void DesktopProcessTest::ConnectNetworkChannel(
    IPC::PlatformFileForTransit desktop_process) {

#if defined(OS_POSIX)
  IPC::ChannelHandle channel_handle(std::string(), desktop_process);
#elif defined(OS_WIN)
  IPC::ChannelHandle channel_handle(desktop_process);
#endif  // defined(OS_WIN)

  network_channel_ = IPC::ChannelProxy::Create(channel_handle,
                                               IPC::Channel::MODE_CLIENT,
                                               &network_listener_,
                                               io_task_runner_.get());
}

void DesktopProcessTest::OnDesktopAttached(
    IPC::PlatformFileForTransit desktop_process) {
#if defined(OS_POSIX)
    DCHECK(desktop_process.auto_close);

    base::File closer(IPC::PlatformFileForTransitToFile(desktop_process));
#endif  // defined(OS_POSIX)
}

DesktopEnvironment* DesktopProcessTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr())
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjectorPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateScreenControlsPtr())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateVideoCapturer));
  EXPECT_CALL(*desktop_environment, GetCapabilities())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_))
      .Times(AtMost(1));

  // Notify the test that the desktop environment has been created.
  network_listener_.OnDesktopEnvironmentCreated();
  return desktop_environment;
}

InputInjector* DesktopProcessTest::CreateInputInjector() {
  MockInputInjector* input_injector = new MockInputInjector();
  EXPECT_CALL(*input_injector, StartPtr(_));
  return input_injector;
}

webrtc::ScreenCapturer* DesktopProcessTest::CreateVideoCapturer() {
  return new FakeScreenCapturer();
}

void DesktopProcessTest::DisconnectChannels() {
  daemon_channel_.reset();
  network_channel_.reset();
  io_task_runner_ = NULL;
}

void DesktopProcessTest::PostDisconnectChannels() {
  message_loop_.PostTask(FROM_HERE, base::Bind(
      &DesktopProcessTest::DisconnectChannels, base::Unretained(this)));
}

void DesktopProcessTest::RunDesktopProcess() {
  base::RunLoop run_loop;
  base::Closure quit_ui_task_runner = base::Bind(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      message_loop_.message_loop_proxy(),
      FROM_HERE, run_loop.QuitClosure());
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), quit_ui_task_runner);

  io_task_runner_ = AutoThread::CreateWithType(
      "IPC thread", ui_task_runner, base::MessageLoop::TYPE_IO);

  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();
  daemon_channel_ = IPC::ChannelProxy::Create(IPC::ChannelHandle(channel_name),
                                              IPC::Channel::MODE_SERVER,
                                              &daemon_listener_,
                                              io_task_runner_.get());

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory(
      new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &DesktopProcessTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  DesktopProcess desktop_process(ui_task_runner, io_task_runner_, channel_name);
  EXPECT_TRUE(desktop_process.Start(
      desktop_environment_factory.PassAs<DesktopEnvironmentFactory>()));

  ui_task_runner = NULL;
  run_loop.Run();
}

void DesktopProcessTest::RunDeathTest() {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(DoAll(
          Invoke(this, &DesktopProcessTest::OnDesktopAttached),
          InvokeWithoutArgs(this, &DesktopProcessTest::SendCrashRequest)));

  RunDesktopProcess();
}

void DesktopProcessTest::SendCrashRequest() {
  tracked_objects::Location location = FROM_HERE;
  daemon_channel_->Send(new ChromotingDaemonMsg_Crash(
      location.function_name(), location.file_name(), location.line_number()));
}

void DesktopProcessTest::SendStartSessionAgent() {
  network_channel_->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "user@domain/rest-of-jid", ScreenResolution(), false));
}

// Launches the desktop process and waits when it connects back.
TEST_F(DesktopProcessTest, Basic) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(DoAll(
          Invoke(this, &DesktopProcessTest::OnDesktopAttached),
          InvokeWithoutArgs(this, &DesktopProcessTest::DisconnectChannels)));

  RunDesktopProcess();
}

// Launches the desktop process and waits when it connects back.
TEST_F(DesktopProcessTest, ConnectNetworkChannel) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(Invoke(this, &DesktopProcessTest::ConnectNetworkChannel));
  EXPECT_CALL(network_listener_, OnChannelConnected(_))
      .WillOnce(InvokeWithoutArgs(
          this, &DesktopProcessTest::DisconnectChannels));

  RunDesktopProcess();
}

// Launches the desktop process, waits when it connects back and starts
// the desktop session agent.
TEST_F(DesktopProcessTest, StartSessionAgent) {
  {
    InSequence s;
    EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
    EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
        .WillOnce(Invoke(this, &DesktopProcessTest::ConnectNetworkChannel));
    EXPECT_CALL(network_listener_, OnChannelConnected(_))
        .WillOnce(InvokeWithoutArgs(
            this, &DesktopProcessTest::SendStartSessionAgent));
  }

  EXPECT_CALL(network_listener_, OnDesktopEnvironmentCreated())
      .WillOnce(InvokeWithoutArgs(
          this, &DesktopProcessTest::PostDisconnectChannels));

  RunDesktopProcess();
}

// Run the desktop process and ask it to crash.
TEST_F(DesktopProcessTest, DeathTest) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";

  EXPECT_DEATH(RunDeathTest(), "");
}

}  // namespace remoting
