// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/ipc_support.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_timeouts.h"
#include "mojo/edk/embedder/master_process_delegate.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/embedder/slave_process_delegate.h"
#include "mojo/edk/system/channel_manager.h"
#include "mojo/edk/system/connection_identifier.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/process_identifier.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/system/waiter.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const char kConnectionIdFlag[] = "test-connection-id";

class TestMasterProcessDelegate : public embedder::MasterProcessDelegate {
 public:
  TestMasterProcessDelegate()
      : on_slave_disconnect_event_(true, false) {}  // Manual reset.
  ~TestMasterProcessDelegate() override {}

  bool TryWaitForOnSlaveDisconnect() {
    return on_slave_disconnect_event_.TimedWait(TestTimeouts::action_timeout());
  }

 private:
  // |embedder::MasterProcessDelegate| methods:
  void OnShutdownComplete() override { NOTREACHED(); }

  void OnSlaveDisconnect(embedder::SlaveInfo /*slave_info*/) override {
    on_slave_disconnect_event_.Signal();
  }

  base::WaitableEvent on_slave_disconnect_event_;

  DISALLOW_COPY_AND_ASSIGN(TestMasterProcessDelegate);
};

class TestSlaveProcessDelegate : public embedder::SlaveProcessDelegate {
 public:
  TestSlaveProcessDelegate() {}
  ~TestSlaveProcessDelegate() override {}

 private:
  // |embedder::SlaveProcessDelegate| methods:
  void OnShutdownComplete() override { NOTREACHED(); }

  void OnMasterDisconnect() override { NOTREACHED(); }

  DISALLOW_COPY_AND_ASSIGN(TestSlaveProcessDelegate);
};

TEST(IPCSupportTest, MasterSlave) {
  embedder::SimplePlatformSupport platform_support;
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  TestMasterProcessDelegate master_process_delegate;
  // Note: Run master process delegate methods on the I/O thread.
  IPCSupport master_ipc_support(
      &platform_support, embedder::ProcessType::MASTER,
      test_io_thread.task_runner(), &master_process_delegate,
      test_io_thread.task_runner(), embedder::ScopedPlatformHandle());

  ConnectionIdentifier connection_id =
      master_ipc_support.GenerateConnectionIdentifier();

  embedder::PlatformChannelPair channel_pair;
  // Note: |ChannelId|s and |ProcessIdentifier|s are interchangeable.
  ProcessIdentifier slave_id = kInvalidProcessIdentifier;
  base::WaitableEvent event1(true, false);
  scoped_refptr<MessagePipeDispatcher> master_mp =
      master_ipc_support.ConnectToSlave(
          connection_id, nullptr, channel_pair.PassServerHandle(),
          base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event1)),
          nullptr, &slave_id);
  ASSERT_TRUE(master_mp);
  EXPECT_NE(slave_id, kInvalidProcessIdentifier);
  EXPECT_NE(slave_id, kMasterProcessIdentifier);
  // Note: We don't have to wait on |event1| now, but we'll have to do so before
  // tearing down the channel.

  TestSlaveProcessDelegate slave_process_delegate;
  // Note: Run process delegate methods on the I/O thread.
  IPCSupport slave_ipc_support(
      &platform_support, embedder::ProcessType::SLAVE,
      test_io_thread.task_runner(), &slave_process_delegate,
      test_io_thread.task_runner(), channel_pair.PassClientHandle());

  ProcessIdentifier master_id = kInvalidProcessIdentifier;
  base::WaitableEvent event2(true, false);
  scoped_refptr<MessagePipeDispatcher> slave_mp =
      slave_ipc_support.ConnectToMaster(
          connection_id,
          base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event2)),
          nullptr, &master_id);
  ASSERT_TRUE(slave_mp);
  EXPECT_EQ(kMasterProcessIdentifier, master_id);

  // Set up waiting on the slave end first (to avoid racing).
  Waiter waiter;
  waiter.Init();
  ASSERT_EQ(
      MOJO_RESULT_OK,
      slave_mp->AddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 0, nullptr));

  // Write a message with just 'x' through the master's end.
  EXPECT_EQ(MOJO_RESULT_OK,
            master_mp->WriteMessage(UserPointer<const void>("x"), 1, nullptr,
                                    MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for it to arrive.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(test::ActionDeadline(), nullptr));
  slave_mp->RemoveAwakable(&waiter, nullptr);

  // Read the message from the slave's end.
  char buffer[10] = {};
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            slave_mp->ReadMessage(UserPointer<void>(buffer),
                                  MakeUserPointer(&buffer_size), 0, nullptr,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, buffer_size);
  EXPECT_EQ('x', buffer[0]);

  // Don't need the message pipe anymore.
  master_mp->Close();
  slave_mp->Close();

  // A message was sent through the message pipe, |Channel|s must have been
  // established on both sides. The events have thus almost certainly been
  // signalled, but we'll wait just to be sure.
  EXPECT_TRUE(event1.TimedWait(TestTimeouts::action_timeout()));
  EXPECT_TRUE(event2.TimedWait(TestTimeouts::action_timeout()));

  test_io_thread.PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelManager::ShutdownChannelOnIOThread,
                 base::Unretained(slave_ipc_support.channel_manager()),
                 master_id));
  test_io_thread.PostTaskAndWait(
      FROM_HERE, base::Bind(&IPCSupport::ShutdownOnIOThread,
                            base::Unretained(&slave_ipc_support)));

  EXPECT_TRUE(master_process_delegate.TryWaitForOnSlaveDisconnect());

  test_io_thread.PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelManager::ShutdownChannelOnIOThread,
                 base::Unretained(master_ipc_support.channel_manager()),
                 slave_id));
  test_io_thread.PostTaskAndWait(
      FROM_HERE, base::Bind(&IPCSupport::ShutdownOnIOThread,
                            base::Unretained(&master_ipc_support)));
}

}  // namespace

