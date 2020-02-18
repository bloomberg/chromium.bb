// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_task_port_provider_mac.h"

#include <mach/mach.h>

#include <vector>

#include "base/mac/scoped_mach_port.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "content/common/child_process.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::_;
using testing::WithArgs;

class MockChildProcess : public mojom::ChildProcess {
 public:
  MOCK_METHOD1(Initialize,
               void(mojo::PendingRemote<mojom::ChildProcessHostBootstrap>));
  MOCK_METHOD0(ProcessShutdown, void());
  MOCK_METHOD1(GetTaskPort, void(GetTaskPortCallback));
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  MOCK_METHOD1(SetIPCLoggingEnabled, void(bool));
#endif
  MOCK_METHOD1(GetBackgroundTracingAgentProvider,
               void(mojo::PendingReceiver<
                    tracing::mojom::BackgroundTracingAgentProvider>));
  MOCK_METHOD0(CrashHungProcess, void());
  MOCK_METHOD2(RunService,
               void(const std::string&,
                    mojo::PendingReceiver<service_manager::mojom::Service>));
  MOCK_METHOD1(BindServiceInterface,
               void(mojo::GenericPendingReceiver receiver));
  MOCK_METHOD1(BindReceiver, void(mojo::GenericPendingReceiver receiver));
};

class ChildProcessTaskPortProviderTest : public testing::Test,
                                         public base::PortProvider::Observer {
 public:
  ChildProcessTaskPortProviderTest()
      : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {
    provider_.AddObserver(this);
    last_seqno_ = GetNotificationPortSequenceNumber();
  }
  ~ChildProcessTaskPortProviderTest() override {
    provider_.RemoveObserver(this);
  }

  void WaitForTaskPort() { event_.Wait(); }

  // There is no observer callback for when a process dies, so use the kernel's
  // sequence number on the notification port receive right to determine if the
  // DEAD_NAME notification has been delivered. If the seqno is different, then
  // assume it is.
  void WaitForNotificationPortSeqnoChange() {
    base::RunLoop run_loop;
    CheckSequenceNumberAndQuitIfChanged(run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  mach_port_urefs_t GetSendRightRefCount(mach_port_t send_right) {
    mach_port_urefs_t refs;
    EXPECT_EQ(KERN_SUCCESS, mach_port_get_refs(mach_task_self(), send_right,
                                               MACH_PORT_RIGHT_SEND, &refs));
    return refs;
  }

  mach_port_urefs_t GetDeadNameRefCount(mach_port_t send_right) {
    mach_port_urefs_t refs;
    EXPECT_EQ(KERN_SUCCESS,
              mach_port_get_refs(mach_task_self(), send_right,
                                 MACH_PORT_RIGHT_DEAD_NAME, &refs));
    return refs;
  }

  // base::PortProvider::Observer:
  void OnReceivedTaskPort(base::ProcessHandle process) override {
    received_processes_.push_back(process);
    event_.Signal();
  }

  ChildProcessTaskPortProvider* provider() { return &provider_; }

  const std::vector<base::ProcessHandle>& received_processes() {
    return received_processes_;
  }

 private:
  mach_port_seqno_t GetNotificationPortSequenceNumber() {
    mach_port_status_t status;
    mach_msg_type_number_t count = sizeof(status);
    kern_return_t kr = mach_port_get_attributes(
        mach_task_self(), provider_.notification_port_.get(),
        MACH_PORT_RECEIVE_STATUS, reinterpret_cast<mach_port_info_t>(&status),
        &count);
    EXPECT_EQ(KERN_SUCCESS, kr);
    return status.mps_seqno;
  }

  void CheckSequenceNumberAndQuitIfChanged(base::OnceClosure quit_closure) {
    mach_port_seqno_t seqno = GetNotificationPortSequenceNumber();
    if (seqno == last_seqno_) {
      task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ChildProcessTaskPortProviderTest::
                             CheckSequenceNumberAndQuitIfChanged,
                         base::Unretained(this), std::move(quit_closure)),
          base::TimeDelta::FromMilliseconds(10));
    } else {
      last_seqno_ = seqno;
      std::move(quit_closure).Run();
    }
  }

  base::test::TaskEnvironment task_environment_;
  ChildProcessTaskPortProvider provider_;
  base::WaitableEvent event_;
  std::vector<base::ProcessHandle> received_processes_;
  mach_port_seqno_t last_seqno_;
};

static constexpr mach_port_t kMachPortNull = MACH_PORT_NULL;

TEST_F(ChildProcessTaskPortProviderTest, InvalidProcess) {
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(99));
}

TEST_F(ChildProcessTaskPortProviderTest, ChildLifecycle) {
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(99));

  // Create a fake task port for the fake process.
  base::mac::ScopedMachReceiveRight receive_right;
  base::mac::ScopedMachSendRight send_right;
  ASSERT_TRUE(base::mac::CreateMachPort(&receive_right, &send_right));

  EXPECT_EQ(1u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right.get()));

  // Return it when the ChildProcess interface is called.
  MockChildProcess child_process;
  EXPECT_CALL(child_process, GetTaskPort(_))
      .WillOnce(WithArgs<0>(
          [&send_right](mojom::ChildProcess::GetTaskPortCallback callback) {
            std::move(callback).Run(mojo::WrapMachPort(send_right.get()));
          }));

  provider()->OnChildProcessLaunched(99, &child_process);

  // Verify that the task-for-pid association is established.
  WaitForTaskPort();
  EXPECT_EQ(std::vector<base::ProcessHandle>{99}, received_processes());
  EXPECT_EQ(receive_right.get(), provider()->TaskForPid(99));

  // References owned by |send_right| and the map.
  EXPECT_EQ(2u, GetSendRightRefCount(provider()->TaskForPid(99)));
  EXPECT_EQ(0u, GetDeadNameRefCount(provider()->TaskForPid(99)));

  // "Kill" the process and verify that the association is deleted.
  receive_right.reset();

  WaitForNotificationPortSeqnoChange();

  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(99));

  // Send rights turned into a dead name right, which is owned by |send_right|.
  EXPECT_EQ(0u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(1u, GetDeadNameRefCount(send_right.get()));
}

