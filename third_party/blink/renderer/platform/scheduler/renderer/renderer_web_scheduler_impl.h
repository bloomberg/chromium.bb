// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_

#include "third_party/blink/renderer/platform/scheduler/child/web_scheduler_impl.h"

namespace blink {
namespace scheduler {

class MainThreadSchedulerImpl;

class PLATFORM_EXPORT RendererWebSchedulerImpl : public WebSchedulerImpl {
 public:
  explicit RendererWebSchedulerImpl(
      MainThreadSchedulerImpl* main_thread_scheduler);

  ~RendererWebSchedulerImpl() override;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override
      WARN_UNUSED_RESULT;
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;

  base::TimeTicks MonotonicallyIncreasingVirtualTime() const override;

  WebMainThreadScheduler* GetWebMainThreadSchedulerForTest() override;

 private:
  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED
  scoped_refptr<TaskQueueWithTaskType> compositor_task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_
