// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedHandle;
using testing::_;
using testing::AnyNumber;
using testing::CreateFunctor;
using testing::DoAll;
using testing::Expectation;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::ReturnPointee;

namespace remoting {

namespace {

const char kIpcSecurityDescriptor[] = "D:(A;;GA;;;AU)";

class MockProcessLauncherDelegate
    : public WorkerProcessLauncher::Delegate {
 public:
  MockProcessLauncherDelegate() {}
  virtual ~MockProcessLauncherDelegate() {}

  virtual DWORD GetProcessId() const OVERRIDE {
    return const_cast<MockProcessLauncherDelegate*>(this)->GetProcessId();
  }

  virtual bool IsPermanentError(int failure_count) const OVERRIDE {
    return const_cast<MockProcessLauncherDelegate*>(this)->IsPermanentError(
        failure_count);
  }

  // IPC::Sender implementation.
  MOCK_METHOD1(Send, bool(IPC::Message*));

  // WorkerProcessLauncher::Delegate implementation
  MOCK_METHOD0(GetProcessId, DWORD());
  MOCK_METHOD1(IsPermanentError, bool(int));
  MOCK_METHOD1(KillProcess, void(DWORD));
  MOCK_METHOD2(LaunchProcess, bool(IPC::Listener*, ScopedHandle*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessLauncherDelegate);
};

class MockIpcDelegate
    : public WorkerProcessIpcDelegate {
 public:
  MockIpcDelegate() {}
  virtual ~MockIpcDelegate() {}

  // WorkerProcessIpcDelegate implementation
  MOCK_METHOD1(OnChannelConnected, void(int32));
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message&));
  MOCK_METHOD0(OnPermanentError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIpcDelegate);
};

}  // namespace

class WorkerProcessLauncherTest
    : public testing::Test,
      public IPC::Listener {
 public:
  WorkerProcessLauncherTest();
  virtual ~WorkerProcessLauncherTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // IPC::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WorkerProcessLauncher::Delegate mocks
  void KillProcess(DWORD exit_code);
  bool LaunchProcess(IPC::Listener* delegate,
                     ScopedHandle* process_exit_event_out);
  bool LaunchProcessAndConnect(IPC::Listener* delegate,
                               ScopedHandle* process_exit_event_out);
  bool FailLaunchAndStopWorker(IPC::Listener* delegate,
                               ScopedHandle* process_exit_event_out);

  void ConnectChannel();
  void DisconnectChannel();

  // Starts the worker.
  void StartWorker();

  // Stops the worker.
  void StopWorker();

  // Quits |message_loop_|.
  void QuitMainMessageLoop();

 protected:
  MessageLoop message_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  MockIpcDelegate ipc_delegate_;
  scoped_ptr<MockProcessLauncherDelegate> launcher_delegate_;

  // The name of the IPC channel.
  std::string channel_name_;

  // Client and server ends of the IPC channel.
  scoped_ptr<IPC::ChannelProxy> channel_client_;
  scoped_ptr<IPC::ChannelProxy> channel_server_;

  // Returned as the worker process PID to the launcher.
  DWORD client_pid_;

  // The worker process launcher.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // The event signalling termination of the worker process.
  ScopedHandle process_exit_event_;

  // True if a permanent error condition should be emulated.
  bool permanent_error_;
};


WorkerProcessLauncherTest::WorkerProcessLauncherTest()
    : message_loop_(MessageLoop::TYPE_IO),
      client_pid_(GetCurrentProcessId()),
      permanent_error_(false) {
}

WorkerProcessLauncherTest::~WorkerProcessLauncherTest() {
}

