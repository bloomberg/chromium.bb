// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void PostQuitTask(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void SetFlagTask(
  scoped_refptr<base::SingleThreadTaskRunner> task_runner,
  bool* success) {
  *success = true;
}

}  // namespace

namespace remoting {

TEST(AutoThreadTaskRunnerTest, StartAndStopMain) {
  // Create a task runner.
  MessageLoop message_loop;
  scoped_refptr<AutoThreadTaskRunner> task_runner =
      new AutoThreadTaskRunner(
          message_loop.message_loop_proxy(),
          base::Bind(&PostQuitTask, &message_loop));

  // Post a task to make sure it is executed.
  bool success = false;
  message_loop.PostTask(FROM_HERE, base::Bind(&SetFlagTask, task_runner,
                                              &success));

  task_runner = NULL;
  message_loop.Run();
  EXPECT_TRUE(success);
}

TEST(AutoThreadTaskRunnerTest, StartAndStopWorker) {
  // Create a task runner.
  MessageLoop message_loop;
  scoped_refptr<AutoThreadTaskRunner> task_runner =
      new AutoThreadTaskRunner(
          message_loop.message_loop_proxy(),
          base::Bind(&PostQuitTask, &message_loop));

  // Create a child task runner.
  base::Thread thread("Child task runner");
  thread.Start();
  task_runner = new AutoThreadTaskRunner(thread.message_loop_proxy(),
                                         task_runner);

  // Post a task to make sure it is executed.
  bool success = false;
  message_loop.PostTask(FROM_HERE, base::Bind(&SetFlagTask, task_runner,
                                              &success));

  task_runner = NULL;
  message_loop.Run();
  EXPECT_TRUE(success);

  thread.Stop();
}

}  // namespace remoting
