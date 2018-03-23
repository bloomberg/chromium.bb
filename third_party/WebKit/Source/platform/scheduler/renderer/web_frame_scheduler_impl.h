// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "platform/PlatformExport.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/page_visibility_state.h"
#include "platform/scheduler/child/worker_scheduler_proxy.h"
#include "platform/scheduler/renderer/frame_origin_type.h"
#include "platform/scheduler/util/tracing_helper.h"

namespace base {
namespace trace_event {
class BlameContext;
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class MainThreadTaskQueue;
class PageSchedulerImpl;
class RendererSchedulerImpl;
class TaskQueue;

namespace renderer_scheduler_impl_unittest {
class RendererSchedulerImplTest;
}

namespace web_frame_scheduler_impl_unittest {
class WebFrameSchedulerImplTest;
}

namespace page_scheduler_impl_unittest {
class PageSchedulerImplTest;
}

class PLATFORM_EXPORT WebFrameSchedulerImpl : public WebFrameScheduler {
 public:
  WebFrameSchedulerImpl(RendererSchedulerImpl* renderer_scheduler,
                        PageSchedulerImpl* parent_page_scheduler,
                        base::trace_event::BlameContext* blame_context,
                        WebFrameScheduler::FrameType frame_type);

  ~WebFrameSchedulerImpl() override;

  // WebFrameScheduler implementation:
  std::unique_ptr<ThrottlingObserverHandle> AddThrottlingObserver(
      ObserverType,
      Observer*) override;
  void SetFrameVisible(bool frame_visible) override;
  bool IsFrameVisible() const override;
  bool IsPageVisible() const override;
  void SetPaused(bool frame_paused) override;
  void SetPageFrozen(bool) override;

  void SetCrossOrigin(bool cross_origin) override;
  bool IsCrossOrigin() const override;
  void TraceUrlChange(const String& url) override;
  WebFrameScheduler::FrameType GetFrameType() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;
  PageScheduler* GetPageScheduler() const override;
  void DidStartProvisionalLoad(bool is_main_frame) override;
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override;
  void OnFirstMeaningfulPaint() override;
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override;
  void AsValueInto(base::trace_event::TracedValue* state) const;
  bool IsExemptFromBudgetBasedThrottling() const override;

  scoped_refptr<TaskQueue> ControlTaskQueue();
  void SetPageVisibility(PageVisibilityState page_visibility);

  bool has_active_connection() const { return active_connection_count_; }

  void OnTraceLogEnabled() {
    tracing_controller_.OnTraceLogEnabled();
  }

 private:
  friend class PageSchedulerImpl;
  friend class renderer_scheduler_impl_unittest::RendererSchedulerImplTest;
  friend class web_frame_scheduler_impl_unittest::WebFrameSchedulerImplTest;
  friend class page_scheduler_impl_unittest::PageSchedulerImplTest;

  class ActiveConnectionHandleImpl : public ActiveConnectionHandle {
   public:
    ActiveConnectionHandleImpl(WebFrameSchedulerImpl* frame_scheduler);
    ~ActiveConnectionHandleImpl() override;

   private:
    base::WeakPtr<WebFrameSchedulerImpl> frame_scheduler_;

    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandleImpl);
  };

  class ThrottlingObserverHandleImpl : public ThrottlingObserverHandle {
   public:
    ThrottlingObserverHandleImpl(WebFrameSchedulerImpl* frame_scheduler,
                                 Observer* observer);
    ~ThrottlingObserverHandleImpl() override;

   private:
    base::WeakPtr<WebFrameSchedulerImpl> frame_scheduler_;
    Observer* observer_;

    DISALLOW_COPY_AND_ASSIGN(ThrottlingObserverHandleImpl);
  };

  void DetachFromPageScheduler();
  void RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();
  void ApplyPolicyToThrottleableQueue();
  bool ShouldThrottleTimers() const;
  void UpdateTaskQueueThrottling();
  WebFrameScheduler::ThrottlingState CalculateThrottlingState() const;
  void UpdateThrottlingState();
  void RemoveThrottlingObserver(Observer* observer);
  void UpdateTaskQueues();
  void UpdateTaskQueue(const scoped_refptr<MainThreadTaskQueue>& queue,
                       TaskQueue::QueueEnabledVoter* voter);

  void DidOpenActiveConnection();
  void DidCloseActiveConnection();

  scoped_refptr<TaskQueue> LoadingTaskQueue();
  scoped_refptr<TaskQueue> LoadingControlTaskQueue();
  scoped_refptr<TaskQueue> ThrottleableTaskQueue();
  scoped_refptr<TaskQueue> DeferrableTaskQueue();
  scoped_refptr<TaskQueue> PausableTaskQueue();
  scoped_refptr<TaskQueue> UnpausableTaskQueue();

  base::WeakPtr<WebFrameSchedulerImpl> GetWeakPtr();

  const WebFrameScheduler::FrameType frame_type_;

  TraceableVariableController tracing_controller_;
  scoped_refptr<MainThreadTaskQueue> loading_task_queue_;
  scoped_refptr<MainThreadTaskQueue> loading_control_task_queue_;
  scoped_refptr<MainThreadTaskQueue> throttleable_task_queue_;
  scoped_refptr<MainThreadTaskQueue> deferrable_task_queue_;
  scoped_refptr<MainThreadTaskQueue> pausable_task_queue_;
  scoped_refptr<MainThreadTaskQueue> unpausable_task_queue_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> loading_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter>
      loading_control_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter>
      throttleable_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> deferrable_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> pausable_queue_enabled_voter_;
  RendererSchedulerImpl* renderer_scheduler_;       // NOT OWNED
  PageSchedulerImpl* parent_page_scheduler_;        // NOT OWNED
  base::trace_event::BlameContext* blame_context_;  // NOT OWNED
  std::set<Observer*> loader_observers_;            // NOT OWNED
  WebFrameScheduler::ThrottlingState throttling_state_;
  TraceableState<bool, kTracingCategoryNameInfo> frame_visible_;
  TraceableState<PageVisibilityState, kTracingCategoryNameInfo>
      page_visibility_;
  TraceableState<bool, kTracingCategoryNameInfo> page_frozen_;
  TraceableState<bool, kTracingCategoryNameInfo> frame_paused_;
  TraceableState<FrameOriginType, kTracingCategoryNameInfo> frame_origin_type_;
  StateTracer<kTracingCategoryNameInfo> url_tracer_;
  // |task_queue_throttled_| is false if |throttleable_task_queue_| is absent.
  TraceableState<bool, kTracingCategoryNameDebug> task_queue_throttled_;
  int active_connection_count_;

  base::WeakPtrFactory<WebFrameSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
