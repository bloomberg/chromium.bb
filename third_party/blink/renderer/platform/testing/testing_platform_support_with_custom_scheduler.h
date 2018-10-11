// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_CUSTOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_CUSTOM_SCHEDULER_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

// Test environment where you can inject your custom implementation of
// ThreadScheduler on the main thread. Multi-thread is not supported.
//
// You would probably like to use this with ScopedTestingPlatformSupport
// class template. See testing_platform_support.h for details.

namespace blink {

class TestingPlatformSupportWithCustomScheduler
    : public TestingPlatformSupport {
 public:
  // |scheduler| must be owned by the caller.
  explicit TestingPlatformSupportWithCustomScheduler(
      ThreadScheduler* scheduler);
  ~TestingPlatformSupportWithCustomScheduler() override;

 private:
  std::unique_ptr<Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(TestingPlatformSupportWithCustomScheduler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_CUSTOM_SCHEDULER_H_
