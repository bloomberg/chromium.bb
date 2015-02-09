// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_manager.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class ChannelManagerTest : public testing::Test {
 public:
  ChannelManagerTest() : message_loop_(base::MessageLoop::TYPE_IO) {}
  ~ChannelManagerTest() override {}

 protected:
  embedder::SimplePlatformSupport* platform_support() {
    return &platform_support_;
  }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  embedder::SimplePlatformSupport platform_support_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelManagerTest);
};

TEST_F(ChannelManagerTest, Basic) {
  ChannelManager cm(platform_support());

  embedder::PlatformChannelPair channel_pair;

  const ChannelId id = 1;
  scoped_refptr<MessagePipeDispatcher> d =
      cm.CreateChannelOnIOThread(id, channel_pair.PassServerHandle());

  scoped_refptr<Channel> ch = cm.GetChannel(id);
  EXPECT_TRUE(ch);
  // |ChannelManager| should have a ref.
  EXPECT_FALSE(ch->HasOneRef());

  cm.WillShutdownChannel(id);
  // |ChannelManager| should still have a ref.
  EXPECT_FALSE(ch->HasOneRef());

  cm.ShutdownChannel(id);
  // On the "I/O" thread, so shutdown should happen synchronously.
  // |ChannelManager| should have given up its ref.
  EXPECT_TRUE(ch->HasOneRef());

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

TEST_F(ChannelManagerTest, TwoChannels) {
  ChannelManager cm(platform_support());

  embedder::PlatformChannelPair channel_pair;

  const ChannelId id1 = 1;
  scoped_refptr<MessagePipeDispatcher> d1 =
      cm.CreateChannelOnIOThread(id1, channel_pair.PassServerHandle());

  const ChannelId id2 = 2;
  scoped_refptr<MessagePipeDispatcher> d2 =
      cm.CreateChannelOnIOThread(id2, channel_pair.PassClientHandle());

  scoped_refptr<Channel> ch1 = cm.GetChannel(id1);
  EXPECT_TRUE(ch1);

  scoped_refptr<Channel> ch2 = cm.GetChannel(id2);
  EXPECT_TRUE(ch2);

  // Calling |WillShutdownChannel()| multiple times (on |id1|) is okay.
  cm.WillShutdownChannel(id1);
  cm.WillShutdownChannel(id1);
  EXPECT_FALSE(ch1->HasOneRef());
  // Not calling |WillShutdownChannel()| (on |id2|) is okay too.

  cm.ShutdownChannel(id1);
  EXPECT_TRUE(ch1->HasOneRef());
  cm.ShutdownChannel(id2);
  EXPECT_TRUE(ch2->HasOneRef());

  EXPECT_EQ(MOJO_RESULT_OK, d1->Close());
  EXPECT_EQ(MOJO_RESULT_OK, d2->Close());
}

class OtherThread : public base::SimpleThread {
 public:
  // Note: There should be no other refs to the channel identified by
  // |channel_id| outside the channel manager.
  OtherThread(scoped_refptr<base::TaskRunner> task_runner,
              ChannelManager* channel_manager,
              ChannelId channel_id,
              base::Closure quit_closure)
      : base::SimpleThread("other_thread"),
        task_runner_(task_runner),
        channel_manager_(channel_manager),
        channel_id_(channel_id),
        quit_closure_(quit_closure) {}
  ~OtherThread() override {}

 private:
  void Run() override {
    // TODO(vtl): Once we have a way of creating a channel from off the I/O
    // thread, do that here instead.

    // You can use any unique, nonzero value as the ID.
    scoped_refptr<Channel> ch = channel_manager_->GetChannel(channel_id_);
    // |ChannelManager| should have a ref.
    EXPECT_FALSE(ch->HasOneRef());

    channel_manager_->WillShutdownChannel(channel_id_);
    // |ChannelManager| should still have a ref.
    EXPECT_FALSE(ch->HasOneRef());

    channel_manager_->ShutdownChannel(channel_id_);
    // This doesn't happen synchronously, so we "wait" until it does.
    // TODO(vtl): Probably |Channel| should provide some notification of being
    // shut down.
    base::TimeTicks start_time(base::TimeTicks::Now());
    for (;;) {
      if (ch->HasOneRef())
        break;

      // Check, instead of assert, since if things go wrong, dying is more
      // reliable than tearing down.
      CHECK_LT(base::TimeTicks::Now() - start_time,
               TestTimeouts::action_timeout());
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
    }

    CHECK(task_runner_->PostTask(FROM_HERE, quit_closure_));
  }

  scoped_refptr<base::TaskRunner> task_runner_;
  ChannelManager* channel_manager_;
  ChannelId channel_id_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(OtherThread);
};

TEST_F(ChannelManagerTest, CallsFromOtherThread) {
  ChannelManager cm(platform_support());

  embedder::PlatformChannelPair channel_pair;

  const ChannelId id = 1;
  scoped_refptr<MessagePipeDispatcher> d =
      cm.CreateChannelOnIOThread(id, channel_pair.PassServerHandle());

  base::RunLoop run_loop;
  OtherThread thread(base::MessageLoopProxy::current(), &cm, id,
                     run_loop.QuitClosure());
  thread.Start();
  run_loop.Run();
  thread.Join();

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

}  // namespace
}  // namespace system
}  // namespace mojo
