// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_

#include <map>
#include <memory>
#include <stack>

#include "base/atomicops.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/single_sample_metrics.h"
#include "base/profiler/sample_metadata.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/deadline_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/idle_time_estimator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/render_widget_signals.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/user_model.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
class TaskObserver;
}  // namespace base

namespace blink {
namespace scheduler {
namespace frame_scheduler_impl_unittest {
class FrameSchedulerImplTest;
}  // namespace frame_scheduler_impl_unittest
namespace main_thread_scheduler_impl_unittest {
class MainThreadSchedulerImplForTest;
class MainThreadSchedulerImplTest;
class MockPageSchedulerImpl;
FORWARD_DECLARE_TEST(MainThreadSchedulerImplTest, ShouldIgnoreTaskForUkm);
FORWARD_DECLARE_TEST(MainThreadSchedulerImplTest, Tracing);
FORWARD_DECLARE_TEST(MainThreadSchedulerImplTest,
                     LogIpcsPostedToDocumentsInBackForwardCache);
}  // namespace main_thread_scheduler_impl_unittest
class AgentGroupSchedulerImpl;
class FrameSchedulerImpl;
class PageSchedulerImpl;
class TaskQueueThrottler;
class WebRenderWidgetSchedulingState;
class CPUTimeBudgetPool;

class PLATFORM_EXPORT MainThreadSchedulerImpl
    : public ThreadSchedulerImpl,
      public IdleHelper::Delegate,
      public MainThreadSchedulerHelper::Observer,
      public RenderWidgetSignals::Observer,
      public base::trace_event::TraceLog::AsyncEnabledStateObserver {
 public:
  // Don't use except for tracing.
  struct TaskDescriptionForTracing {
    TaskType task_type;
    absl::optional<MainThreadTaskQueue::QueueType> queue_type;

    // Required in order to wrap in TraceableState.
    constexpr bool operator!=(const TaskDescriptionForTracing& rhs) const {
      return task_type != rhs.task_type || queue_type != rhs.queue_type;
    }
  };

  struct SchedulingSettings {
    SchedulingSettings();

    // Background page priority experiment (crbug.com/848835).
    bool low_priority_background_page;
    bool best_effort_background_page;

    // Task and subframe priority experiment (crbug.com/852380).
    bool low_priority_subframe;
    bool low_priority_throttleable;
    bool low_priority_subframe_throttleable;
    bool low_priority_hidden_frame;

    // Used along with |low_priority_subframe|, |low_priority_throttleable|,
    // |low_priority_subframe_throttleable|, |low_priority_hidden_frame|
    // to enable one of these experiments during the loading phase only.
    bool use_frame_priorities_only_during_loading;

    // Ads priority experiment (crbug.com/856150).
    bool low_priority_ad_frame;
    bool best_effort_ad_frame;
    bool use_adframe_priorities_only_during_loading;

    // Origin type priority experiment (crbug.com/856158).
    bool low_priority_cross_origin;
    bool low_priority_cross_origin_only_during_loading;

    // Use resource fetch priority for resource loading tasks
    // (crbug.com/860545).
    bool use_resource_fetch_priority;
    bool use_resource_priorities_only_during_loading;

    // Prioritize compositing and loading tasks until first contentful paint.
    // (crbug.com/971191)
    bool prioritize_compositing_and_loading_during_early_loading;

    // Prioritise one BeginMainFrame after an input task.
    bool prioritize_compositing_after_input;

    // Contains a mapping from net::RequestPriority to TaskQueue::QueuePriority
    // when use_resource_fetch_priority is enabled.
    std::array<base::sequence_manager::TaskQueue::QueuePriority,
               net::RequestPrioritySize::NUM_PRIORITIES>
        net_to_blink_priority;

    // If enabled, base::ThreadTaskRunnerHandle::Get() and
    // base::SequencedTaskRunnerHandle::Get() returns the current active
    // per-ASG task runner instead of the per-thread task runner.
    bool mbi_override_task_runner_handle;

    // If enabled, per-AgentGroupScheduler CompositorTaskRunner will be used
    // instead of per-MainThreadScheduler CompositorTaskRunner.
    bool mbi_compositor_task_runner_per_agent_scheduling_group;
  };

  static const char* UseCaseToString(UseCase use_case);
  static const char* RAILModeToString(RAILMode rail_mode);
  static const char* VirtualTimePolicyToString(
      PageScheduler::VirtualTimePolicy);

  explicit MainThreadSchedulerImpl(
      std::unique_ptr<base::sequence_manager::SequenceManager>
          sequence_manager);
  MainThreadSchedulerImpl(const MainThreadSchedulerImpl&) = delete;
  MainThreadSchedulerImpl& operator=(const MainThreadSchedulerImpl&) = delete;

  ~MainThreadSchedulerImpl() override;

  // WebThreadScheduler implementation:
  std::unique_ptr<Thread> CreateMainThread() override;
  std::unique_ptr<WebWidgetScheduler> CreateWidgetScheduler() override;
  // Note: this is also shared by the ThreadScheduler interface.
  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override;
  std::unique_ptr<WebRenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() override;
  void WillBeginFrame(const viz::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void WillPostInputEventToMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override;
  void WillHandleInputEventOnMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  void DidAnimateForInputOnCompositorThread() override;
  void DidScheduleBeginMainFrame() override;
  void DidRunBeginMainFrame() override;
  void SetRendererHidden(bool hidden) override;
  void SetRendererBackgrounded(bool backgrounded) override;
  void OnMainFrameRequestedForInput() override;
#if BUILDFLAG(IS_ANDROID)
  void PauseTimersForAndroidWebView() override;
  void ResumeTimersForAndroidWebView() override;
#endif
  [[nodiscard]] std::unique_ptr<ThreadScheduler::RendererPauseHandle>
  PauseRenderer() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::TaskObserver* task_observer) override;
  void RemoveTaskObserver(base::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void AddRAILModeObserver(RAILModeObserver* observer) override;
  void RemoveRAILModeObserver(RAILModeObserver const* observer) override;
  void SetRendererProcessType(WebRendererProcessType type) override;
  Vector<WebInputEventAttribution> GetPendingUserInputInfo(
      bool include_continuous) const override;
  bool IsBeginMainFrameScheduled() const override;

  // ThreadScheduler implementation:
  void PostIdleTask(const base::Location&, Thread::IdleTask) override;
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override;
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta delay,
                           Thread::IdleTask) override;
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override;
  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override;
  std::unique_ptr<ThreadScheduler::RendererPauseHandle> PauseScheduler()
      override;
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override;
  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }

