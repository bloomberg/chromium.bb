// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/stack_unwinder_android.h"

#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/trace_event/cfi_backtrace_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

const size_t kMaxStackFrames = 40;

class StackUnwinderTest : public testing::Test {
 public:
  StackUnwinderTest() : testing::Test() {}
  ~StackUnwinderTest() override {}

  void SetUp() override {
    unwinder_.Initialize();
    base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
        ->AllocateCacheForCurrentThread();
  }

  StackUnwinderAndroid* unwinder() { return &unwinder_; }

 private:
  StackUnwinderAndroid unwinder_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(StackUnwinderTest);
};

uintptr_t GetCurrentPC() {
  return reinterpret_cast<uintptr_t>(__builtin_return_address(0));
}

}  // namespace

TEST_F(StackUnwinderTest, UnwindCurrentThread) {
  const void* frames[kMaxStackFrames];
  size_t result = unwinder()->TraceStack(frames, kMaxStackFrames);
  EXPECT_GT(result, 0u);

  // Since we are starting from chrome library function (this), all the unwind
  // frames will be chrome frames.
  for (size_t i = 0; i < result; ++i) {
    EXPECT_TRUE(
        base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
            ->is_chrome_address(reinterpret_cast<uintptr_t>(frames[i])));
  }
}

TEST_F(StackUnwinderTest, UnwindOtherThread) {
  base::WaitableEvent unwind_finished_event;
  auto task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  auto callback = [](StackUnwinderAndroid* unwinder, base::PlatformThreadId tid,
                     base::WaitableEvent* unwind_finished_event,
                     uintptr_t test_pc) {
    const void* frames[kMaxStackFrames];
    size_t result = unwinder->TraceStack(tid, frames, kMaxStackFrames);
    EXPECT_GT(result, 0u);
    for (size_t i = 0; i < result; ++i) {
      uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
      EXPECT_TRUE(unwinder->IsAddressMapped(addr));
    }

    unwind_finished_event->Signal();
  };

  // Post task on background thread to unwind the current thread.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(callback, base::Unretained(unwinder()),
                                       base::PlatformThread::CurrentId(),
                                       &unwind_finished_event, GetCurrentPC()));

  // While the background thread is trying to unwind make some slow framework
  // calls (malloc) so that the current thread can be stopped in framework
  // library functions on stack.
  // TODO(ssid): Test for reliable unwinding through non-chrome and chrome
  // frames.
  while (true) {
    std::vector<int> temp;
    temp.reserve(kMaxStackFrames);
    usleep(100);

    if (unwind_finished_event.IsSignaled())
      break;
  }
}

}  // namespace tracing
