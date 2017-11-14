// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/single_sample_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/scheduler/child/idle_canceled_delayed_task_sweeper.h"
#include "platform/scheduler/child/idle_helper.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/deadline_task_runner.h"
#include "platform/scheduler/renderer/idle_time_estimator.h"
#include "platform/scheduler/renderer/main_thread_scheduler_helper.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"
#include "platform/scheduler/renderer/pollable_thread_safe_flag.h"
#include "platform/scheduler/renderer/queueing_time_estimator.h"
#include "platform/scheduler/renderer/render_widget_signals.h"
#include "platform/scheduler/renderer/renderer_metrics_helper.h"
#include "platform/scheduler/renderer/task_cost_estimator.h"
#include "platform/scheduler/renderer/user_model.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/util/tracing_helper.h"
#include "public/platform/scheduler/renderer/renderer_scheduler.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace blink {
namespace scheduler {
namespace renderer_scheduler_impl_unittest {
class RendererSchedulerImplForTest;
class RendererSchedulerImplTest;
FORWARD_DECLARE_TEST(RendererSchedulerImplTest, Tracing);
}  // namespace renderer_scheduler_impl_unittest
class RenderWidgetSchedulingState;
class WebViewSchedulerImpl;
class TaskQueueThrottler;

class PLATFORM_EXPORT RendererSchedulerImpl
    : public RendererScheduler,
      public IdleHelper::Delegate,
      public MainThreadSchedulerHelper::Observer,
      public RenderWidgetSignals::Observer,
      public QueueingTimeEstimator::Client,
      public base::trace_event::TraceLog::AsyncEnabledStateObserver,
      public AutoAdvancingVirtualTimeDomain::Observer {
 public:
  // Keep RendererScheduler::UseCaseToString in sync with this enum.
  enum class UseCase {
    // No active use case detected.
    kNone,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread.
    kCompositorGesture,
    // An unspecified touch gesture which is being handled by the main thread.
    // Note that since we don't have a full view of the use case, we should be
    // careful to prioritize all work equally.
    kMainThreadCustomInputHandling,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread but also observed by the main thread. An example is
    // synchronized scrolling where a scroll listener on the main thread changes
    // page layout based on the current scroll position.
    kSynchronizedGesture,
    // A gesture has recently started and we are about to run main thread touch
    // listeners to find out the actual gesture type. To minimize touch latency,
    // only input handling work should run in this state.
    kTouchstart,
    // A page is loading.
    kLoading,
    // A continuous gesture (e.g., scroll) which is being handled by the main
    // thread.
    kMainThreadGesture,
    // Must be the last entry.
    kUseCaseCount,
    kFirstUseCase = kNone,
  };
  static const char* UseCaseToString(UseCase use_case);
  static const char* RAILModeToString(v8::RAILMode rail_mode);
  static const char* VirtualTimePolicyToString(
      WebViewScheduler::VirtualTimePolicy virtual_time_policy);

  RendererSchedulerImpl(scoped_refptr<SchedulerTqmDelegate> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  std::unique_ptr<WebThread> CreateMainThread() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  std::unique_ptr<RenderWidgetSchedulingState> NewRenderWidgetSchedulingState()
      override;
  void WillBeginFrame(const viz::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  base::TimeDelta MostRecentExpectedQueueingTime() override;
  void DidAnimateForInputOnCompositorThread() override;
  void SetRendererHidden(bool hidden) override;
  void SetRendererBackgrounded(bool backgrounded) override;
#if defined(OS_ANDROID)
  void PauseTimersForAndroidWebView();
  void ResumeTimersForAndroidWebView();
#endif
  std::unique_ptr<RendererPauseHandle> PauseRenderer() override
      WARN_UNUSED_RESULT;
  void AddPendingNavigation(NavigatingFrameType type) override;
  void RemovePendingNavigation(NavigatingFrameType type) override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SetStoppingWhenBackgroundedEnabled(bool enabled) override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void SetRAILModeObserver(RAILModeObserver* observer) override;
  bool MainThreadSeemsUnresponsive(
      base::TimeDelta main_thread_responsiveness_threshold) override;
  void SetRendererProcessType(RendererProcessType type) override;

  // AutoAdvancingVirtualTimeDomain::Observer implementation:
  void OnVirtualTimeAdvanced() override;

  // RenderWidgetSignals::Observer implementation:
  void SetAllRenderWidgetsHidden(bool hidden) override;
  void SetHasVisibleRenderWidgetWithTouchHandler(
      bool has_visible_render_widget_with_touch_handler) override;

  // SchedulerHelper::Observer implementation:
  void OnTriedToExecuteBlockedTask() override;
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // QueueingTimeEstimator::Client implementation:
  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override;
  void OnReportSplitExpectedQueueingTime(
      const char* split_description,
      base::TimeDelta queueing_time) override;

  scoped_refptr<MainThreadTaskQueue> DefaultTaskQueue();
  scoped_refptr<MainThreadTaskQueue> CompositorTaskQueue();
  scoped_refptr<MainThreadTaskQueue> LoadingTaskQueue();
  scoped_refptr<MainThreadTaskQueue> TimerTaskQueue();
  scoped_refptr<MainThreadTaskQueue> kV8TaskQueue();

  // Returns a new task queue created with given params.
  scoped_refptr<MainThreadTaskQueue> NewTaskQueue(
      const MainThreadTaskQueue::QueueCreationParams& params);

  // Returns a new loading task queue. This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  // Note: Tasks posted to kFrameLoading_kControl queues must execute quickly.
  scoped_refptr<MainThreadTaskQueue> NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType queue_type);

  // Returns a new timer task queue. This queue is intended for DOM Timers.
  scoped_refptr<MainThreadTaskQueue> NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType queue_type);

  // Returns a task queue where tasks run at the highest possible priority.
  scoped_refptr<MainThreadTaskQueue> ControlTaskQueue();

  // A control task queue which also respects virtual time. Only available if
  // virtual time has been enabled.
  scoped_refptr<MainThreadTaskQueue> VirtualTimeControlTaskQueue();

  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);