  // Use a separate task runner so that IPC tasks are not logged via the same
  // task queue that executes them. Otherwise this would result in an infinite
  // loop of posting and logging to a single queue.
  scoped_refptr<base::SingleThreadTaskRunner>
  BackForwardCacheIpcTrackingTaskRunner() {
    return back_forward_cache_ipc_tracking_task_runner_;
  }

  // WebThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;

  // The following functions are defined in both WebThreadScheduler and
  // ThreadScheduler, and have the same function signatures -- see above.
  // This class implements those functions for both base classes.
  //
  // void Shutdown() override;
  //
  // TODO(yutak): Reduce the overlaps and simplify.

  // RenderWidgetSignals::Observer implementation:
  void SetAllRenderWidgetsHidden(bool hidden) override;
  void SetHasVisibleRenderWidgetWithTouchHandler(
      bool has_visible_render_widget_with_touch_handler) override;

  // SchedulerHelper::Observer implementation:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // ThreadSchedulerImpl implementation:
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner() override;
  const base::TickClock* GetTickClock() const override;

  base::TimeTicks NowTicks() const;

  scoped_refptr<base::SingleThreadTaskRunner> VirtualTimeControlTaskRunner();

  // Returns a new task queue created with given params.
  scoped_refptr<MainThreadTaskQueue> NewTaskQueue(
      const MainThreadTaskQueue::QueueCreationParams& params);

  // Returns a new loading task queue. This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  // Note: Tasks posted to kFrameLoadingControl queues must execute quickly.
  scoped_refptr<MainThreadTaskQueue> NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType queue_type,
      FrameSchedulerImpl* frame_scheduler);

  // Returns a new throttleable task queue to be used for tests.
  scoped_refptr<MainThreadTaskQueue> NewThrottleableTaskQueueForTest(
      FrameSchedulerImpl* frame_scheduler);

  scoped_refptr<base::sequence_manager::TaskQueue> NewTaskQueueForTest();

  using VirtualTimePolicy = PageScheduler::VirtualTimePolicy;