// Test is flaky. See https://crbug.com/986288.
TEST_F(ChildProcessTaskPortProviderTest, DISABLED_DeadTaskPort) {
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(6));

  // Create a fake task port for the fake process.
  base::mac::ScopedMachReceiveRight receive_right;
  base::mac::ScopedMachSendRight send_right;
  ASSERT_TRUE(base::mac::CreateMachPort(&receive_right, &send_right));

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool()});

  MockChildProcess child_process;
  EXPECT_CALL(child_process, GetTaskPort(_))
      .WillOnce(
          WithArgs<0>([&task_runner, &receive_right, &send_right](
                          mojom::ChildProcess::GetTaskPortCallback callback) {
            mojo::ScopedHandle mach_handle =
                mojo::WrapMachPort(send_right.get());

            // Destroy the receive right.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&base::mac::ScopedMachReceiveRight::reset,
                               base::Unretained(&receive_right),
                               kMachPortNull));
            // And then return a send right to the now-dead name.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(mach_handle)));
          }));

  provider()->OnChildProcessLaunched(6, &child_process);

  // Create a second fake process.
  base::mac::ScopedMachReceiveRight receive_right2;
  base::mac::ScopedMachSendRight send_right2;
  ASSERT_TRUE(base::mac::CreateMachPort(&receive_right2, &send_right2));

  MockChildProcess child_contol2;
  EXPECT_CALL(child_contol2, GetTaskPort(_))
      .WillOnce(
          WithArgs<0>([&task_runner, &send_right2](
                          mojom::ChildProcess::GetTaskPortCallback callback) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               mojo::WrapMachPort(send_right2.get())));
          }));

  provider()->OnChildProcessLaunched(123, &child_contol2);

  WaitForTaskPort();

  // Verify that the dead name does not register for the process.
  EXPECT_EQ(std::vector<base::ProcessHandle>{123}, received_processes());
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(6));
  EXPECT_EQ(receive_right2.get(), provider()->TaskForPid(123));

  // Clean up the second receive right.
  receive_right2.reset();
  WaitForNotificationPortSeqnoChange();
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(123));
}

TEST_F(ChildProcessTaskPortProviderTest, ReplacePort) {
  EXPECT_EQ(kMachPortNull, provider()->TaskForPid(42));

  // Create a fake task port for the fake process.
  base::mac::ScopedMachReceiveRight receive_right;
  base::mac::ScopedMachSendRight send_right;
  ASSERT_TRUE(base::mac::CreateMachPort(&receive_right, &send_right));

  EXPECT_EQ(1u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right.get()));

  // Return it when the ChildProcess interface is called.
  MockChildProcess child_process;
  EXPECT_CALL(child_process, GetTaskPort(_))
      .Times(2)
      .WillRepeatedly(WithArgs<0>(
          [&receive_right](mojom::ChildProcess::GetTaskPortCallback callback) {
            std::move(callback).Run(mojo::WrapMachPort(receive_right.get()));
          }));

  provider()->OnChildProcessLaunched(42, &child_process);
  WaitForTaskPort();

  EXPECT_EQ(2u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right.get()));

  provider()->OnChildProcessLaunched(42, &child_process);
  WaitForTaskPort();

  EXPECT_EQ(2u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right.get()));

  // Verify that the task-for-pid association is established.
  std::vector<base::ProcessHandle> expected_receive{42, 42};
  EXPECT_EQ(expected_receive, received_processes());
  EXPECT_EQ(receive_right.get(), provider()->TaskForPid(42));

  // Now simulate PID reuse by replacing the task port with a new one.
  base::mac::ScopedMachReceiveRight receive_right2;
  base::mac::ScopedMachSendRight send_right2;
  ASSERT_TRUE(base::mac::CreateMachPort(&receive_right2, &send_right2));
  EXPECT_EQ(1u, GetSendRightRefCount(send_right2.get()));

  MockChildProcess child_process2;
  EXPECT_CALL(child_process2, GetTaskPort(_))
      .WillOnce(
          [&send_right2](mojom::ChildProcess::GetTaskPortCallback callback) {
            std::move(callback).Run(mojo::WrapMachPort(send_right2.get()));
          });

  provider()->OnChildProcessLaunched(42, &child_process2);
  WaitForTaskPort();

  // Reference to |send_right| is dropped from the map and is solely owned
  // by |send_right|.
  EXPECT_EQ(1u, GetSendRightRefCount(send_right.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right.get()));

  EXPECT_EQ(2u, GetSendRightRefCount(send_right2.get()));
  EXPECT_EQ(0u, GetDeadNameRefCount(send_right2.get()));

  expected_receive.push_back(42);
  EXPECT_EQ(expected_receive, received_processes());
  EXPECT_EQ(receive_right2.get(), provider()->TaskForPid(42));
}

}  // namespace content
