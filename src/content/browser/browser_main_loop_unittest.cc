// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/scoped_command_line.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Verify that a single-process browser process has at least as many threads as
// the number of cores in its foreground pool.
TEST(BrowserMainLoopTest, CreateThreadsInSingleProcess) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kSingleProcess);
    base::ThreadPool::Create("Browser");
    StartBrowserThreadPool();
    BrowserTaskExecutor::Create();
    MainFunctionParams main_function_params(
        *scoped_command_line.GetProcessCommandLine());
    BrowserMainLoop browser_main_loop(
        main_function_params,
        std::make_unique<base::ThreadPool::ScopedExecutionFence>());
    browser_main_loop.MainMessageLoopStart();
    browser_main_loop.Init();
    browser_main_loop.CreateThreads();
    EXPECT_GE(base::ThreadPool::GetInstance()
                  ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                      {base::TaskPriority::USER_VISIBLE}),
              base::SysInfo::NumberOfProcessors() - 1);
    browser_main_loop.ShutdownThreadsAndCleanUp();
  }
  BrowserTaskExecutor::ResetForTesting();
  for (int id = BrowserThread::UI; id < BrowserThread::ID_COUNT; ++id) {
    BrowserThreadImpl::ResetGlobalsForTesting(
        static_cast<BrowserThread::ID>(id));
  }
  base::ThreadPool::GetInstance()->JoinForTesting();
  base::ThreadPool::SetInstance(nullptr);
}

}  // namespace content
