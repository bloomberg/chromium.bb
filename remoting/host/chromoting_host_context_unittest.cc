// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "remoting/host/chromoting_host_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// A simple test that starts and stop the context. This tests the context
// operates properly and all threads and message loops are valid.
TEST(ChromotingHostContextTest, StartAndStop) {
  ChromotingHostContext context(NULL);
  context.Start();
  EXPECT_TRUE(context.jingle_thread());
  EXPECT_TRUE(context.main_message_loop());
  EXPECT_TRUE(context.encode_message_loop());
  context.Stop();

  // Expect all the threads are stopped.
  EXPECT_FALSE(context.main_thread_.IsRunning());
  EXPECT_FALSE(context.encode_thread_.IsRunning());
}

}  // namespace remoting
