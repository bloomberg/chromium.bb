// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "remoting/base/auto_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kThreadName[] = "Test thread";
const char kThreadName2[] = "Test thread 2";

void SetFlagTask(bool* success) {
  *success = true;
}

void PostSetFlagTask(
    scoped_refptr<base::TaskRunner> task_runner,
    bool* success) {
  task_runner->PostTask(FROM_HERE, base::Bind(&SetFlagTask, success));
}

}  // namespace

namespace remoting {

class AutoThreadTest : public testing::Test {
 public:
  AutoThreadTest() : message_loop_quit_correctly_(false) {
  }

  void RunMessageLoop() {
    // Release |main_task_runner_|, then run |message_loop_| until other
    // references created in tests are gone.  We also post a delayed quit
    // task to |message_loop_| so the test will not hang on failure.
    main_task_runner_ = NULL;
    message_loop_.PostDelayedTask(
        FROM_HERE, MessageLoop::QuitClosure(), base::TimeDelta::FromSeconds(5));
    message_loop_.Run();
  }

  virtual void SetUp() OVERRIDE {
    main_task_runner_ = new AutoThreadTaskRunner(
        message_loop_.message_loop_proxy(),
        base::Bind(&AutoThreadTest::QuitMainMessageLoop,
                   base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    // Verify that |message_loop_| was quit by the AutoThreadTaskRunner.
    EXPECT_TRUE(message_loop_quit_correctly_);
  }

 protected:
  void QuitMainMessageLoop() {
    message_loop_quit_correctly_ = true;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }


  MessageLoop message_loop_;
  bool message_loop_quit_correctly_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
};

TEST_F(AutoThreadTest, StartAndStop) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner.get());

  task_runner = NULL;
  RunMessageLoop();
}

TEST_F(AutoThreadTest, ProcessTask) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner.get());

  // Post a task to it.
  bool success = false;
  task_runner->PostTask(FROM_HERE, base::Bind(&SetFlagTask, &success));

  task_runner = NULL;
  RunMessageLoop();

  EXPECT_TRUE(success);
}

TEST_F(AutoThreadTest, ThreadDependency) {
  // Create two AutoThreads joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner1 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner1.get());
  scoped_refptr<base::TaskRunner> task_runner2 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner2.get());

  // Post a task to thread 1 that will post a task to thread 2.
  bool success = false;
  task_runner1->PostTask(FROM_HERE,
      base::Bind(&PostSetFlagTask, task_runner2, &success));

  task_runner1 = NULL;
  task_runner2 = NULL;
  RunMessageLoop();

  EXPECT_TRUE(success);
}

}  // namespace remoting
