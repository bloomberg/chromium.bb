// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {
namespace page_scheduler_impl_unittest {
class PageSchedulerImplTest;
class PageSchedulerImplPageTransitionTest;
class
    PageSchedulerImplPageTransitionTest_PageLifecycleStateTransitionMetric_Test;
}  // namespace page_scheduler_impl_unittest

class CPUTimeBudgetPool;
class FrameSchedulerImpl;
class MainThreadSchedulerImpl;
class MainThreadTaskQueue;
class WakeUpBudgetPool;

class PLATFORM_EXPORT PageSchedulerImpl : public PageScheduler {
 public:
  // Interval between throttled wake ups, when intensive throttling is disabled.
  static constexpr base::TimeDelta kDefaultThrottledWakeUpInterval =
      base::TimeDelta::FromSeconds(1);

  PageSchedulerImpl(PageScheduler::Delegate*, AgentGroupSchedulerImpl&);

  ~PageSchedulerImpl() override;

  // PageScheduler implementation:
  void OnTitleOrFaviconUpdated() override;
  void SetPageVisible(bool page_visible) override;
  void SetPageFrozen(bool) override;
  void SetPageBackForwardCached(bool) override;
  void OnFocusChanged(bool focused) override;
  void SetKeepActive(bool) override;
  bool IsMainFrameLocal() const override;
  void SetIsMainFrameLocal(bool is_local) override;
  void OnLocalMainFrameNetworkAlmostIdle() override;
  base::TimeTicks GetStoredInBackForwardCacheTimestamp() {
    return stored_in_back_forward_cache_timestamp_;
  }
  bool IsInBackForwardCache() const override {
    return is_stored_in_back_forward_cache_;
  }
  bool has_ipc_detection_enabled() { return has_ipc_detection_enabled_; }

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) override;
  base::TimeTicks EnableVirtualTime() override;
  void DisableVirtualTimeForTesting() override;
  bool VirtualTimeAllowedToAdvance() const override;
  void SetVirtualTimePolicy(VirtualTimePolicy) override;
  void SetInitialVirtualTime(base::Time time) override;
  void SetInitialVirtualTimeOffset(base::TimeDelta offset) override;
  void GrantVirtualTimeBudget(
      base::TimeDelta budget,
      base::OnceClosure budget_exhausted_callback) override;
  void SetMaxVirtualTimeTaskStarvationCount(
      int max_task_starvation_count) override;
  void AudioStateChanged(bool is_audio_playing) override;
  bool IsAudioPlaying() const override;
  bool IsExemptFromBudgetBasedThrottling() const override;
  bool OptedOutFromAggressiveThrottlingForTest() const override;
  bool RequestBeginMainFrameNotExpected(bool new_state) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const WTF::String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override;

  // Virtual for testing.
  virtual void ReportIntervention(const String& message);

  bool IsPageVisible() const;
  bool IsFrozen() const;
  bool OptedOutFromAggressiveThrottling() const;
  // Returns whether CPU time is throttled for the page. Note: This is
  // independent from wake up rate throttling.
  bool IsCPUTimeThrottled() const;
  bool KeepActive() const;

  bool IsLoading() const;

  // An "ordinary" PageScheduler is responsible for a fully-featured page
  // owned by a web view.
  bool IsOrdinary() const;

  MainThreadSchedulerImpl* GetMainThreadScheduler() const;
  AgentGroupSchedulerImpl& GetAgentGroupScheduler() override;

  void Unregister(FrameSchedulerImpl*);
  void OnNavigation();

  void OnThrottlingStatusUpdated();

  void OnTraceLogEnabled();

  bool IsPageFocused() const;

  // Virtual for testing.
  virtual bool IsWaitingForMainFrameContentfulPaint() const;
  virtual bool IsWaitingForMainFrameMeaningfulPaint() const;

  // Return a number of child web frame schedulers for this PageScheduler.
  size_t FrameCount() const;

  PageLifecycleState GetPageLifecycleState() const;

  void SetUpIPCTaskDetection();
  // This flag tracks whether or not IPC tasks are tracked if they are posted to
  // frames or pages that are stored in the back-forward cache
  bool has_ipc_detection_enabled_ = false;

  // Generally UKMs are associated with the main frame of a page, but the
  // implementation allows to request a recorder from any local frame with
  // the same result (e.g. for OOPIF support), therefore we need to select
  // any frame here.
  // Note that selecting main frame doesn't work for OOPIFs where the main
  // frame it not a local one.
  FrameSchedulerImpl* SelectFrameForUkmAttribution();

  void WriteIntoTracedValue(perfetto::TracedValue context) const;

  base::WeakPtr<PageSchedulerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class FrameSchedulerImpl;
  friend class page_scheduler_impl_unittest::PageSchedulerImplTest;
  friend class page_scheduler_impl_unittest::
      PageSchedulerImplPageTransitionTest;
  friend class page_scheduler_impl_unittest::
      PageSchedulerImplPageTransitionTest_PageLifecycleStateTransitionMetric_Test;

  enum class AudioState {
    kSilent,
    kAudible,
    kRecentlyAudible,
  };

  enum class NotificationPolicy { kNotifyFrames, kDoNotNotifyFrames };

  // This enum is used for a histogram and should not be renumbered.
  // It tracks permissible page state transitions between PageLifecycleStates.
  // We allow all transitions except for visible to frozen and self transitions.
  enum class PageLifecycleStateTransition {
    kActiveToHiddenForegrounded = 0,
    kActiveToHiddenBackgrounded = 1,
    kHiddenForegroundedToActive = 2,
    kHiddenForegroundedToHiddenBackgrounded = 3,
    kHiddenForegroundedToFrozen = 4,
    kHiddenBackgroundedToActive = 5,
    kHiddenBackgroundedToHiddenForegrounded = 6,
    kHiddenBackgroundedToFrozen = 7,
    kFrozenToActive = 8,
    kFrozenToHiddenForegrounded = 9,
    kFrozenToHiddenBackgrounded = 10,
    kMaxValue = kFrozenToHiddenBackgrounded,
  };

  class PageLifecycleStateTracker {
    USING_FAST_MALLOC(PageLifecycleStateTracker);

   public:
    explicit PageLifecycleStateTracker(PageSchedulerImpl*, PageLifecycleState);
    ~PageLifecycleStateTracker() = default;

    void SetPageLifecycleState(PageLifecycleState);
    PageLifecycleState GetPageLifecycleState() const;

   private:
    static base::Optional<PageLifecycleStateTransition>
    ComputePageLifecycleStateTransition(PageLifecycleState old_state,
                                        PageLifecycleState new_state);

    static void RecordPageLifecycleStateTransition(
        PageLifecycleStateTransition);

    PageSchedulerImpl* page_scheduler_impl_;
    PageLifecycleState current_state_;

    DISALLOW_COPY_AND_ASSIGN(PageLifecycleStateTracker);
  };

  void RegisterFrameSchedulerImpl(FrameSchedulerImpl* frame_scheduler);

  // A page cannot be throttled or frozen 30 seconds after playing audio.
  //
  // This used to be 5 seconds, which was barely enough to cover the time of
  // silence during which a logo and button are shown after a YouTube ad. Since
  // most pages don't play audio in background, it was decided that the delay
  // can be increased to 30 seconds without significantly affecting performance.
  static constexpr base::TimeDelta kRecentAudioDelay =
      base::TimeDelta::FromSeconds(30);

  static const char kHistogramPageLifecycleStateTransition[];

  // Support not issuing a notification to frames when we disable freezing as
  // a part of foregrounding the page.
  void SetPageFrozenImpl(bool frozen, NotificationPolicy notification_policy);

  // Adds or removes a |task_queue| from the WakeUpBudgetPool associated with
  // |frame_origin_type|. When the FrameOriginType of a FrameScheduler changes,
  // it should remove all its TaskQueues from their current WakeUpBudgetPool and
  // add them back to the WakeUpBudgetPool appropriate for the new
  // FrameOriginType.
  void AddQueueToWakeUpBudgetPool(MainThreadTaskQueue* task_queue,
                                  FrameOriginType frame_origin_type,
                                  base::sequence_manager::LazyNow* lazy_now);
  void RemoveQueueFromWakeUpBudgetPool(
      MainThreadTaskQueue* task_queue,
      FrameOriginType frame_origin_type,
      base::sequence_manager::LazyNow* lazy_now);
  // Returns the WakeUpBudgetPool to use for |task_queue| which belongs to a
  // frame with |frame_origin_type|.
  WakeUpBudgetPool* GetWakeUpBudgetPool(MainThreadTaskQueue* task_queue,
                                        FrameOriginType frame_origin_type);
  // Initializes WakeUpBudgetPools, if not already initialized.
  void MaybeInitializeWakeUpBudgetPools(
      base::sequence_manager::LazyNow* lazy_now);

  CPUTimeBudgetPool* background_cpu_time_budget_pool();
  void MaybeInitializeBackgroundCPUTimeBudgetPool(
      base::sequence_manager::LazyNow* lazy_now);

  void OnThrottlingReported(base::TimeDelta throttling_duration);

  // Depending on page visibility, either turns throttling off, or schedules a
  // call to enable it after a grace period.
  void UpdatePolicyOnVisibilityChange(NotificationPolicy notification_policy);

  // Adjusts settings of budget pools depending on current state of the page.
  void UpdateCPUTimeBudgetPool(base::sequence_manager::LazyNow* lazy_now);
  void UpdateWakeUpBudgetPools(base::sequence_manager::LazyNow* lazy_now);
  base::TimeDelta GetIntensiveWakeUpThrottlingDuration(bool is_same_origin);

  // Callback for marking page is silent after a delay since last audible
  // signal.
  void OnAudioSilent();

  // Callbacks for adjusting the settings of a budget pool after a delay.
  // TODO(altimin): Trigger throttling depending on the loading state
  // of the page.
  void DoThrottleCPUTime();
  void DoIntensivelyThrottleWakeUps();
  void ResetHadRecentTitleOrFaviconUpdate();

  // Notify frames that the page scheduler state has been updated.
  void NotifyFrames();

  void EnableThrottling();

  // Returns true if the page is backgrounded, false otherwise. A page is
  // considered backgrounded if it is both not visible and not playing audio.
  bool IsBackgrounded() const;

  // Returns true if the page should be frozen after delay, which happens if
  // IsBackgrounded() and freezing is enabled.
  bool ShouldFreezePage() const;

  // Callback for freezing the page. Freezing must be enabled and the page must
  // be freezable.
  void DoFreezePage();

  // Returns true if WakeUpBudgetPools were initialized.
  bool HasWakeUpBudgetPools() const;

  // Returns all WakeUpBudgetPools owned by this PageSchedulerImpl.
  static constexpr int kNumWakeUpBudgetPools = 3;
  std::array<WakeUpBudgetPool*, kNumWakeUpBudgetPools> AllWakeUpBudgetPools();

  TraceableVariableController tracing_controller_;
  HashSet<FrameSchedulerImpl*> frame_schedulers_;
  MainThreadSchedulerImpl* main_thread_scheduler_;
  AgentGroupSchedulerImpl& agent_group_scheduler_;

  PageVisibilityState page_visibility_;
  base::TimeTicks page_visibility_changed_time_;
  AudioState audio_state_;
  bool is_frozen_;
  bool reported_background_throttling_since_navigation_;
  bool opted_out_from_aggressive_throttling_;
  bool nested_runloop_;
  bool is_main_frame_local_;
  bool is_cpu_time_throttled_;
  bool are_wake_ups_intensively_throttled_;
  bool keep_active_;
  bool had_recent_title_or_favicon_update_;
  bool focused_;
  CPUTimeBudgetPool* cpu_time_budget_pool_ = nullptr;

  // Wake up budget pools for each throttling scenario:
  //
  //                                  Same-origin frame    Cross-origin frame
  // Normal throttling only           1                    1
  // Normal and intensive throttling  2                    3
  //
  // 1: This pool allows 1-second aligned wake ups.
  WakeUpBudgetPool* normal_wake_up_budget_pool_ = nullptr;
  // 2: This pool allows 1-second aligned wake ups if the page is not
  //    intensively throttled of if there hasn't been a wake up in the last
  //    minute. Otherwise, it allows 1-minute aligned wake ups.
  WakeUpBudgetPool* same_origin_intensive_wake_up_budget_pool_ = nullptr;
  // 3: This pool allows 1-second aligned wake ups if the page is not
  //    intensively throttled. Otherwise, it allows 1-minute aligned wake ups.
  //
  //    Unlike |same_origin_intensive_wake_up_budget_pool_|, this pool does not
  //    allow a 1-second aligned wake up when there hasn't been a wake up in the
  //    last minute. This is to prevent frames from different origins from
  //    learning about each other. Concretely, this means that
  //    MaybeInitializeWakeUpBudgetPools() does not invoke
  //    AllowUnalignedWakeUpIfNoRecentWakeUp() on this pool.
  WakeUpBudgetPool* cross_origin_intensive_wake_up_budget_pool_ = nullptr;

  PageScheduler::Delegate* delegate_;
  CancelableClosureHolder do_throttle_cpu_time_callback_;
  CancelableClosureHolder do_intensively_throttle_wake_ups_callback_;
  CancelableClosureHolder reset_had_recent_title_or_favicon_update_;
  CancelableClosureHolder on_audio_silent_closure_;
  CancelableClosureHolder do_freeze_page_callback_;
  const base::TimeDelta delay_for_background_tab_freezing_;

  // Whether a background page can be frozen before
  // |delay_for_background_tab_freezing_| if network is idle.
  const bool freeze_on_network_idle_enabled_;

  // Delay after which a background page can be frozen if network is idle.
  const base::TimeDelta delay_for_background_and_network_idle_tab_freezing_;

  bool is_stored_in_back_forward_cache_ = false;
  TaskHandle set_ipc_posted_handler_task_;
  base::TimeTicks stored_in_back_forward_cache_timestamp_;

  std::unique_ptr<PageLifecycleStateTracker> page_lifecycle_state_tracker_;
  base::WeakPtrFactory<PageSchedulerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PageSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_
