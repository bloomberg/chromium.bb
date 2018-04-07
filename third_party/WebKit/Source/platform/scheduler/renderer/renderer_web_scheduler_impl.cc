// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/renderer/renderer_web_scheduler_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_runner_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

namespace blink {
namespace scheduler {

// TODO(kraynov): Ditch kDeprecatedNone here.
RendererWebSchedulerImpl::RendererWebSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler)
    : WebSchedulerImpl(renderer_scheduler,
                       renderer_scheduler->IdleTaskRunner(),
                       renderer_scheduler->V8TaskQueue()),
      renderer_scheduler_(renderer_scheduler),
      compositor_task_runner_(
          TaskRunnerImpl::Create(renderer_scheduler_->CompositorTaskQueue(),
                                 TaskType::kDeprecatedNone)) {}

RendererWebSchedulerImpl::~RendererWebSchedulerImpl() = default;

base::SingleThreadTaskRunner* RendererWebSchedulerImpl::CompositorTaskRunner() {
  return compositor_task_runner_.get();
}

std::unique_ptr<RendererWebSchedulerImpl::RendererPauseHandle>
RendererWebSchedulerImpl::PauseScheduler() {
  return renderer_scheduler_->PauseRenderer();
}

std::unique_ptr<blink::PageScheduler>
RendererWebSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  return base::WrapUnique(
      new PageSchedulerImpl(delegate, renderer_scheduler_,
                            !blink::RuntimeEnabledFeatures::
                                TimerThrottlingForBackgroundTabsEnabled()));
}

base::TimeTicks RendererWebSchedulerImpl::MonotonicallyIncreasingVirtualTime()
    const {
  return renderer_scheduler_->GetActiveTimeDomain()->Now();
}

WebMainThreadScheduler*
RendererWebSchedulerImpl::GetWebMainThreadSchedulerForTest() {
  return renderer_scheduler_;
}

}  // namespace scheduler
}  // namespace blink