void WorkerProcessLauncherTest::SetUp() {
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(),
      base::Bind(&WorkerProcessLauncherTest::QuitMainMessageLoop,
                 base::Unretained(this)));

  // Set up process launcher delegate
  launcher_delegate_.reset(new MockProcessLauncherDelegate());
  EXPECT_CALL(*launcher_delegate_, Send(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*launcher_delegate_, GetProcessId())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnPointee(&client_pid_));
  EXPECT_CALL(*launcher_delegate_, IsPermanentError(_))
      .Times(AnyNumber())
      .WillRepeatedly(ReturnPointee(&permanent_error_));
  EXPECT_CALL(*launcher_delegate_, KillProcess(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::KillProcess));

  // Set up IPC delegate.
  EXPECT_CALL(ipc_delegate_, OnMessageReceived(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
}

void WorkerProcessLauncherTest::TearDown() {
}

bool WorkerProcessLauncherTest::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void WorkerProcessLauncherTest::KillProcess(DWORD exit_code) {
  BOOL result = SetEvent(process_exit_event_);
  EXPECT_TRUE(result);
}

bool WorkerProcessLauncherTest::LaunchProcess(
    IPC::Listener* delegate,
    ScopedHandle* process_exit_event_out) {
  process_exit_event_.Set(CreateEvent(NULL, TRUE, FALSE, NULL));
  if (!process_exit_event_.IsValid())
    return false;

  channel_name_ = IPC::Channel::GenerateUniqueRandomChannelID();
  ScopedHandle pipe;
  if (!CreateIpcChannel(channel_name_, kIpcSecurityDescriptor, &pipe)) {
    return false;
  }

  // Wrap the pipe into an IPC channel.
  channel_server_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(pipe),
      IPC::Channel::MODE_SERVER,
      delegate,
      task_runner_));

  return DuplicateHandle(GetCurrentProcess(),
                         process_exit_event_,
                         GetCurrentProcess(),
                         process_exit_event_out->Receive(),
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS) != FALSE;
}

bool WorkerProcessLauncherTest::LaunchProcessAndConnect(
    IPC::Listener* delegate,
    ScopedHandle* process_exit_event_out) {
  if (!LaunchProcess(delegate, process_exit_event_out))
    return false;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WorkerProcessLauncherTest::ConnectChannel,
                 base::Unretained(this)));
  return true;
}

bool WorkerProcessLauncherTest::FailLaunchAndStopWorker(
    IPC::Listener* delegate,
    ScopedHandle* process_exit_event_out) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WorkerProcessLauncherTest::StopWorker,
                 base::Unretained(this)));
  return false;
}

void WorkerProcessLauncherTest::ConnectChannel() {
  channel_client_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(channel_name_),
      IPC::Channel::MODE_CLIENT,
      this,
      task_runner_));
}

void WorkerProcessLauncherTest::DisconnectChannel() {
  channel_client_.reset();
}

void WorkerProcessLauncherTest::StartWorker() {
  launcher_.reset(new WorkerProcessLauncher(
      task_runner_, launcher_delegate_.Pass(), &ipc_delegate_));
}

void WorkerProcessLauncherTest::StopWorker() {
  launcher_.reset();
  DisconnectChannel();
  channel_name_.clear();
  channel_server_.reset();
  task_runner_ = NULL;
}

void WorkerProcessLauncherTest::QuitMainMessageLoop() {
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

TEST_F(WorkerProcessLauncherTest, Start) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::LaunchProcess));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
      .Times(0);
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  StopWorker();
  message_loop_.Run();
}

// Starts and connects to the worker process. Expect OnChannelConnected to be
// called.
TEST_F(WorkerProcessLauncherTest, StartAndConnect) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();
}

// Kills the worker process after the 1st connect and expects it to be
// restarted.
TEST_F(WorkerProcessLauncherTest, Restart) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));
  Expectation first_connect =
      EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(InvokeWithoutArgs(CreateFunctor(
              this, &WorkerProcessLauncherTest::KillProcess, CONTROL_C_EXIT)))
          .WillOnce(InvokeWithoutArgs(this,
                                      &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();
}

// Drops the IPC channel to the worker process after the 1st connect and expects
// the worker process to be restarted.
TEST_F(WorkerProcessLauncherTest, DropIpcChannel) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  Expectation first_connect =
      EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(InvokeWithoutArgs(
              this, &WorkerProcessLauncherTest::DisconnectChannel))
          .WillOnce(InvokeWithoutArgs(
              this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();
}

// Returns a permanent error exit code and expects OnPermanentError() to be
// invoked.
TEST_F(WorkerProcessLauncherTest, PermanentError) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(CreateFunctor(
          this, &WorkerProcessLauncherTest::KillProcess, CONTROL_C_EXIT)));
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(1)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));

  permanent_error_ = true;
  StartWorker();
  message_loop_.Run();
}

// Returns invalid client PID and expects the connection to be rejected.
TEST_F(WorkerProcessLauncherTest, InvalidClientPid) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(2)
      .WillOnce(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect))
      .WillOnce(Invoke(
          this, &WorkerProcessLauncherTest::FailLaunchAndStopWorker));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected(_))
      .Times(0);
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  client_pid_ = GetCurrentProcessId() + 4;
  StartWorker();
  message_loop_.Run();
}

}  // namespace remoting