  using VirtualTimePolicy = WebViewScheduler::VirtualTimePolicy;
  using VirtualTimeObserver = WebViewScheduler::VirtualTimeObserver;

  // Tells the scheduler that all TaskQueues should use virtual time.
  void EnableVirtualTime();

  // Migrates all task queues to real time.
  void DisableVirtualTimeForTesting();

  // Returns true if virtual time is not paused.
  bool VirtualTimeAllowedToAdvance() const;
  void SetVirtualTimePolicy(VirtualTimePolicy virtual_time_policy);
  void SetMaxVirtualTimeTaskStarvationCount(int max_task_starvation_count);
  void AddVirtualTimeObserver(VirtualTimeObserver*);
  void RemoveVirtualTimeObserver(VirtualTimeObserver*);
  void IncrementVirtualTimePauseCount();
  void DecrementVirtualTimePauseCount();

  void AddWebViewScheduler(WebViewSchedulerImpl* web_view_scheduler);
  void RemoveWebViewScheduler(WebViewSchedulerImpl* web_view_scheduler);

  void AddTaskTimeObserver(TaskTimeObserver* task_time_observer);
  void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer);

  // Snapshots this RendererScheduler for tracing.
  void CreateTraceEventObjectSnapshot() const;

  // Called when one of associated WebView schedulers has changed audio
  // state.
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

  // Test helpers.
  MainThreadSchedulerHelper* GetSchedulerHelperForTesting();
  TaskCostEstimator* GetLoadingTaskCostEstimatorForTesting();
  TaskCostEstimator* GetTimerTaskCostEstimatorForTesting();
  IdleTimeEstimator* GetIdleTimeEstimatorForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;
  void RunIdleTasksForTesting(const base::Closure& callback);
  void EndIdlePeriodForTesting(const base::Closure& callback,
                               base::TimeTicks time_remaining);
  bool PolicyNeedsUpdateForTesting();
  WakeUpBudgetPool* GetWakeUpBudgetPoolForTesting();

  base::TickClock* tick_clock() const;

  RealTimeDomain* real_time_domain() const {
    return helper_.real_time_domain();
  }

  AutoAdvancingVirtualTimeDomain* GetVirtualTimeDomain();

  TimeDomain* GetActiveTimeDomain();

  TaskQueueThrottler* task_queue_throttler() const {
    return task_queue_throttler_.get();
  }

  void OnFirstMeaningfulPaint();

  void OnShutdownTaskQueue(const scoped_refptr<MainThreadTaskQueue>& queue);

  void OnTaskStarted(MainThreadTaskQueue* queue,
                     const TaskQueue::Task& task,
                     base::TimeTicks start);

  void OnTaskCompleted(MainThreadTaskQueue* queue,
                       const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end);

  // base::trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 protected:
  // RendererScheduler implementation.
  // Use *TaskQueue internally.
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;

 private:
  friend class RenderWidgetSchedulingState;
  friend class RendererMetricsHelper;

  friend class RendererMetricsHelperTest;
  friend class renderer_scheduler_impl_unittest::RendererSchedulerImplForTest;
  friend class renderer_scheduler_impl_unittest::RendererSchedulerImplTest;
  FRIEND_TEST_ALL_PREFIXES(
      renderer_scheduler_impl_unittest::RendererSchedulerImplTest,
      Tracing);

  enum class ExpensiveTaskPolicy { kRun, kBlock, kThrottle };

  enum class TimeDomainType {
    kReal,
    kThrottled,
    kVirtual,
  };

  static const char* TimeDomainTypeToString(TimeDomainType domain_type);

  void SetStoppedInBackground(bool) const;

  struct TaskQueuePolicy {
    // Default constructor of TaskQueuePolicy should match behaviour of a
    // newly-created task queue.
    TaskQueuePolicy()
        : is_enabled(true),
          is_paused(false),
          is_throttled(false),
          is_blocked(false),
          is_stopped(false),
          use_virtual_time(false),
          priority(TaskQueue::kNormalPriority) {}

    bool is_enabled;
    bool is_paused;
    bool is_throttled;
    bool is_blocked;
    bool is_stopped;
    bool use_virtual_time;
    TaskQueue::QueuePriority priority;

    bool IsQueueEnabled(MainThreadTaskQueue* task_queue) const;

    TaskQueue::QueuePriority GetPriority(MainThreadTaskQueue* task_queue) const;

    TimeDomainType GetTimeDomainType(MainThreadTaskQueue* task_queue) const;

    bool operator==(const TaskQueuePolicy& other) const {
      return is_enabled == other.is_enabled && is_paused == other.is_paused &&
             is_throttled == other.is_throttled &&
             is_blocked == other.is_blocked && is_stopped == other.is_stopped &&
             use_virtual_time == other.use_virtual_time &&
             priority == other.priority;
    }

    void AsValueInto(base::trace_event::TracedValue* state) const;
  };

  class Policy {
   public:
    Policy()
        : rail_mode_(v8::PERFORMANCE_ANIMATION),
          should_disable_throttling_(false) {}
    ~Policy() {}

    TaskQueuePolicy& compositor_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kCompositor)];
    }
    const TaskQueuePolicy& compositor_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kCompositor)];
    }

    TaskQueuePolicy& loading_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kLoading)];
    }
    const TaskQueuePolicy& loading_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kLoading)];
    }

    TaskQueuePolicy& timer_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kTimer)];
    }
    const TaskQueuePolicy& timer_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kTimer)];
    }

    TaskQueuePolicy& default_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kNone)];
    }
    const TaskQueuePolicy& default_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kNone)];
    }

    const TaskQueuePolicy& GetQueuePolicy(
        MainThreadTaskQueue::QueueClass queue_class) const {
      return policies_[static_cast<size_t>(queue_class)];
    }

    v8::RAILMode& rail_mode() { return rail_mode_; }
    v8::RAILMode rail_mode() const { return rail_mode_; }

    bool& should_disable_throttling() { return should_disable_throttling_; }
    bool should_disable_throttling() const {
      return should_disable_throttling_;
    }

    bool operator==(const Policy& other) const {
      return policies_ == other.policies_ && rail_mode_ == other.rail_mode_ &&
             should_disable_throttling_ == other.should_disable_throttling_;
    }

    void AsValueInto(base::trace_event::TracedValue* state) const;

   private:
    v8::RAILMode rail_mode_;
    bool should_disable_throttling_;

    std::array<TaskQueuePolicy,
               static_cast<size_t>(MainThreadTaskQueue::QueueClass::kCount)>
        policies_;
  };

  class PollableNeedsUpdateFlag {
   public:
    PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetWhileLocked(bool value);

    // Returns true iff the flag is set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  class TaskDurationMetricTracker;

  class RendererPauseHandleImpl : public RendererPauseHandle {
   public:
    explicit RendererPauseHandleImpl(RendererSchedulerImpl* scheduler);
    ~RendererPauseHandleImpl() override;

   private:
    RendererSchedulerImpl* scheduler_;  // NOT OWNED
  };

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override;
  void OnIdlePeriodEnded() override;

  void OnPendingTasksChanged(bool has_tasks) override;

  void EndIdlePeriod();

  // Returns the serialized scheduler state for tracing.
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      base::TimeTicks optional_now) const;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;
  void CreateTraceEventObjectSnapshotLocked() const;

  static bool ShouldPrioritizeInputEvent(const WebInputEvent& web_input_event);

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time in milliseconds we have to respond to user input as
  // defined by RAILS.
  static const int kRailsResponseTimeMillis = 50;

  // The amount of time to wait before suspending shared timers, and loading
  // etc. after the renderer has been backgrounded. This is used only if
  // background suspension is enabled.
  static const int kDelayForBackgroundTabStoppingMillis = 5 * 60 * 1000;

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

  // The implelemtation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
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

  std::unique_ptr<base::SingleSampleMetric> CreateMaxQueueingTimeMetric();

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(WebInputEvent::Type type,
                                             InputEventState input_event_state);

  // The task cost estimators and the UserModel need to be reset upon page
  // nagigation. This function does that. Must be called from the main thread.
  void ResetForNavigationLocked();

  // Estimates the maximum task length that won't cause a jank based on the
  // current system state. Must be called from the main thread.
  base::TimeDelta EstimateLongestJankFreeTaskDuration() const;

  // Report an intervention to all WebViews in this process.
  void BroadcastIntervention(const std::string& message);

  void ApplyTaskQueuePolicy(
      MainThreadTaskQueue* task_queue,
      TaskQueue::QueueEnabledVoter* task_queue_enabled_voter,
      const TaskQueuePolicy& old_task_queue_policy,
      const TaskQueuePolicy& new_task_queue_policy) const;

  static const char* ExpensiveTaskPolicyToString(
      ExpensiveTaskPolicy expensive_task_policy);

  bool ShouldDisableThrottlingBecauseOfAudio(base::TimeTicks now);

  void AddQueueToWakeUpBudgetPool(MainThreadTaskQueue* queue);

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

  MainThreadSchedulerHelper helper_;
  IdleHelper idle_helper_;
  IdleCanceledDelayedTaskSweeper idle_canceled_delayed_task_sweeper_;
  std::unique_ptr<TaskQueueThrottler> task_queue_throttler_;
  RenderWidgetSignals render_widget_scheduler_signals_;

  const scoped_refptr<MainThreadTaskQueue> control_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  scoped_refptr<MainThreadTaskQueue> virtual_time_control_task_queue_;
  std::unique_ptr<TaskQueue::QueueEnabledVoter>
      compositor_task_queue_enabled_voter_;

  using TaskQueueVoterMap =
      std::map<scoped_refptr<MainThreadTaskQueue>,
               std::unique_ptr<TaskQueue::QueueEnabledVoter>>;

  TaskQueueVoterMap task_runners_;

  scoped_refptr<MainThreadTaskQueue> default_loading_task_queue_;
  scoped_refptr<MainThreadTaskQueue> default_timer_task_queue_;
  scoped_refptr<MainThreadTaskQueue> v8_task_queue_;
  scoped_refptr<MainThreadTaskQueue> ipc_task_queue_;

  // Note |virtual_time_domain_| is lazily created.
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> virtual_time_domain_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;

  using SeqLockQueueingTimeEstimator =
      device::SharedMemorySeqLockBuffer<QueueingTimeEstimator>;

  SeqLockQueueingTimeEstimator seqlock_queueing_time_estimator_;

  base::TimeDelta delay_for_background_tab_stopping_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.

  struct MainThreadOnly {
    MainThreadOnly(
        RendererSchedulerImpl* renderer_scheduler_impl,
        const scoped_refptr<MainThreadTaskQueue>& compositor_task_runner,
        base::TickClock* time_source,
        base::TimeTicks now);
    ~MainThreadOnly();

    TaskCostEstimator loading_task_cost_estimator;
    TaskCostEstimator timer_task_cost_estimator;
    IdleTimeEstimator idle_time_estimator;
    TraceableState<UseCase, kTracingCategoryNameDefault> current_use_case;
    Policy current_policy;
    base::TimeTicks current_policy_expiration_time;
    base::TimeTicks estimated_next_frame_begin;
    base::TimeTicks current_task_start_time;
    base::TimeDelta most_recent_expected_queueing_time;
    base::TimeDelta compositor_frame_interval;
    base::TimeDelta longest_jank_free_task_duration;
    base::Optional<base::TimeTicks> last_audio_state_change;
    int renderer_pause_count;  // Renderer is paused if non-zero.
    int navigation_task_expected_count;
    TraceableState<ExpensiveTaskPolicy, kTracingCategoryNameInfo>
        expensive_task_policy;
    TraceableState<v8::RAILMode, kTracingCategoryNameInfo>
        rail_mode_for_tracing;  // Don't use except for tracing.
    bool renderer_hidden;
    TraceableState<bool, kTracingCategoryNameDefault> renderer_backgrounded;
    bool stopping_when_backgrounded_enabled;
    bool stopped_when_backgrounded;
    bool was_shutdown;
    TraceableState<bool, kTracingCategoryNameInfo> loading_tasks_seem_expensive;
    TraceableState<bool, kTracingCategoryNameInfo> timer_tasks_seem_expensive;
    TraceableState<bool, kTracingCategoryNameDefault> touchstart_expected_soon;
    bool have_seen_a_begin_main_frame;
    bool have_reported_blocking_intervention_in_current_policy;
    bool have_reported_blocking_intervention_since_navigation;
    bool has_visible_render_widget_with_touch_handler;
    bool begin_frame_not_expected_soon;
    bool in_idle_period_for_testing;
    bool use_virtual_time;
    TraceableState<bool, kTracingCategoryNameDefault> is_audio_playing;
    bool compositor_will_send_main_frame_not_expected;
    bool has_navigated;
    bool pause_timers_for_webview;
    std::unique_ptr<base::SingleSampleMetric> max_queueing_time_metric;
    base::TimeDelta max_queueing_time;
    base::TimeTicks background_status_changed_at;
    std::set<WebViewSchedulerImpl*> web_view_schedulers;  // Not owned.
    RAILModeObserver* rail_mode_observer;                 // Not owned.
    WakeUpBudgetPool* wake_up_budget_pool;                // Not owned.
    RendererMetricsHelper metrics_helper;
    RendererProcessType process_type;

    base::ObserverList<VirtualTimeObserver> virtual_time_observers;
    base::TimeTicks initial_virtual_time;
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
    bool nested_runloop;
  };

  struct AnyThread {
    AnyThread();
    ~AnyThread();

    base::TimeTicks last_idle_period_end_time;
    base::TimeTicks fling_compositor_escalation_deadline;
    UserModel user_model;
    bool awaiting_touch_start_response;
    bool in_idle_period;
    bool begin_main_frame_on_critical_path;
    bool last_gesture_was_compositor_driven;
    bool default_gesture_prevented;
    bool have_seen_a_potentially_blocking_gesture;
    bool waiting_for_meaningful_paint;
    bool have_seen_input_since_navigation;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    WebInputEvent::Type last_input_type;
    bool main_thread_seems_unresponsive;
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

  // Don't access main_thread_only_, instead use MainThreadOnly().
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
  // Don't access any_thread_, instead use AnyThread().
  AnyThread any_thread_;
  AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  // Don't access compositor_thread_only_, instead use CompositorThreadOnly().
  CompositorThreadOnly compositor_thread_only_;
  CompositorThreadOnly& GetCompositorThreadOnly() {
    compositor_thread_only_.CheckOnValidThread();
    return compositor_thread_only_;
  }

  PollableThreadSafeFlag policy_may_need_update_;

  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