// Note: This test isn't in an anonymous namespace, since it needs to be
// friended by |IPCSupport|.
TEST(IPCSupportTest, MasterSlaveInternal) {
  embedder::SimplePlatformSupport platform_support;
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  TestMasterProcessDelegate master_process_delegate;
  // Note: Run master process delegate methods on the I/O thread.
  IPCSupport master_ipc_support(
      &platform_support, embedder::ProcessType::MASTER,
      test_io_thread.task_runner(), &master_process_delegate,
      test_io_thread.task_runner(), embedder::ScopedPlatformHandle());

  ConnectionIdentifier connection_id =
      master_ipc_support.GenerateConnectionIdentifier();

  embedder::PlatformChannelPair channel_pair;
  ProcessIdentifier slave_id = kInvalidProcessIdentifier;
  embedder::ScopedPlatformHandle master_second_platform_handle =
      master_ipc_support.ConnectToSlaveInternal(
          connection_id, nullptr, channel_pair.PassServerHandle(), &slave_id);
  ASSERT_TRUE(master_second_platform_handle.is_valid());
  EXPECT_NE(slave_id, kInvalidProcessIdentifier);
  EXPECT_NE(slave_id, kMasterProcessIdentifier);

  TestSlaveProcessDelegate slave_process_delegate;
  // Note: Run process delegate methods on the I/O thread.
  IPCSupport slave_ipc_support(
      &platform_support, embedder::ProcessType::SLAVE,
      test_io_thread.task_runner(), &slave_process_delegate,
      test_io_thread.task_runner(), channel_pair.PassClientHandle());

  embedder::ScopedPlatformHandle slave_second_platform_handle =
      slave_ipc_support.ConnectToMasterInternal(connection_id);
  ASSERT_TRUE(slave_second_platform_handle.is_valid());

  // Write an 'x' through the master's end.
  size_t n = 0;
  EXPECT_TRUE(mojo::test::BlockingWrite(master_second_platform_handle.get(),
                                        "x", 1, &n));
  EXPECT_EQ(1u, n);

  // Read it from the slave's end.
  char c = '\0';
  n = 0;
  EXPECT_TRUE(
      mojo::test::BlockingRead(slave_second_platform_handle.get(), &c, 1, &n));
  EXPECT_EQ(1u, n);
  EXPECT_EQ('x', c);

  test_io_thread.PostTaskAndWait(
      FROM_HERE, base::Bind(&IPCSupport::ShutdownOnIOThread,
                            base::Unretained(&slave_ipc_support)));

  EXPECT_TRUE(master_process_delegate.TryWaitForOnSlaveDisconnect());

  test_io_thread.PostTaskAndWait(
      FROM_HERE, base::Bind(&IPCSupport::ShutdownOnIOThread,
                            base::Unretained(&master_ipc_support)));
}

// This is a true multiprocess version of IPCSupportTest.MasterSlaveInternal.
// Note: This test isn't in an anonymous namespace, since it needs to be
// friended by |IPCSupport|.
#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
// TODO(vtl): I'm guessing this is true of this test too?
#define MAYBE_MultiprocessMasterSlaveInternal \
  DISABLED_MultiprocessMasterSlaveInternal
