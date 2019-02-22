// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_custom_scheduler.h"

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class ThreadWithCustomScheduler : public Thread {
 public:
  explicit ThreadWithCustomScheduler(ThreadScheduler* scheduler)
      : scheduler_(scheduler) {}
  ~ThreadWithCustomScheduler() override {}

  bool IsCurrentThread() const override {
    DCHECK(WTF::IsMainThread());
    return true;
  }
  ThreadScheduler* Scheduler() override { return scheduler_; }

 private:
  ThreadScheduler* scheduler_;
};

}  // namespace

TestingPlatformSupportWithCustomScheduler::
    TestingPlatformSupportWithCustomScheduler(ThreadScheduler* scheduler)
    : thread_(std::make_unique<ThreadWithCustomScheduler>(scheduler)) {
  // If main_thread_ is set, Platform::SetCurrentPlatformForTesting() properly
  // sets up the platform so Platform::CurrentThread() would return the
  // thread specified here.
  main_thread_ = thread_.get();
}

TestingPlatformSupportWithCustomScheduler::
    ~TestingPlatformSupportWithCustomScheduler() {}

}  // namespace blink
