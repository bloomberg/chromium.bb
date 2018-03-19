// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_PAGE_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_PAGE_SCHEDULER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/page_visibility_state.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scheduler/common/throttling/task_queue_throttler.h"
#include "platform/scheduler/renderer/page_scheduler.h"
#include "platform/scheduler/util/tracing_helper.h"

namespace base {
namespace trace_event {
class BlameContext;
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class CPUTimeBudgetPool;
class WebFrameSchedulerImpl;

class PLATFORM_EXPORT PageSchedulerImpl : public PageScheduler {
 public:
  PageSchedulerImpl(PageScheduler::Delegate*,
                    RendererSchedulerImpl*,
                    bool disable_background_timer_throttling);

  ~PageSchedulerImpl() override;

  // PageScheduler implementation:
  void SetPageVisible(bool page_visible) override;
  void SetPageFrozen(bool) override;
  std::unique_ptr<WebFrameScheduler> CreateFrameScheduler(
      BlameContext*,
      WebFrameScheduler::FrameType) override;
  base::TimeTicks EnableVirtualTime() override;
  void DisableVirtualTimeForTesting() override;
  bool VirtualTimeAllowedToAdvance() const override;
  void SetVirtualTimePolicy(VirtualTimePolicy) override;
  void SetInitialVirtualTimeOffset(base::TimeDelta offset) override;
  void GrantVirtualTimeBudget(
      base::TimeDelta budget,
      base::OnceClosure budget_exhausted_callback) override;
  void SetMaxVirtualTimeTaskStarvationCount(
      int max_task_starvation_count) override;
  void AudioStateChanged(bool is_audio_playing) override;
  bool IsPlayingAudio() const override;
  bool IsExemptFromBudgetBasedThrottling() const override;
  bool HasActiveConnectionForTest() const override;
  void RequestBeginMainFrameNotExpected(bool new_state) override;
  void AddVirtualTimeObserver(VirtualTimeObserver*) override;
  void RemoveVirtualTimeObserver(VirtualTimeObserver*) override;

  // Virtual for testing.
  virtual void ReportIntervention(const std::string& message);

  std::unique_ptr<WebFrameSchedulerImpl> CreateWebFrameSchedulerImpl(
      base::trace_event::BlameContext*,
      WebFrameScheduler::FrameType);

  void Unregister(WebFrameSchedulerImpl*);
  void OnNavigation();

  void OnConnectionUpdated();

  void OnTraceLogEnabled();

  // Return a number of child web frame schedulers for this PageScheduler.
  size_t FrameCount() const;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  base::WeakPtr<PageSchedulerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class WebFrameSchedulerImpl;

  CPUTimeBudgetPool* BackgroundCPUTimeBudgetPool();
  void MaybeInitializeBackgroundCPUTimeBudgetPool();

  void OnThrottlingReported(base::TimeDelta throttling_duration);

  // Depending on page visibility, either turns throttling off, or schedules a
  // call to enable it after a grace period.
  void UpdateBackgroundThrottlingState();

  // As a part of UpdateBackgroundThrottlingState set correct
  // background_time_budget_pool_ state depending on page visibility and
  // number of active connections.
  void UpdateBackgroundBudgetPoolThrottlingState();

  TraceableVariableController tracing_controller_;
  std::set<WebFrameSchedulerImpl*> frame_schedulers_;
  RendererSchedulerImpl* renderer_scheduler_;

  PageVisibilityState page_visibility_;
  bool disable_background_timer_throttling_;
  bool is_audio_playing_;
  bool reported_background_throttling_since_navigation_;
  bool has_active_connection_;
  bool nested_runloop_;
  CPUTimeBudgetPool* background_time_budget_pool_;  // Not owned.
  PageScheduler::Delegate* delegate_;               // Not owned.
  base::WeakPtrFactory<PageSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_PAGE_SCHEDULER_IMPL_H_
