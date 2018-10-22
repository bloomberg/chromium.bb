// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/test/gtest_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/content_browser_test.h"

namespace content {

class BrowserThreadPostTaskBeforeInitBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    // This should fail because the TaskScheduler + TaskExecutor weren't created
    // yet.
    EXPECT_DCHECK_DEATH(base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                                                 base::DoNothing()));

    // Obtaining a TaskRunner should also fail.
    EXPECT_DCHECK_DEATH(base::CreateTaskRunnerWithTraits({BrowserThread::IO}));

    ContentBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserThreadPostTaskBeforeInitBrowserTest,
                       ExpectFailures) {}

class BrowserThreadPostTaskBeforeThreadCreationBrowserTest
    : public ContentBrowserTest {
 protected:
  void CreatedBrowserMainParts(BrowserMainParts* browser_main_parts) override {
    // This should fail because the IO thread hasn't been initialized yet.
    EXPECT_DCHECK_DEATH(base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                                                 base::DoNothing()));

    // Obtaining a TaskRunner should work, because the TaskExecutor was
    // registered already.
    auto task_runner = base::CreateTaskRunnerWithTraits({BrowserThread::IO});
    // But posting should still fail.
    EXPECT_DCHECK_DEATH(task_runner->PostTask(FROM_HERE, base::DoNothing()));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserThreadPostTaskBeforeThreadCreationBrowserTest,
                       ExpectFailures) {}

}  // namespace content
