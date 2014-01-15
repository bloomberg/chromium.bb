// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

namespace remoting {

namespace {

enum Messages {
  kMessageCrash = ChromotingDaemonMsg_Crash::ID,
  kMessageConfiguration = ChromotingDaemonNetworkMsg_Configuration::ID,
  kMessageConnectTerminal = ChromotingNetworkHostMsg_ConnectTerminal::ID,
  kMessageDisconnectTerminal = ChromotingNetworkHostMsg_DisconnectTerminal::ID,
  kMessageTerminalDisconnected =
      ChromotingDaemonNetworkMsg_TerminalDisconnected::ID
};

// Provides a public constructor allowing the test to create instances of
// DesktopSession directly.
class FakeDesktopSession : public DesktopSession {
 public:
  FakeDesktopSession(DaemonProcess* daemon_process, int id);
  virtual ~FakeDesktopSession();

  virtual void SetScreenResolution(
      const ScreenResolution& resolution) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDesktopSession);
};

class MockDaemonProcess : public DaemonProcess {
 public:
  MockDaemonProcess(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);
  virtual ~MockDaemonProcess();

  virtual scoped_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) OVERRIDE;

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void SendToNetwork(IPC::Message* message) OVERRIDE;

  MOCK_METHOD1(Received, void(const IPC::Message&));
  MOCK_METHOD1(Sent, void(const IPC::Message&));

  MOCK_METHOD3(OnDesktopSessionAgentAttached,
               bool(int, base::ProcessHandle, IPC::PlatformFileForTransit));

  MOCK_METHOD1(DoCreateDesktopSessionPtr, DesktopSession*(int));
  MOCK_METHOD1(DoCrashNetworkProcess, void(const tracked_objects::Location&));
  MOCK_METHOD0(LaunchNetworkProcess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemonProcess);
};

FakeDesktopSession::FakeDesktopSession(DaemonProcess* daemon_process, int id)
    : DesktopSession(daemon_process, id) {
}

FakeDesktopSession::~FakeDesktopSession() {
}

MockDaemonProcess::MockDaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(caller_task_runner, io_task_runner, stopped_callback) {
}

MockDaemonProcess::~MockDaemonProcess() {
}

scoped_ptr<DesktopSession> MockDaemonProcess::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  return scoped_ptr<DesktopSession>(DoCreateDesktopSessionPtr(terminal_id));
}

bool MockDaemonProcess::OnMessageReceived(const IPC::Message& message) {
  // Notify the mock method.
  Received(message);

  // Call the actual handler.
  return DaemonProcess::OnMessageReceived(message);
}

void MockDaemonProcess::SendToNetwork(IPC::Message* message) {
  // Notify the mock method.
  Sent(*message);
  delete message;
}

}  // namespace

class DaemonProcessTest : public testing::Test {
 public:
  DaemonProcessTest();
  virtual ~DaemonProcessTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // DaemonProcess mocks
  DesktopSession* DoCreateDesktopSession(int terminal_id);
  void DoCrashNetworkProcess(const tracked_objects::Location& location);
  void LaunchNetworkProcess();

  // Deletes |daemon_process_|.
  void DeleteDaemonProcess();

  // Quits |message_loop_|.
  void QuitMessageLoop();

  void StartDaemonProcess();

  const DaemonProcess::DesktopSessionList& desktop_sessions() const {
    return daemon_process_->desktop_sessions();
  }

 protected:
  base::MessageLoopForIO message_loop_;

  scoped_ptr<MockDaemonProcess> daemon_process_;
  int terminal_id_;
};

DaemonProcessTest::DaemonProcessTest() : terminal_id_(0) {
}

DaemonProcessTest::~DaemonProcessTest() {
}

void DaemonProcessTest::SetUp() {
  scoped_refptr<AutoThreadTaskRunner> task_runner = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(),
      base::Bind(&DaemonProcessTest::QuitMessageLoop,
                 base::Unretained(this)));
  daemon_process_.reset(
      new MockDaemonProcess(task_runner, task_runner,
                            base::Bind(&DaemonProcessTest::DeleteDaemonProcess,
                                       base::Unretained(this))));

  // Set up daemon process mocks.
  EXPECT_CALL(*daemon_process_, DoCreateDesktopSessionPtr(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::DoCreateDesktopSession));
  EXPECT_CALL(*daemon_process_, DoCrashNetworkProcess(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::DoCrashNetworkProcess));
  EXPECT_CALL(*daemon_process_, LaunchNetworkProcess())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::LaunchNetworkProcess));
}

void DaemonProcessTest::TearDown() {
  daemon_process_->Stop();
  message_loop_.Run();
}

DesktopSession* DaemonProcessTest::DoCreateDesktopSession(int terminal_id) {
  return new FakeDesktopSession(daemon_process_.get(), terminal_id);
}

void DaemonProcessTest::DoCrashNetworkProcess(
    const tracked_objects::Location& location) {
  daemon_process_->SendToNetwork(
      new ChromotingDaemonMsg_Crash(location.function_name(),
                                    location.file_name(),
                                    location.line_number()));
}

void DaemonProcessTest::LaunchNetworkProcess() {
  terminal_id_ = 0;
  daemon_process_->OnChannelConnected(0);
}

void DaemonProcessTest::DeleteDaemonProcess() {
  daemon_process_.reset();
}

void DaemonProcessTest::QuitMessageLoop() {
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

void DaemonProcessTest::StartDaemonProcess() {
  // DaemonProcess::Initialize() sets up the config watcher that this test does
  // not support. Launch the process directly.
  daemon_process_->LaunchNetworkProcess();
}

MATCHER_P(Message, type, "") {
  return arg.type() == static_cast<uint32>(type);
}

TEST_F(DaemonProcessTest, OpenClose) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
}

TEST_F(DaemonProcessTest, CallCloseDesktopSession) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
}

// Sends two CloseDesktopSession messages and expects the second one to be
// ignored.
TEST_F(DaemonProcessTest, DoubleDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
}

// Tries to close an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageCrash)))
      .WillOnce(InvokeWithoutArgs(this,
                                  &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));

  StartDaemonProcess();

  int id = terminal_id_++;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

// Tries to open an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidConnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageCrash)))
      .WillOnce(InvokeWithoutArgs(this,
                                  &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

}  // namespace remoting
