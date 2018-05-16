// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/renderer/renderer_web_scheduler_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_runner_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

namespace blink {
namespace scheduler {

RendererWebSchedulerImpl::RendererWebSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler)
    : WebSchedulerImpl(main_thread_scheduler,
                       main_thread_scheduler->IdleTaskRunner(),
                       main_thread_scheduler->V8TaskQueue()),
      main_thread_scheduler_(main_thread_scheduler),
      compositor_task_runner_(
          TaskRunnerImpl::Create(main_thread_scheduler_->CompositorTaskQueue(),
                                 TaskType::kMainThreadTaskQueueCompositor)) {}

RendererWebSchedulerImpl::~RendererWebSchedulerImpl() = default;

scoped_refptr<base::SingleThreadTaskRunner>
RendererWebSchedulerImpl::CompositorTaskRunner() {
  return compositor_task_runner_;
}

std::unique_ptr<RendererWebSchedulerImpl::RendererPauseHandle>
RendererWebSchedulerImpl::PauseScheduler() {
  return main_thread_scheduler_->PauseRenderer();
}

std::unique_ptr<blink::PageScheduler>
RendererWebSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  return base::WrapUnique(
      new PageSchedulerImpl(delegate, main_thread_scheduler_,
                            !blink::RuntimeEnabledFeatures::
                                TimerThrottlingForBackgroundTabsEnabled()));
}

base::TimeTicks RendererWebSchedulerImpl::MonotonicallyIncreasingVirtualTime()
    const {
  return main_thread_scheduler_->GetActiveTimeDomain()->Now();
}

WebMainThreadScheduler*
RendererWebSchedulerImpl::GetWebMainThreadSchedulerForTest() {
  return main_thread_scheduler_;
}

}  // namespace scheduler
}  // namespace blink