  // Tells the scheduler that all TaskQueues should use virtual time. Returns
  // the base::TimeTicks that virtual time offsets will be relative to.
  base::TimeTicks EnableVirtualTime(base::Time initial_time);
  bool IsVirtualTimeEnabled() const;

  // Migrates all task queues to real time.
  void DisableVirtualTimeForTesting();

  // Returns true if virtual time is not paused.
  bool VirtualTimeAllowedToAdvance() const;
  void SetVirtualTimePolicy(VirtualTimePolicy virtual_time_policy);
  void SetMaxVirtualTimeTaskStarvationCount(int max_task_starvation_count);
  base::TimeTicks IncrementVirtualTimePauseCount();
  void DecrementVirtualTimePauseCount();
  void MaybeAdvanceVirtualTime(base::TimeTicks new_virtual_time);

  void RemoveAgentGroupScheduler(AgentGroupSchedulerImpl*);
  void AddPageScheduler(PageSchedulerImpl*);
  void RemovePageScheduler(PageSchedulerImpl*);

  // Called by an associated PageScheduler when frozen or resumed.
  void OnPageFrozen();
  void OnPageResumed();

  void AddTaskTimeObserver(base::sequence_manager::TaskTimeObserver*);
  void RemoveTaskTimeObserver(base::sequence_manager::TaskTimeObserver*);

  // Snapshots this MainThreadSchedulerImpl for tracing.
  void CreateTraceEventObjectSnapshot() const;

  // Called when one of associated page schedulers has changed audio state.
  void OnAudioStateChanged();

  // Tells the scheduler that a provisional load has committed. Must be called
  // from the main thread.
  void DidStartProvisionalLoad(bool is_main_frame);

