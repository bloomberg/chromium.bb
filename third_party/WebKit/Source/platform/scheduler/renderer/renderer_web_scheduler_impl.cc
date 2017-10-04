// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_web_scheduler_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

namespace blink {
namespace scheduler {

RendererWebSchedulerImpl::RendererWebSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler)
    : WebSchedulerImpl(renderer_scheduler,
                       renderer_scheduler->IdleTaskRunner(),
                       renderer_scheduler->LoadingTaskQueue(),
                       renderer_scheduler->TimerTaskQueue(),
                       renderer_scheduler->V8TaskQueue()),
      renderer_scheduler_(renderer_scheduler),
      compositor_task_runner_(WebTaskRunnerImpl::Create(
          renderer_scheduler_->CompositorTaskQueue())) {}

RendererWebSchedulerImpl::~RendererWebSchedulerImpl() {}

WebTaskRunner* RendererWebSchedulerImpl::CompositorTaskRunner() {
  return compositor_task_runner_.get();
}

void RendererWebSchedulerImpl::PauseTimerQueue() {
  renderer_scheduler_->PauseTimerQueue();
}

void RendererWebSchedulerImpl::ResumeTimerQueue() {
  renderer_scheduler_->ResumeTimerQueue();
}

std::unique_ptr<blink::WebViewScheduler>
RendererWebSchedulerImpl::CreateWebViewScheduler(
    InterventionReporter* intervention_reporter,
    WebViewScheduler::WebViewSchedulerDelegate* delegate) {
  return base::WrapUnique(new WebViewSchedulerImpl(
      intervention_reporter, delegate, renderer_scheduler_,
      !blink::RuntimeEnabledFeatures::
          TimerThrottlingForBackgroundTabsEnabled()));
}

RendererScheduler* RendererWebSchedulerImpl::GetRendererSchedulerForTest() {
  return renderer_scheduler_;
}

}  // namespace scheduler
}  // namespace blink