#else
#define MAYBE_MultiprocessMasterSlaveInternal MultiprocessMasterSlaveInternal
#endif  // defined(OS_ANDROID)
TEST(IPCSupportTest, MAYBE_MultiprocessMasterSlaveInternal) {
  embedder::SimplePlatformSupport platform_support;
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  TestMasterProcessDelegate master_process_delegate;
  // Note: Run process delegate methods on the I/O thread.
  IPCSupport ipc_support(&platform_support, embedder::ProcessType::MASTER,
                         test_io_thread.task_runner(), &master_process_delegate,
                         test_io_thread.task_runner(),
                         embedder::ScopedPlatformHandle());

  ConnectionIdentifier connection_id =
      ipc_support.GenerateConnectionIdentifier();
  mojo::test::MultiprocessTestHelper multiprocess_test_helper;
  ProcessIdentifier slave_id = kInvalidProcessIdentifier;
  embedder::ScopedPlatformHandle second_platform_handle =
      ipc_support.ConnectToSlaveInternal(
          connection_id, nullptr,
          multiprocess_test_helper.server_platform_handle.Pass(), &slave_id);
  ASSERT_TRUE(second_platform_handle.is_valid());
  EXPECT_NE(slave_id, kInvalidProcessIdentifier);
  EXPECT_NE(slave_id, kMasterProcessIdentifier);

  multiprocess_test_helper.StartChildWithExtraSwitch(
      "MultiprocessMasterSlaveInternal", kConnectionIdFlag,
      connection_id.ToString());

  // We write a '?'. The slave should write a '!' in response.
  size_t n = 0;
  EXPECT_TRUE(
      mojo::test::BlockingWrite(second_platform_handle.get(), "?", 1, &n));
  EXPECT_EQ(1u, n);

  char c = '\0';
  n = 0;
  EXPECT_TRUE(
      mojo::test::BlockingRead(second_platform_handle.get(), &c, 1, &n));
  EXPECT_EQ(1u, n);
  EXPECT_EQ('!', c);

  EXPECT_TRUE(master_process_delegate.TryWaitForOnSlaveDisconnect());
  EXPECT_TRUE(multiprocess_test_helper.WaitForChildTestShutdown());

  test_io_thread.PostTaskAndWait(FROM_HERE,
                                 base::Bind(&IPCSupport::ShutdownOnIOThread,
                                            base::Unretained(&ipc_support)));
}

MOJO_MULTIPROCESS_TEST_CHILD_TEST(MultiprocessMasterSlaveInternal) {
  embedder::ScopedPlatformHandle client_platform_handle =
      mojo::test::MultiprocessTestHelper::client_platform_handle.Pass();
  ASSERT_TRUE(client_platform_handle.is_valid());

  embedder::SimplePlatformSupport platform_support;
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  TestSlaveProcessDelegate slave_process_delegate;
  // Note: Run process delegate methods on the I/O thread.
  IPCSupport ipc_support(&platform_support, embedder::ProcessType::SLAVE,
                         test_io_thread.task_runner(), &slave_process_delegate,
                         test_io_thread.task_runner(),
                         client_platform_handle.Pass());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  ASSERT_TRUE(command_line.HasSwitch(kConnectionIdFlag));
  bool ok = false;
  ConnectionIdentifier connection_id = ConnectionIdentifier::FromString(
      command_line.GetSwitchValueASCII(kConnectionIdFlag), &ok);
  ASSERT_TRUE(ok);

  embedder::ScopedPlatformHandle second_platform_handle =
      ipc_support.ConnectToMasterInternal(connection_id);
  ASSERT_TRUE(second_platform_handle.is_valid());

  // The master should write a '?'. We'll write a '!' in response.
  char c = '\0';
  size_t n = 0;
  EXPECT_TRUE(
      mojo::test::BlockingRead(second_platform_handle.get(), &c, 1, &n));
  EXPECT_EQ(1u, n);
  EXPECT_EQ('?', c);

  n = 0;
  EXPECT_TRUE(
      mojo::test::BlockingWrite(second_platform_handle.get(), "!", 1, &n));
  EXPECT_EQ(1u, n);

  test_io_thread.PostTaskAndWait(FROM_HERE,
                                 base::Bind(&IPCSupport::ShutdownOnIOThread,
                                            base::Unretained(&ipc_support)));
}

// TODO(vtl): Also test the case of the master "dying" before the slave. (The
// slave should get OnMasterDisconnect(), which we currently don't test.)

}  // namespace system
}  // namespace mojo
