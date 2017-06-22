// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "platform/PlatformExport.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/task_queue.h"

namespace base {
namespace trace_event {
class BlameContext;
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class TaskQueue;
class WebTaskRunnerImpl;
class WebViewSchedulerImpl;

class WebFrameSchedulerImpl : public WebFrameScheduler {
 public:
  WebFrameSchedulerImpl(RendererSchedulerImpl* renderer_scheduler,
                        WebViewSchedulerImpl* parent_web_view_scheduler,
                        base::trace_event::BlameContext* blame_context);

  ~WebFrameSchedulerImpl() override;

  // WebFrameScheduler implementation:
  void AddThrottlingObserver(ObserverType, Observer*) override;
  void RemoveThrottlingObserver(ObserverType, Observer*) override;
  void SetFrameVisible(bool frame_visible) override;
  void SetPageVisible(bool page_visible) override;
  void SetSuspended(bool frame_suspended) override;
  void SetCrossOrigin(bool cross_origin) override;
  RefPtr<WebTaskRunner> LoadingTaskRunner() override;
  RefPtr<WebTaskRunner> TimerTaskRunner() override;
  RefPtr<WebTaskRunner> SuspendableTaskRunner() override;
  RefPtr<WebTaskRunner> UnthrottledTaskRunner() override;
  RefPtr<WebTaskRunner> UnthrottledButBlockableTaskRunner() override;
  WebViewScheduler* GetWebViewScheduler() override;
  void WillNavigateBackForwardSoon() override;
  void DidStartProvisionalLoad() override;
  void DidFailProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame) override;
  void DidStartLoading(unsigned long identifier) override;
  void DidStopLoading(unsigned long identifier) override;
  void SetDocumentParsingInBackground(bool background_parser_active) override;
  void OnFirstMeaningfulPaint() override;
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override;
  void AsValueInto(base::trace_event::TracedValue* state) const;

  bool has_active_connection() const { return active_connection_count_; }

 private:
  friend class WebViewSchedulerImpl;

  class ActiveConnectionHandleImpl : public ActiveConnectionHandle {
   public:
    ActiveConnectionHandleImpl(WebFrameSchedulerImpl* frame_scheduler);
    ~ActiveConnectionHandleImpl() override;

   private:
    base::WeakPtr<WebFrameSchedulerImpl> frame_scheduler_;

    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandleImpl);
  };

  void DetachFromWebViewScheduler();
  void RemoveTimerQueueFromBackgroundCPUTimeBudgetPool();
  void ApplyPolicyToTimerQueue();
  bool ShouldThrottleTimers() const;
  void UpdateTimerThrottling(bool was_throttled);

  void DidOpenActiveConnection();
  void DidCloseActiveConnection();

  base::WeakPtr<WebFrameSchedulerImpl> AsWeakPtr();

  scoped_refptr<TaskQueue> loading_task_queue_;
  scoped_refptr<TaskQueue> timer_task_queue_;
  scoped_refptr<TaskQueue> unthrottled_task_queue_;
  scoped_refptr<TaskQueue> suspendable_task_queue_;
  scoped_refptr<TaskQueue> unthrottled_but_blockable_task_queue_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> loading_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> timer_queue_enabled_voter_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter>
      suspendable_queue_enabled_voter_;
  RefPtr<WebTaskRunnerImpl> loading_web_task_runner_;
  RefPtr<WebTaskRunnerImpl> timer_web_task_runner_;
  RefPtr<WebTaskRunnerImpl> unthrottled_web_task_runner_;
  RefPtr<WebTaskRunnerImpl> suspendable_web_task_runner_;
  RefPtr<WebTaskRunnerImpl> unthrottled_but_blockable_web_task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;        // NOT OWNED
  WebViewSchedulerImpl* parent_web_view_scheduler_;  // NOT OWNED
  base::trace_event::BlameContext* blame_context_;   // NOT OWNED
  std::set<Observer*> loader_observers_;             // NOT OWNED
  bool frame_visible_;
  bool page_visible_;
  bool frame_suspended_;
  bool cross_origin_;
  int active_connection_count_;

  base::WeakPtrFactory<WebFrameSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
