// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/lazy_scheduler_message_loop_delegate_for_tests.h"

namespace blink {
namespace scheduler {

std::unique_ptr<RendererScheduler> CreateRendererSchedulerForTests() {
  return std::make_unique<scheduler::RendererSchedulerImpl>(
      scheduler::LazySchedulerMessageLoopDelegateForTests::Create());
}

void RunIdleTasksForTesting(RendererScheduler* scheduler,
                            const base::Closure& callback) {
  RendererSchedulerImpl* scheduler_impl =
      static_cast<RendererSchedulerImpl*>(scheduler);
  scheduler_impl->RunIdleTasksForTesting(callback);
}

}  // namespace scheduler
}  // namespace blink
