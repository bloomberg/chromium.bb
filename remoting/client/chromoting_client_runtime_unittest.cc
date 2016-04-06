// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

#if defined(OS_IOS) || defined(OS_ANDROID)

// A simple test that starts and stop the runtime. This tests the runtime
// operates properly and all threads and message loops are valid.
TEST(ChromotingClientRuntimeTest, StartAndStop) {
  std::unique_ptr<base::MessageLoopForUI> ui_loop;
  ui_loop.reset(new base::MessageLoopForUI());
#if defined(OS_IOS)
  ui_loop->Attach();
#endif

  std::unique_ptr<ChromotingClientRuntime> runtime =
      ChromotingClientRuntime::Create(ui_loop.get());

  ASSERT_TRUE(runtime);
  EXPECT_TRUE(runtime->network_task_runner().get());
  EXPECT_TRUE(runtime->ui_task_runner().get());
  EXPECT_TRUE(runtime->display_task_runner().get());
  EXPECT_TRUE(runtime->file_task_runner().get());
  EXPECT_TRUE(runtime->url_requester().get());
}

#endif

}  // namespace remoting
