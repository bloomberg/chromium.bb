// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// A simple test that starts and stop the context. This tests the context
// operates properly and all threads and message loops are valid.
TEST(ChromotingHostContextTest, StartAndStop) {
  MessageLoopForUI message_loop;
  ChromotingHostContext context(new AutoThreadTaskRunner(
      base::MessageLoopProxy::current()));

  context.Start();
  EXPECT_TRUE(context.network_task_runner());
  EXPECT_TRUE(context.capture_task_runner());
  EXPECT_TRUE(context.encode_task_runner());
}

}  // namespace remoting
