// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MultiThreadedTestUtil_h
#define MultiThreadedTestUtil_h

#include "testing/gtest/include/gtest/gtest.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

// This creates 2 macros for TSAN: TSAN_TEST and TSAN_TEST_F.
// Those tests are automatically disabled on non-tsan builds and should be used
// instead of the normal gtest macros for MultiThreadedTests.
// It guarantees that those tests are only run on Thread Sanitizer enabled
// builds.
// Also, TSAN_TEST subclasses MultiThreadTest instead of testing::Test.
#if defined(THREAD_SANITIZER)

#define TSAN_TEST(test_case_name, test_name)                         \
  GTEST_TEST_(test_case_name, test_name, ::blink::MultiThreadedTest, \
              ::testing::internal::GetTypeId<::blink::MultiThreadedTest>())

#define TSAN_TEST_F(test_fixture, test_name) TEST_F(test_fixture, test_name)

#else

#define TSAN_TEST(test_case_name, test_name)        \
  GTEST_TEST_(test_case_name, DISABLED_##test_name, \
              ::blink::MultiThreadedTest,           \
              ::testing::internal::GetTypeId<::blink::MultiThreadedTest>())

#define TSAN_TEST_F(test_fixture, test_name) \
  TEST_F(test_fixture, DISABLED_##test_name)

#endif

class MultiThreadedTest : public testing::Test {
 public:
  // RunOnThreads run a closure num_threads * callbacks_per_thread times.
  // The default for this is 10*100 = 1000 times.
  template <typename FunctionType, typename... Ps>
  void RunOnThreads(FunctionType function, Ps&&... parameters) {
    Vector<std::unique_ptr<WebThreadSupportingGC>> threads;
    Vector<std::unique_ptr<WaitableEvent>> waits;

    for (int i = 0; i < num_threads_; ++i) {
      threads.push_back(WebThreadSupportingGC::Create(""));
      waits.push_back(WTF::MakeUnique<WaitableEvent>());
    }

    for (int i = 0; i < num_threads_; ++i) {
      WebTaskRunner* task_runner =
          threads[i]->PlatformThread().GetWebTaskRunner();

      task_runner->PostTask(
          FROM_HERE,
          CrossThreadBind(
              [](WebThreadSupportingGC* thread) { thread->Initialize(); },
              CrossThreadUnretained(threads[i].get())));

      for (int j = 0; j < callbacks_per_thread_; ++j) {
        task_runner->PostTask(FROM_HERE,
                              CrossThreadBind(function, parameters...));
      }

      task_runner->PostTask(
          FROM_HERE, CrossThreadBind(
                         [](WebThreadSupportingGC* thread, WaitableEvent* w) {
                           thread->Shutdown();
                           w->Signal();
                         },
                         CrossThreadUnretained(threads[i].get()),
                         CrossThreadUnretained(waits[i].get())));
    }

    for (int i = 0; i < num_threads_; ++i) {
      waits[i]->Wait();
    }
  }

 protected:
  int num_threads_ = 10;
  int callbacks_per_thread_ = 100;
};

}  // namespace blink

#endif  // MultiThreadedTestUtil_h
