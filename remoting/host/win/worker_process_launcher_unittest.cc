// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  // WorkerProcessLauncher::Delegate implementation
  MOCK_METHOD0(GetExitCode, DWORD());
  MOCK_METHOD1(KillProcess, void(DWORD));
  MOCK_METHOD2(LaunchProcess, bool(const std::string&, ScopedHandle*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessLauncherDelegate);
};

class MockIpcDelegate
    : public WorkerProcessIpcDelegate {
 public:
  MockIpcDelegate() {}
  virtual ~MockIpcDelegate() {}

  // WorkerProcessIpcDelegate implementation
  MOCK_METHOD0(OnChannelConnected, void());
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
  bool LaunchProcess(const std::string& channel_name,
                     ScopedHandle* process_exit_event_out);
  bool LaunchProcessAndConnect(const std::string& channel_name,
                               ScopedHandle* process_exit_event_out);

  void ConnectTo(const std::string& channel_name);
  void Disconnect();

  // Starts the worker.
  void StartWorker();

  // Stops the worker.
  void StopWorker();

  // Quits |message_loop_|.
  void QuitMainMessageLoop();

 protected:
  MessageLoop message_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Exit code of the worker process.
  DWORD exit_code_;

  MockIpcDelegate ipc_delegate_;
  scoped_ptr<MockProcessLauncherDelegate> launcher_delegate_;

  // Client end of the IPC channel.
  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  // The worker process launcher.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // The event signalling termination of the worker process.
  ScopedHandle process_exit_event_;
};


WorkerProcessLauncherTest::WorkerProcessLauncherTest()
    : message_loop_(MessageLoop::TYPE_IO) {
}

WorkerProcessLauncherTest::~WorkerProcessLauncherTest() {
}

void WorkerProcessLauncherTest::SetUp() {
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(),
      base::Bind(&WorkerProcessLauncherTest::QuitMainMessageLoop,
                 base::Unretained(this)));

  exit_code_ = STILL_ACTIVE;

  // Set up process launcher delegate
  launcher_delegate_.reset(new MockProcessLauncherDelegate());
  EXPECT_CALL(*launcher_delegate_, GetExitCode())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnPointee(&exit_code_));
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
  exit_code_ = exit_code;
  BOOL result = SetEvent(process_exit_event_);
  EXPECT_TRUE(result);
}

bool WorkerProcessLauncherTest::LaunchProcess(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  return LaunchProcessAndConnect("", process_exit_event_out);
}

bool WorkerProcessLauncherTest::LaunchProcessAndConnect(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  process_exit_event_.Set(CreateEvent(NULL, TRUE, FALSE, NULL));
  if (!process_exit_event_.IsValid())
    return false;

  if (!channel_name.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WorkerProcessLauncherTest::ConnectTo,
                   base::Unretained(this),
                   channel_name));
  }

  exit_code_ = STILL_ACTIVE;
  return DuplicateHandle(GetCurrentProcess(),
                         process_exit_event_,
                         GetCurrentProcess(),
                         process_exit_event_out->Receive(),
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS) != FALSE;
}

void WorkerProcessLauncherTest::ConnectTo(const std::string& channel_name) {
  ipc_channel_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(channel_name),
      IPC::Channel::MODE_CLIENT,
      this,
      task_runner_));
}

void WorkerProcessLauncherTest::Disconnect() {
  ipc_channel_.reset();
}

void WorkerProcessLauncherTest::StartWorker() {
  launcher_.reset(new WorkerProcessLauncher(task_runner_,
                                            task_runner_,
                                            launcher_delegate_.Pass(),
                                            &ipc_delegate_,
                                            kIpcSecurityDescriptor));
}

void WorkerProcessLauncherTest::StopWorker() {
  launcher_.reset();
  Disconnect();
  task_runner_ = NULL;
}

void WorkerProcessLauncherTest::QuitMainMessageLoop() {
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

TEST_F(WorkerProcessLauncherTest, Start) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::LaunchProcess));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected())
      .Times(0);
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  StopWorker();
  message_loop_.Run();

  EXPECT_EQ(exit_code_, CONTROL_C_EXIT);
}

// Starts and connects to the worker process. Expect OnChannelConnected to be
// called.
TEST_F(WorkerProcessLauncherTest, StartAndConnect) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(ipc_delegate_, OnChannelConnected())
      .Times(1)
      .WillOnce(Invoke(this, &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();

  EXPECT_EQ(exit_code_, CONTROL_C_EXIT);
}

// Kills the worker process after the 1st connect and expects it to be
// restarted.
TEST_F(WorkerProcessLauncherTest, Restart) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));
  Expectation first_connect =
      EXPECT_CALL(ipc_delegate_, OnChannelConnected())
          .Times(2)
          .WillOnce(Invoke(CreateFunctor(
              this, &WorkerProcessLauncherTest::KillProcess, CONTROL_C_EXIT)))
          .WillOnce(Invoke(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();

  EXPECT_EQ(exit_code_, CONTROL_C_EXIT);
}

// Drops the IPC channel to the worker process after the 1st connect and expects
// the worker process to be restarted.
TEST_F(WorkerProcessLauncherTest, DropIpcChannel) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  Expectation first_connect =
      EXPECT_CALL(ipc_delegate_, OnChannelConnected())
          .Times(2)
          .WillOnce(Invoke(this, &WorkerProcessLauncherTest::Disconnect))
          .WillOnce(Invoke(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(0);

  StartWorker();
  message_loop_.Run();

  EXPECT_EQ(exit_code_, CONTROL_C_EXIT);
}

// Returns a permanent error exit code and expects OnPermanentError() to be
// invoked.
TEST_F(WorkerProcessLauncherTest, PermanentError) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  base::Closure terminate_permanently =
      base::Bind(&WorkerProcessLauncherTest::KillProcess,
                 base::Unretained(this),
                 kInvalidHostConfigurationExitCode);
  EXPECT_CALL(ipc_delegate_, OnChannelConnected())
      .Times(1)
      .WillRepeatedly(Invoke(&terminate_permanently, &base::Closure::Run));

  EXPECT_CALL(ipc_delegate_, OnPermanentError())
      .Times(1)
      .WillOnce(Invoke(this, &WorkerProcessLauncherTest::StopWorker));

  StartWorker();
  message_loop_.Run();

  EXPECT_EQ(exit_code_, kInvalidHostConfigurationExitCode);
}

}  // namespace remoting