  // Tells the scheduler that a provisional load has committed. The scheduler
  // may reset the task cost estimators and the UserModel. Must be called from
  // the main thread.
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame);

  // Note that the main's thread policy should be upto date to compute
  // the correct priority.
  base::sequence_manager::TaskQueue::QueuePriority ComputePriority(
      MainThreadTaskQueue* task_queue) const;

  // Test helpers.
  MainThreadSchedulerHelper* GetSchedulerHelperForTesting();
  IdleTimeEstimator* GetIdleTimeEstimatorForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;
  void RunIdleTasksForTesting(base::OnceClosure callback);
  void EndIdlePeriodForTesting(base::OnceClosure callback,
                               base::TimeTicks time_remaining);
  bool PolicyNeedsUpdateForTesting();

  AutoAdvancingVirtualTimeDomain* GetVirtualTimeDomain();

  std::unique_ptr<CPUTimeBudgetPool> CreateCPUTimeBudgetPoolForTesting(
      const char* name);

  // Virtual for test.
  virtual void OnMainFramePaint();

  void OnShutdownTaskQueue(const scoped_refptr<MainThreadTaskQueue>& queue);

  void OnTaskStarted(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void OnTaskCompleted(
      base::WeakPtr<MainThreadTaskQueue> queue,
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now);

  void UpdateIpcTracking();
  void SetOnIPCTaskPostedWhileInBackForwardCacheIfNeeded();
  void OnIPCTaskPostedWhileInAllPagesBackForwardCache(
      uint32_t ipc_hash,
      const char* ipc_interface_name);
  void DetachOnIPCTaskPostedWhileInBackForwardCacheHandler();

  bool IsAudioPlaying() const;

  // base::trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  UseCase current_use_case() const;

  const SchedulingSettings& scheduling_settings() const;

  void SetPrioritizeCompositingAfterInput(
      bool prioritize_compositing_after_input);

  base::WeakPtr<MainThreadSchedulerImpl> GetWeakPtr();

  base::sequence_manager::TaskQueue::QueuePriority compositor_priority() const {
    return main_thread_only().compositor_priority;
  }

  bool should_prioritize_loading_with_compositing() const {
    return main_thread_only()
        .current_policy.should_prioritize_loading_with_compositing();
  }

  bool main_thread_compositing_is_fast() const {
    return main_thread_only().main_thread_compositing_is_fast;
  }

  QueuePriority find_in_page_priority() const {
    return main_thread_only().current_policy.find_in_page_priority();
  }

 protected:
  // ThreadSchedulerImpl implementation:
  WTF::Vector<base::OnceClosure>& GetOnTaskCompletionCallbacks() override;

  scoped_refptr<MainThreadTaskQueue> ControlTaskQueue();
  scoped_refptr<MainThreadTaskQueue> DefaultTaskQueue();
  scoped_refptr<MainThreadTaskQueue> CompositorTaskQueue();
  scoped_refptr<MainThreadTaskQueue> V8TaskQueue();
  // A control task queue which also respects virtual time. Only available if
  // virtual time has been enabled.
  scoped_refptr<MainThreadTaskQueue> VirtualTimeControlTaskQueue();

  // `current_use_case` will be overwritten by the next call to UpdatePolicy.
  // Thus, this function should be only used for testing purposes.
  void SetCurrentUseCaseForTest(UseCase use_case) {
    main_thread_only().current_use_case = use_case;
  }

  void SetHaveSeenABlockingGestureForTesting(bool status);

  virtual void PerformMicrotaskCheckpoint();

 private:
  friend class WebRenderWidgetSchedulingState;
  friend class MainThreadMetricsHelper;

  friend class MainThreadMetricsHelperTest;
  friend class frame_scheduler_impl_unittest::FrameSchedulerImplTest;
  friend class main_thread_scheduler_impl_unittest::
      MainThreadSchedulerImplForTest;
  friend class main_thread_scheduler_impl_unittest::MockPageSchedulerImpl;
  friend class main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest;

  friend class FindInPageBudgetPoolController;

  FRIEND_TEST_ALL_PREFIXES(
      main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest,
      ShouldIgnoreTaskForUkm);
  FRIEND_TEST_ALL_PREFIXES(
      main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest,
      Tracing);
  FRIEND_TEST_ALL_PREFIXES(
      main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest,
      LogIpcsPostedToDocumentsInBackForwardCache);

  enum class TimeDomainType {
    kReal,
    kVirtual,
  };

  // WebThreadScheduler private implementation:
  WebThreadScheduler* GetWebMainThreadScheduler() override;

  static const char* TimeDomainTypeToString(TimeDomainType domain_type);

  void AddAgentGroupScheduler(AgentGroupSchedulerImpl*);

  struct AgentGroupSchedulerScope {
    std::unique_ptr<base::ThreadTaskRunnerHandleOverride>
        thread_task_runner_handle_override;
    WebAgentGroupScheduler* previous_agent_group_scheduler;
    WebAgentGroupScheduler* current_agent_group_scheduler;
    scoped_refptr<base::SingleThreadTaskRunner> previous_task_runner;
    scoped_refptr<base::SingleThreadTaskRunner> current_task_runner;
    const char* trace_event_scope_name;
    void* trace_event_scope_id;
  };

  void BeginAgentGroupSchedulerScope(
      WebAgentGroupScheduler* next_agent_group_scheduler);
  void EndAgentGroupSchedulerScope();

  bool IsAnyMainFrameWaitingForFirstContentfulPaint() const;
  bool IsAnyMainFrameWaitingForFirstMeaningfulPaint() const;

  class Policy {
    DISALLOW_NEW();

   public:
    Policy() = default;
    ~Policy() = default;

    RAILMode& rail_mode() { return rail_mode_; }
    RAILMode rail_mode() const { return rail_mode_; }

    bool& frozen_when_backgrounded() { return frozen_when_backgrounded_; }
    bool frozen_when_backgrounded() const { return frozen_when_backgrounded_; }

    bool& should_prioritize_loading_with_compositing() {
      return should_prioritize_loading_with_compositing_;
    }
    bool should_prioritize_loading_with_compositing() const {
      return should_prioritize_loading_with_compositing_;
    }

    bool& should_freeze_compositor_task_queue() {
      return should_freeze_compositor_task_queue_;
    }
    bool should_freeze_compositor_task_queue() const {
      return should_freeze_compositor_task_queue_;
    }

    bool& should_defer_task_queues() { return should_defer_task_queues_; }
    bool should_defer_task_queues() const { return should_defer_task_queues_; }

    bool& should_pause_task_queues() { return should_pause_task_queues_; }
    bool should_pause_task_queues() const { return should_pause_task_queues_; }

    bool& should_pause_task_queues_for_android_webview() {
      return should_pause_task_queues_for_android_webview_;
    }
    bool should_pause_task_queues_for_android_webview() const {
      return should_pause_task_queues_for_android_webview_;
    }

    base::sequence_manager::TaskQueue::QueuePriority& find_in_page_priority() {
      return find_in_page_priority_;
    }
    base::sequence_manager::TaskQueue::QueuePriority find_in_page_priority()
        const {
      return find_in_page_priority_;
    }

    UseCase& use_case() { return use_case_; }
    UseCase use_case() const { return use_case_; }

    bool operator==(const Policy& other) const {
      return rail_mode_ == other.rail_mode_ &&
             frozen_when_backgrounded_ == other.frozen_when_backgrounded_ &&
             should_prioritize_loading_with_compositing_ ==
                 other.should_prioritize_loading_with_compositing_ &&
             should_freeze_compositor_task_queue_ ==
                 other.should_freeze_compositor_task_queue_ &&
             should_defer_task_queues_ == other.should_defer_task_queues_ &&
             should_pause_task_queues_ == other.should_pause_task_queues_ &&
             should_pause_task_queues_for_android_webview_ ==
                 other.should_pause_task_queues_for_android_webview_ &&
             find_in_page_priority_ == other.find_in_page_priority_ &&
             use_case_ == other.use_case_;
    }

    bool IsQueueEnabled(MainThreadTaskQueue* task_queue) const;

    void WriteIntoTrace(perfetto::TracedValue context) const;

   private:
    RAILMode rail_mode_{RAILMode::kAnimation};
    bool frozen_when_backgrounded_{false};
    bool should_prioritize_loading_with_compositing_{false};
    bool should_freeze_compositor_task_queue_{false};
    bool should_defer_task_queues_{false};
    bool should_pause_task_queues_{false};
    bool should_pause_task_queues_for_android_webview_{false};

    base::sequence_manager::TaskQueue::QueuePriority find_in_page_priority_{
        FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority};

    UseCase use_case_{UseCase::kNone};
  };

  class TaskDurationMetricTracker;

  class RendererPauseHandleImpl : public ThreadScheduler::RendererPauseHandle {
   public:
    explicit RendererPauseHandleImpl(MainThreadSchedulerImpl* scheduler);
    ~RendererPauseHandleImpl() override;

   private:
    MainThreadSchedulerImpl* scheduler_;  // NOT OWNED
  };

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override;
  void OnIdlePeriodEnded() override;
  void OnPendingTasksChanged(bool has_tasks) override;

  void DispatchRequestBeginMainFrameNotExpected(bool has_tasks);

  void EndIdlePeriod();

  // Update a policy which increases priority for the next beginMainFrame after
  // an input event.
  void UpdatePrioritizeCompositingAfterInputAfterTaskCompleted(
      MainThreadTaskQueue* queue);

  // Returns the serialized scheduler state for tracing.
  void WriteIntoTraceLocked(perfetto::TracedValue context,
                            base::TimeTicks optional_now) const;
  void CreateTraceEventObjectSnapshotLocked() const;

  static bool ShouldPrioritizeInputEvent(const WebInputEvent& web_input_event);

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time in milliseconds we have to respond to user input as
  // defined by RAILS.
  static const int kRailsResponseTimeMillis = 50;

  // The time we should stay in a priority-escalated mode after a call to
  // DidAnimateForInputOnCompositorThread().
  static const int kFlingEscalationLimitMillis = 100;

  // Schedules an immediate PolicyUpdate, if there isn't one already pending and
  // sets |policy_may_need_update_|. Note |any_thread_lock_| must be
  // locked.
  void EnsureUrgentPolicyUpdatePostedOnMainThread(
      const base::Location& from_here);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Locks |any_thread_lock_| and updates the scheduler policy.  May early
  // out if the policy is unchanged. Must be called from the main thread.
  void UpdatePolicy();

  // Like UpdatePolicy, except it doesn't early out.
  void ForceUpdatePolicy();

  enum class UpdateType {
    kMayEarlyOutIfPolicyUnchanged,
    kForceUpdate,
  };

  // The implementation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
  // early out if |update_type| is kMayEarlyOutIfPolicyUnchanged.
  virtual void UpdatePolicyLocked(UpdateType update_type);

  // Helper for computing the use case. |expected_usecase_duration| will be
  // filled with the amount of time after which the use case should be updated
  // again. If the duration is zero, a new use case update should not be
  // scheduled. Must be called with |any_thread_lock_| held. Can be called from
  // any thread.
  UseCase ComputeCurrentUseCase(
      base::TimeTicks now,
      base::TimeDelta* expected_use_case_duration) const;

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(const WebInputEvent& event,
                                             InputEventState input_event_state);

  // The task cost estimators and the UserModel need to be reset upon page
  // nagigation. This function does that. Must be called from the main thread.
  void ResetForNavigationLocked();

  // Estimates the maximum task length that won't cause a jank based on the
  // current system state. Must be called from the main thread.
  base::TimeDelta EstimateLongestJankFreeTaskDuration() const;

  // Report an intervention to all WebViews in this process.
  void BroadcastIntervention(const String& message);

  // Trigger an update to all task queues' priorities, throttling, and
  // enabled/disabled state based on current policy. When triggered from a
  // policy update, |previous_policy| should be populated with the pre-update
  // policy.
  void UpdateStateForAllTaskQueues(absl::optional<Policy> previous_policy);

  void UpdateTaskQueueState(
      MainThreadTaskQueue* task_queue,
      base::sequence_manager::TaskQueue::QueueEnabledVoter*
          task_queue_enabled_voter,
      const Policy& old_policy,
      const Policy& new_policy,
      bool should_update_priority) const;

  void PauseRendererImpl();
  void ResumeRendererImpl();

  void NotifyVirtualTimePaused();
  void SetVirtualTimeStopped(bool virtual_time_stopped);
  void ApplyVirtualTimePolicy();

  // Pauses the timer queues by inserting a fence that blocks any tasks posted
  // after this point from running. Orthogonal to PauseTimerQueue. Care must
  // be taken when using this API to avoid fighting with the TaskQueueThrottler.
  void VirtualTimePaused();

  // Removes the fence added by VirtualTimePaused allowing timers to execute
  // normally. Care must be taken when using this API to avoid fighting with the
  // TaskQueueThrottler.
  void VirtualTimeResumed();

  // Returns true if there is a change in the main thread's policy that should
  // trigger a priority update.
  bool ShouldUpdateTaskQueuePriorities(Policy new_policy) const;

  // Computes compositor priority based on various experiments and
  // the use case. Defaults to kNormalPriority.
  TaskQueue::QueuePriority ComputeCompositorPriority() const;

  // Used to update the compositor priority on the main thread.
  void UpdateCompositorTaskQueuePriority();

  // Called from OnTaskCompleted, this method checks to see if the compositor
  // task queue priority needs to be updated.
  void MaybeUpdateCompositorTaskQueuePriorityOnTaskCompleted(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  // Computes the priority for compositing based on the current use case.
  // Returns nullopt if the use case does not need to set the priority.
  absl::optional<TaskQueue::QueuePriority>
  ComputeCompositorPriorityFromUseCase() const;

  static void RunIdleTask(Thread::IdleTask, base::TimeTicks deadline);

  // Probabilistically record all task metadata for the current task.
  // If task belongs to a per-frame queue, this task is attributed to
  // a particular Page, otherwise it's attributed to all Pages in the process.
  void RecordTaskUkm(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  UkmRecordingStatus RecordTaskUkmImpl(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing,
      FrameSchedulerImpl* frame_scheduler,
      bool precise_attribution);

  void SetNumberOfCompositingTasksToPrioritize(int number_of_tasks);

  void ShutdownAllQueues();

  bool AllPagesFrozen() const;

  // Indicates that scheduler has been shutdown.
  // It should be accessed only on the main thread, but couldn't be a member
  // of MainThreadOnly struct because last might be destructed before we
  // have to check this flag during scheduler's destruction.
  bool was_shutdown_ = false;

  bool has_ipc_callback_set_ = false;
  bool IsIpcTrackingEnabledForAllPages();

  // This controller should be initialized before any TraceableVariables
  // because they require one to initialize themselves.
  TraceableVariableController tracing_controller_;

  // Used for experiments on finch. On main thread instantiation, we cache
  // the values of base::Feature flags using this struct, since calling
  // base::Feature::IsEnabled is a relatively expensive operation.
  //
  // Note that it is important to keep this as the first member to ensure it is
  // initialized first and can be used everywhere.
  const SchedulingSettings scheduling_settings_;

  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  MainThreadSchedulerHelper helper_;
  scoped_refptr<MainThreadTaskQueue> idle_helper_queue_;
  IdleHelper idle_helper_;
  RenderWidgetSignals render_widget_scheduler_signals_;

  std::unique_ptr<FindInPageBudgetPoolController>
      find_in_page_budget_pool_controller_;

  const scoped_refptr<MainThreadTaskQueue> control_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  scoped_refptr<MainThreadTaskQueue> virtual_time_control_task_queue_;
  scoped_refptr<MainThreadTaskQueue>
      back_forward_cache_ipc_tracking_task_queue_;

  using TaskQueueVoterMap = std::map<
      scoped_refptr<MainThreadTaskQueue>,
      std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>>;

  TaskQueueVoterMap task_runners_;

  scoped_refptr<MainThreadTaskQueue> v8_task_queue_;
  scoped_refptr<MainThreadTaskQueue> memory_purge_task_queue_;
  scoped_refptr<MainThreadTaskQueue> non_waking_task_queue_;

  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> non_waking_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      back_forward_cache_ipc_tracking_task_runner_;

  MemoryPurgeManager memory_purge_manager_;

  // Note |virtual_time_domain_| is lazily created.
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> virtual_time_domain_;

  base::RepeatingClosure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.
  struct MainThreadOnly {
    MainThreadOnly(
        MainThreadSchedulerImpl* main_thread_scheduler_impl,
        const base::TickClock* time_source,
        base::TimeTicks now);
    ~MainThreadOnly();

    bool IsInNestedRunloop();

    IdleTimeEstimator idle_time_estimator;
    TraceableState<UseCase, TracingCategory::kDefault> current_use_case;
    Policy current_policy;
    base::TimeTicks current_policy_expiration_time;
    base::TimeTicks estimated_next_frame_begin;
    base::TimeTicks current_task_start_time;
    base::TimeDelta compositor_frame_interval;
    TraceableCounter<base::TimeDelta, TracingCategory::kDebug>
        longest_jank_free_task_duration;
    TraceableCounter<int, TracingCategory::kInfo>
        renderer_pause_count;  // Renderer is paused if non-zero.

    TraceableObjectState<RAILMode,
                         TracingCategory::kTopLevel>
        rail_mode_for_tracing;  // Don't use except for tracing.

    TraceableObjectState<bool, TracingCategory::kTopLevel> renderer_hidden;
    absl::optional<base::ScopedSampleMetadata> renderer_hidden_metadata;
    TraceableObjectState<bool, TracingCategory::kTopLevel>
        renderer_backgrounded;
    TraceableState<bool, TracingCategory::kDefault>
        blocking_input_expected_soon;
    TraceableState<bool, TracingCategory::kDebug>
        have_reported_blocking_intervention_in_current_policy;
    TraceableState<bool, TracingCategory::kDebug>
        have_reported_blocking_intervention_since_navigation;
    TraceableState<bool, TracingCategory::kDebug>
        has_visible_render_widget_with_touch_handler;
    TraceableState<bool, TracingCategory::kDebug> in_idle_period_for_testing;
    TraceableState<bool, TracingCategory::kInfo> use_virtual_time;
    TraceableState<bool, TracingCategory::kTopLevel> is_audio_playing;
    TraceableState<bool, TracingCategory::kDebug>
        compositor_will_send_main_frame_not_expected;
    TraceableState<bool, TracingCategory::kDebug> has_navigated;
    TraceableState<bool, TracingCategory::kDebug> pause_timers_for_webview;
    base::TimeTicks background_status_changed_at;
    HashSet<PageSchedulerImpl*> page_schedulers;  // Not owned.
    base::ObserverList<RAILModeObserver>::Unchecked
        rail_mode_observers;  // Not owned.
    MainThreadMetricsHelper metrics_helper;
    TraceableState<WebRendererProcessType, TracingCategory::kTopLevel>
        process_type;
    TraceableState<absl::optional<TaskDescriptionForTracing>,
                   TracingCategory::kInfo>
        task_description_for_tracing;  // Don't use except for tracing.
    TraceableState<
        absl::optional<base::sequence_manager::TaskQueue::QueuePriority>,
        TracingCategory::kInfo>
        task_priority_for_tracing;  // Only used for tracing.
    base::Time initial_virtual_time;
    base::TimeTicks initial_virtual_time_ticks;

    VirtualTimePolicy virtual_time_policy;

    // In VirtualTimePolicy::kDeterministicLoading virtual time is only allowed
    // to advance if this is zero.
    int virtual_time_pause_count;

    // The maximum number amount of delayed task starvation we will allow in
    // VirtualTimePolicy::kAdvance or VirtualTimePolicy::kDeterministicLoading
    // unless the run_loop is nested (in which case infinite starvation is
    // allowed). NB a value of 0 allows infinite starvation.
    int max_virtual_time_task_starvation_count;
    bool virtual_time_stopped;

    // Holds task queues that are currently running.
    // The queue for the inmost task is at the top of stack when there are
    // nested RunLoops.
    std::stack<scoped_refptr<MainThreadTaskQueue>,
               std::vector<scoped_refptr<MainThreadTaskQueue>>>
        running_queues;

    // Depth of nested_runloop.
    int nested_runloop_depth = 0;

    // High-priority for compositing events after input. This will cause
    // compositing events get a higher priority until the start of the next
    // animation frame.
    TraceableState<bool, TracingCategory::kDefault>
        prioritize_compositing_after_input;

    // List of callbacks to execute after the current task.
    WTF::Vector<base::OnceClosure> on_task_completion_callbacks;

    bool main_thread_compositing_is_fast;

    // Priority given to the main thread's compositor task queue. Defaults to
    // kNormalPriority and is updated via UpdateCompositorTaskQueuePriority().
    // After 100ms with nothing running from this queue, the compositor will
    // be set to kVeryHighPriority until a frame is run.
    TraceableState<TaskQueue::QueuePriority, TracingCategory::kDefault>
        compositor_priority;
    base::TimeTicks last_frame_time;
    bool should_prioritize_compositor_task_queue_after_delay;
    bool have_seen_a_frame;

    WTF::Vector<AgentGroupSchedulerScope> agent_group_scheduler_scope_stack;

    std::unique_ptr<power_scheduler::PowerModeVoter> audible_power_mode_voter;
  };

  struct AnyThread {
    explicit AnyThread(MainThreadSchedulerImpl* main_thread_scheduler_impl);
    ~AnyThread();

    PendingUserInput::Monitor pending_input_monitor;
    base::TimeTicks last_idle_period_end_time;
    base::TimeTicks fling_compositor_escalation_deadline;
    UserModel user_model;
    TraceableState<bool, TracingCategory::kInfo> awaiting_touch_start_response;
    TraceableState<bool, TracingCategory::kInfo> in_idle_period;
    TraceableState<bool, TracingCategory::kInfo>
        begin_main_frame_on_critical_path;
    TraceableState<bool, TracingCategory::kInfo>
        last_gesture_was_compositor_driven;
    TraceableState<bool, TracingCategory::kInfo> default_gesture_prevented;
    TraceableState<bool, TracingCategory::kInfo> have_seen_a_blocking_gesture;
    TraceableState<bool, TracingCategory::kInfo>
        waiting_for_any_main_frame_contentful_paint;
    TraceableState<bool, TracingCategory::kInfo>
        waiting_for_any_main_frame_meaningful_paint;
    TraceableState<bool, TracingCategory::kInfo>
        have_seen_input_since_navigation;
    TraceableCounter<uint32_t, TracingCategory::kInfo>
        begin_main_frame_scheduled_count;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    WebInputEvent::Type last_input_type;
    std::unique_ptr<base::ThreadChecker> compositor_thread_checker;

    void CheckOnValidThread() {
#if DCHECK_IS_ON()
      // We don't actually care which thread this called from, just so long as
      // its consistent.
      if (!compositor_thread_checker)
        compositor_thread_checker.reset(new base::ThreadChecker());
      DCHECK(compositor_thread_checker->CalledOnValidThread());
#endif
    }
  };

  // Don't access main_thread_only_, instead use main_thread_only().
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }
  const struct MainThreadOnly& main_thread_only() const {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }

  mutable base::Lock any_thread_lock_;
  // Don't access any_thread_, instead use any_thread().
  AnyThread any_thread_;
  AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  // Don't access compositor_thread_only_, instead use
  // |GetCompositorThreadOnly()|.
  CompositorThreadOnly compositor_thread_only_;
  CompositorThreadOnly& GetCompositorThreadOnly() {
    compositor_thread_only_.CheckOnValidThread();
    return compositor_thread_only_;
  }

  PollableThreadSafeFlag policy_may_need_update_;
  WTF::HashSet<AgentGroupSchedulerImpl*> agent_group_schedulers_;
  WebAgentGroupScheduler* current_agent_group_scheduler_{nullptr};

  base::WeakPtrFactory<MainThreadSchedulerImpl> weak_factory_{this};
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_
