// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/time_converter.h"
#include "platform/scheduler/base/trace_helper.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/task_queue_throttler.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"
#include "public/platform/Platform.h"

namespace blink {
namespace scheduler {
namespace {
// The run time of loading tasks is strongly bimodal.  The vast majority are
// very cheap, but there are usually a handful of very expensive tasks (e.g ~1
// second on a mobile device) so we take a very pessimistic view when estimating
// the cost of loading tasks.
const int kLoadingTaskEstimationSampleCount = 1000;
const double kLoadingTaskEstimationPercentile = 99;
const int kTimerTaskEstimationSampleCount = 1000;
const double kTimerTaskEstimationPercentile = 99;
const int kShortIdlePeriodDurationSampleCount = 10;
const double kShortIdlePeriodDurationPercentile = 50;
// Amount of idle time left in a frame (as a ratio of the vsync interval) above
// which main thread compositing can be considered fast.
const double kFastCompositingIdleTimeThreshold = .2;
constexpr base::TimeDelta kThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromMinutes(1);
// We do not throttle anything while audio is played and shortly after that.
constexpr base::TimeDelta kThrottlingDelayAfterAudioIsPlayed =
    base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kQueueingTimeWindowDuration =
    base::TimeDelta::FromSeconds(1);
// Threshold for discarding ultra-long tasks. It is assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep in the middle
// of the task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kLongIdlePeriodDiscardingThreshold =
    base::TimeDelta::FromMinutes(3);

}  // namespace

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner)
    : helper_(main_task_runner, this),
      idle_helper_(
          &helper_,
          this,
          "RendererSchedulerIdlePeriod",
          base::TimeDelta(),
          helper_.NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::IDLE))),
      idle_canceled_delayed_task_sweeper_(&helper_,
                                          idle_helper_.IdleTaskRunner()),
      render_widget_scheduler_signals_(this),
      control_task_queue_(helper_.ControlMainThreadTaskQueue()),
      compositor_task_queue_(
          helper_.NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                                   MainThreadTaskQueue::QueueType::COMPOSITOR)
                                   .SetShouldMonitorQuiescence(true))),
      compositor_task_queue_enabled_voter_(
          compositor_task_queue_->CreateQueueEnabledVoter()),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlMainThreadTaskQueue()),
      seqlock_queueing_time_estimator_(
          QueueingTimeEstimator(this, kQueueingTimeWindowDuration, 20)),
      main_thread_only_(this,
                        compositor_task_queue_,
                        helper_.scheduler_tqm_delegate().get(),
                        helper_.scheduler_tqm_delegate()->NowTicks()),
      policy_may_need_update_(&any_thread_lock_),
      weak_factory_(this) {
  task_queue_throttler_.reset(new TaskQueueThrottler(this));
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  // Compositor task queue and default task queue should be managed by
  // RendererScheduler. Control task queue should not.
  task_runners_.insert(
      std::make_pair(helper_.DefaultMainThreadTaskQueue(), nullptr));
  task_runners_.insert(
      std::make_pair(compositor_task_queue_,
                     compositor_task_queue_->CreateQueueEnabledVoter()));

  default_loading_task_queue_ =
      NewLoadingTaskQueue(MainThreadTaskQueue::QueueType::DEFAULT_LOADING);
  default_timer_task_queue_ =
      NewTimerTaskQueue(MainThreadTaskQueue::QueueType::DEFAULT_TIMER);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  helper_.SetObserver(this);
  helper_.AddTaskTimeObserver(this);

  // Register a tracing state observer unless we're running in a test without a
  // task runner. Note that it's safe to remove a non-existent observer.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
        weak_factory_.GetWeakPtr());
  }
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  for (auto& pair : task_runners_) {
    TaskCostEstimator* observer = nullptr;
    switch (pair.first->queue_class()) {
      case MainThreadTaskQueue::QueueClass::LOADING:
        observer = &GetMainThreadOnly().loading_task_cost_estimator;
        break;
      case MainThreadTaskQueue::QueueClass::TIMER:
        observer = &GetMainThreadOnly().timer_task_cost_estimator;
        break;
      default:
        observer = nullptr;
    }

    if (observer)
      pair.first->RemoveTaskObserver(observer);
  }

  if (virtual_time_domain_)
    UnregisterTimeDomain(virtual_time_domain_.get());

  helper_.RemoveTaskTimeObserver(this);

  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      this);

  // Ensure the renderer scheduler was shut down explicitly, because otherwise
  // we could end up having stale pointers to the Blink heap which has been
  // terminated by this point.
  DCHECK(GetMainThreadOnly().was_shutdown);
}

#define TASK_DURATION_METRIC_NAME "RendererScheduler.TaskDurationPerQueueType2"

RendererSchedulerImpl::MainThreadOnly::MainThreadOnly(
    RendererSchedulerImpl* renderer_scheduler_impl,
    const scoped_refptr<MainThreadTaskQueue>& compositor_task_runner,
    base::TickClock* time_source,
    base::TimeTicks now)
    : loading_task_cost_estimator(time_source,
                                  kLoadingTaskEstimationSampleCount,
                                  kLoadingTaskEstimationPercentile),
      timer_task_cost_estimator(time_source,
                                kTimerTaskEstimationSampleCount,
                                kTimerTaskEstimationPercentile),
      idle_time_estimator(compositor_task_runner,
                          time_source,
                          kShortIdlePeriodDurationSampleCount,
                          kShortIdlePeriodDurationPercentile),
      main_thread_load_tracker(
          now,
          base::Bind(&RendererSchedulerImpl::RecordMainThreadTaskLoad,
                     base::Unretained(renderer_scheduler_impl)),
          kThreadLoadTrackerReportingInterval),
      background_main_thread_load_tracker(
          now,
          base::Bind(&RendererSchedulerImpl::RecordBackgroundMainThreadTaskLoad,
                     base::Unretained(renderer_scheduler_impl)),
          kThreadLoadTrackerReportingInterval),
      foreground_main_thread_load_tracker(
          now,
          base::Bind(&RendererSchedulerImpl::RecordForegroundMainThreadTaskLoad,
                     base::Unretained(renderer_scheduler_impl)),
          kThreadLoadTrackerReportingInterval),
      current_use_case(UseCase::NONE),
      timer_queue_suspend_count(0),
      navigation_task_expected_count(0),
      expensive_task_policy(ExpensiveTaskPolicy::RUN),
      renderer_hidden(false),
      renderer_backgrounded(false),
      renderer_suspended(false),
      timer_queue_suspension_when_backgrounded_enabled(false),
      timer_queue_suspended_when_backgrounded(false),
      was_shutdown(false),
      loading_tasks_seem_expensive(false),
      timer_tasks_seem_expensive(false),
      touchstart_expected_soon(false),
      have_seen_a_begin_main_frame(false),
      have_reported_blocking_intervention_in_current_policy(false),
      have_reported_blocking_intervention_since_navigation(false),
      has_visible_render_widget_with_touch_handler(false),
      begin_frame_not_expected_soon(false),
      in_idle_period_for_testing(false),
      use_virtual_time(false),
      is_audio_playing(false),
      compositor_will_send_main_frame_not_expected(false),
      virtual_time_paused(false),
      has_navigated(false),
      background_status_changed_at(now),
      rail_mode_observer(nullptr),
      wake_up_budget_pool(nullptr),
      task_duration_reporter(TASK_DURATION_METRIC_NAME),
      foreground_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                        ".Foreground"),
      foreground_first_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Foreground.FirstMinute"),
      foreground_second_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Foreground.SecondMinute"),
      foreground_third_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Foreground.ThirdMinute"),
      foreground_after_third_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Foreground.AfterThirdMinute"),
      background_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                        ".Background"),
      background_first_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.FirstMinute"),
      background_second_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.SecondMinute"),
      background_third_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.ThirdMinute"),
      background_fourth_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.FourthMinute"),
      background_fifth_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.FifthMinute"),
      background_after_fifth_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.AfterFifthMinute"),
      hidden_task_duration_reporter(TASK_DURATION_METRIC_NAME ".Hidden"),
      visible_task_duration_reporter(TASK_DURATION_METRIC_NAME ".Visible"),
      hidden_music_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                          ".HiddenMusic") {
  main_thread_load_tracker.Resume(now);
  foreground_main_thread_load_tracker.Resume(now);
}

RendererSchedulerImpl::MainThreadOnly::~MainThreadOnly() {}

RendererSchedulerImpl::AnyThread::AnyThread()
    : awaiting_touch_start_response(false),
      in_idle_period(false),
      begin_main_frame_on_critical_path(false),
      last_gesture_was_compositor_driven(false),
      default_gesture_prevented(true),
      have_seen_a_potentially_blocking_gesture(false),
      waiting_for_meaningful_paint(false),
      have_seen_input_since_navigation(false) {}

RendererSchedulerImpl::AnyThread::~AnyThread() {}

RendererSchedulerImpl::CompositorThreadOnly::CompositorThreadOnly()
    : last_input_type(blink::WebInputEvent::kUndefined),
      main_thread_seems_unresponsive(false) {}

RendererSchedulerImpl::CompositorThreadOnly::~CompositorThreadOnly() {}

void RendererSchedulerImpl::Shutdown() {
  base::TimeTicks now = tick_clock()->NowTicks();
  GetMainThreadOnly().background_main_thread_load_tracker.RecordIdle(now);
  GetMainThreadOnly().foreground_main_thread_load_tracker.RecordIdle(now);

  task_queue_throttler_.reset();
  helper_.Shutdown();
  idle_helper_.Shutdown();
  GetMainThreadOnly().was_shutdown = true;
  GetMainThreadOnly().rail_mode_observer = nullptr;
}

std::unique_ptr<blink::WebThread> RendererSchedulerImpl::CreateMainThread() {
  return base::MakeUnique<WebThreadImplForRendererScheduler>(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultMainThreadTaskQueue();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_queue_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return default_loading_task_queue_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::TimerTaskRunner() {
  helper_.CheckOnValidThread();
  return default_timer_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::DefaultTaskQueue() {
  return helper_.DefaultMainThreadTaskQueue();
}

scoped_refptr<MainThreadTaskQueue>
RendererSchedulerImpl::CompositorTaskQueue() {
  helper_.CheckOnValidThread();
  return compositor_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::LoadingTaskQueue() {
  helper_.CheckOnValidThread();
  return default_loading_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::TimerTaskQueue() {
  helper_.CheckOnValidThread();
  return default_timer_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::ControlTaskQueue() {
  helper_.CheckOnValidThread();
  return helper_.ControlMainThreadTaskQueue();
}

scoped_refptr<MainThreadTaskQueue>
RendererSchedulerImpl::VirtualTimeControlTaskQueue() {
  helper_.CheckOnValidThread();
  return virtual_time_control_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::NewTaskQueue(
    const MainThreadTaskQueue::QueueCreationParams& params) {
  helper_.CheckOnValidThread();
  scoped_refptr<MainThreadTaskQueue> task_queue(helper_.NewTaskQueue(params));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter;
  if (params.can_be_blocked || params.can_be_suspended)
    voter = task_queue->CreateQueueEnabledVoter();

  auto insert_result =
      task_runners_.insert(std::make_pair(task_queue, std::move(voter)));
  auto queue_class = task_queue->queue_class();
  if (queue_class == MainThreadTaskQueue::QueueClass::TIMER) {
    task_queue->AddTaskObserver(&GetMainThreadOnly().timer_task_cost_estimator);
  } else if (queue_class == MainThreadTaskQueue::QueueClass::LOADING) {
    task_queue->AddTaskObserver(
        &GetMainThreadOnly().loading_task_cost_estimator);
  }

  ApplyTaskQueuePolicy(
      task_queue.get(), insert_result.first->second.get(), TaskQueuePolicy(),
      GetMainThreadOnly().current_policy.GetQueuePolicy(queue_class));

  if (task_queue->CanBeThrottled())
    AddQueueToWakeUpBudgetPool(task_queue.get());

  return task_queue;
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::NewLoadingTaskQueue(
    MainThreadTaskQueue::QueueType queue_type) {
  DCHECK_EQ(MainThreadTaskQueue::QueueClassForQueueType(queue_type),
            MainThreadTaskQueue::QueueClass::LOADING);
  return NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(queue_type)
                          .SetCanBeSuspended(true)
                          .SetCanBeBlocked(true));
}

scoped_refptr<MainThreadTaskQueue> RendererSchedulerImpl::NewTimerTaskQueue(
    MainThreadTaskQueue::QueueType queue_type) {
  DCHECK_EQ(MainThreadTaskQueue::QueueClassForQueueType(queue_type),
            MainThreadTaskQueue::QueueClass::TIMER);
  auto timer_task_queue =
      NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(queue_type)
                       .SetShouldReportWhenExecutionBlocked(true)
                       .SetCanBeSuspended(true)
                       .SetCanBeBlocked(true)
                       .SetCanBeThrottled(true));
  if (GetMainThreadOnly().virtual_time_paused)
    timer_task_queue->InsertFence(TaskQueue::InsertFencePosition::NOW);
  return timer_task_queue;
}

std::unique_ptr<RenderWidgetSchedulingState>
RendererSchedulerImpl::NewRenderWidgetSchedulingState() {
  return render_widget_scheduler_signals_.NewRenderWidgetSchedulingState();
}

void RendererSchedulerImpl::OnUnregisterTaskQueue(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  if (task_queue_throttler_)
    task_queue_throttler_->UnregisterTaskQueue(task_queue.get());

  if (task_runners_.erase(task_queue)) {
    switch (task_queue->queue_class()) {
      case MainThreadTaskQueue::QueueClass::TIMER:
        task_queue->RemoveTaskObserver(
            &GetMainThreadOnly().timer_task_cost_estimator);
      case MainThreadTaskQueue::QueueClass::LOADING:
        task_queue->RemoveTaskObserver(
            &GetMainThreadOnly().loading_task_cost_estimator);
      default:
        break;
    }
  }
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

void RendererSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void RendererSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  EndIdlePeriod();
  GetMainThreadOnly().estimated_next_frame_begin =
      args.frame_time + args.interval;
  GetMainThreadOnly().have_seen_a_begin_main_frame = true;
  GetMainThreadOnly().begin_frame_not_expected_soon = false;
  GetMainThreadOnly().compositor_frame_interval = args.interval;
  {
    base::AutoLock lock(any_thread_lock_);
    GetAnyThread().begin_main_frame_on_critical_path = args.on_critical_path;
  }
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.scheduler_tqm_delegate()->NowTicks());
  if (now < GetMainThreadOnly().estimated_next_frame_begin) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now,
        GetMainThreadOnly().estimated_next_frame_begin);
  }

  GetMainThreadOnly().idle_time_estimator.DidCommitFrameToCompositor();
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  GetMainThreadOnly().begin_frame_not_expected_soon = true;
  idle_helper_.EnableLongIdlePeriod();
  {
    base::AutoLock lock(any_thread_lock_);
    GetAnyThread().begin_main_frame_on_critical_path = false;
  }
}

void RendererSchedulerImpl::BeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.scheduler_tqm_delegate()->NowTicks());
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginMainFrameNotExpectedUntil",
               "time_remaining", (time - now).InMillisecondsF());

  if (now < time) {
    // End any previous idle period.
    EndIdlePeriod();

    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now, time);
  }
}

void RendererSchedulerImpl::SetAllRenderWidgetsHidden(bool hidden) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::SetAllRenderWidgetsHidden", "hidden",
               hidden);

  helper_.CheckOnValidThread();

  if (helper_.IsShutdown() || GetMainThreadOnly().renderer_hidden == hidden)
    return;

  end_renderer_hidden_idle_period_closure_.Cancel();

  if (hidden) {
    idle_helper_.EnableLongIdlePeriod();

    // Ensure that we stop running idle tasks after a few seconds of being
    // hidden.
    base::TimeDelta end_idle_when_hidden_delay =
        base::TimeDelta::FromMilliseconds(kEndIdleWhenHiddenDelayMillis);
    control_task_queue_->PostDelayedTask(
        FROM_HERE, end_renderer_hidden_idle_period_closure_.GetCallback(),
        end_idle_when_hidden_delay);
    GetMainThreadOnly().renderer_hidden = true;
  } else {
    GetMainThreadOnly().renderer_hidden = false;
    EndIdlePeriod();
  }

  // TODO(alexclarke): Should we update policy here?
  CreateTraceEventObjectSnapshot();
}

void RendererSchedulerImpl::SetHasVisibleRenderWidgetWithTouchHandler(
    bool has_visible_render_widget_with_touch_handler) {
  helper_.CheckOnValidThread();
  if (has_visible_render_widget_with_touch_handler ==
      GetMainThreadOnly().has_visible_render_widget_with_touch_handler)
    return;

  GetMainThreadOnly().has_visible_render_widget_with_touch_handler =
      has_visible_render_widget_with_touch_handler;

  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::SetRendererHidden(bool hidden) {
  if (hidden) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererSchedulerImpl::OnRendererHidden");
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererSchedulerImpl::OnRendererVisible");
  }
  helper_.CheckOnValidThread();
  GetMainThreadOnly().renderer_hidden = hidden;
}

void RendererSchedulerImpl::SetRendererBackgrounded(bool backgrounded) {
  if (backgrounded) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererSchedulerImpl::OnRendererBackgrounded");
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererSchedulerImpl::OnRendererForegrounded");
  }
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() ||
      GetMainThreadOnly().renderer_backgrounded == backgrounded)
    return;

  GetMainThreadOnly().renderer_backgrounded = backgrounded;
  if (!backgrounded)
    GetMainThreadOnly().renderer_suspended = false;

  GetMainThreadOnly().background_status_changed_at = tick_clock()->NowTicks();

  UpdatePolicy();

  base::TimeTicks now = tick_clock()->NowTicks();
  if (backgrounded) {
    GetMainThreadOnly().foreground_main_thread_load_tracker.Pause(now);
    GetMainThreadOnly().background_main_thread_load_tracker.Resume(now);
  } else {
    GetMainThreadOnly().foreground_main_thread_load_tracker.Resume(now);
    GetMainThreadOnly().background_main_thread_load_tracker.Pause(now);
  }
}

void RendererSchedulerImpl::OnAudioStateChanged() {
  bool is_audio_playing = false;
  for (WebViewSchedulerImpl* web_view_scheduler :
       GetMainThreadOnly().web_view_schedulers) {
    is_audio_playing = is_audio_playing || web_view_scheduler->IsAudioPlaying();
  }

  if (is_audio_playing == GetMainThreadOnly().is_audio_playing)
    return;

  GetMainThreadOnly().last_audio_state_change =
      helper_.scheduler_tqm_delegate()->NowTicks();
  GetMainThreadOnly().is_audio_playing = is_audio_playing;

  UpdatePolicy();
}

void RendererSchedulerImpl::SuspendRenderer() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;
  if (!GetMainThreadOnly().renderer_backgrounded)
    return;

  UMA_HISTOGRAM_COUNTS("PurgeAndSuspend.PendingTaskCount",
                       helper_.GetNumberOfPendingTasks());

  // TODO(hajimehoshi): We might need to suspend not only timer queue but also
  // e.g. loading tasks or postMessage.
  GetMainThreadOnly().renderer_suspended = true;
  UpdatePolicy();
}

void RendererSchedulerImpl::ResumeRenderer() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;
  if (!GetMainThreadOnly().renderer_backgrounded)
    return;
  GetMainThreadOnly().renderer_suspended = false;
  UpdatePolicy();
}

void RendererSchedulerImpl::EndIdlePeriod() {
  if (GetMainThreadOnly().in_idle_period_for_testing)
    return;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  idle_helper_.EndIdlePeriod();
}

void RendererSchedulerImpl::EndIdlePeriodForTesting(
    const base::Closure& callback,
    base::TimeTicks time_remaining) {
  GetMainThreadOnly().in_idle_period_for_testing = false;
  EndIdlePeriod();
  callback.Run();
}

bool RendererSchedulerImpl::PolicyNeedsUpdateForTesting() {
  return policy_may_need_update_.IsSet();
}

// static
bool RendererSchedulerImpl::ShouldPrioritizeInputEvent(
    const blink::WebInputEvent& web_input_event) {
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if ((web_input_event.GetType() == blink::WebInputEvent::kMouseDown ||
       web_input_event.GetType() == blink::WebInputEvent::kMouseMove) &&
      (web_input_event.GetModifiers() &
       blink::WebInputEvent::kLeftButtonDown)) {
    return true;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::IsMouseEventType(web_input_event.GetType()) ||
      blink::WebInputEvent::IsKeyboardEventType(web_input_event.GetType())) {
    return false;
  }
  return true;
}

void RendererSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnCompositorThread");
  if (!ShouldPrioritizeInputEvent(web_input_event))
    return;

  UpdateForInputEventOnCompositorThread(web_input_event.GetType(), event_state);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidAnimateForInputOnCompositorThread");
  base::AutoLock lock(any_thread_lock_);
  GetAnyThread().fling_compositor_escalation_deadline =
      helper_.scheduler_tqm_delegate()->NowTicks() +
      base::TimeDelta::FromMilliseconds(kFlingEscalationLimitMillis);
}

void RendererSchedulerImpl::UpdateForInputEventOnCompositorThread(
    blink::WebInputEvent::Type type,
    InputEventState input_event_state) {
  base::AutoLock lock(any_thread_lock_);
  base::TimeTicks now = helper_.scheduler_tqm_delegate()->NowTicks();

  // TODO(alexclarke): Move WebInputEventTraits where we can access it from here
  // and record the name rather than the integer representation.
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::UpdateForInputEventOnCompositorThread",
               "type", static_cast<int>(type), "input_event_state",
               InputEventStateToString(input_event_state));

  base::TimeDelta unused_policy_duration;
  UseCase previous_use_case =
      ComputeCurrentUseCase(now, &unused_policy_duration);
  bool was_awaiting_touch_start_response =
      GetAnyThread().awaiting_touch_start_response;

  GetAnyThread().user_model.DidStartProcessingInputEvent(type, now);
  GetAnyThread().have_seen_input_since_navigation = true;

  if (input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR)
    GetAnyThread().user_model.DidFinishProcessingInputEvent(now);

  switch (type) {
    case blink::WebInputEvent::kTouchStart:
      GetAnyThread().awaiting_touch_start_response = true;
      // This is just a fail-safe to reset the state of
      // |last_gesture_was_compositor_driven| to the default. We don't know
      // yet where the gesture will run.
      GetAnyThread().last_gesture_was_compositor_driven = false;
      GetAnyThread().have_seen_a_potentially_blocking_gesture = true;
      // Assume the default gesture is prevented until we see evidence
      // otherwise.
      GetAnyThread().default_gesture_prevented = true;
      break;

    case blink::WebInputEvent::kTouchMove:
      // Observation of consecutive touchmoves is a strong signal that the
      // page is consuming the touch sequence, in which case touchstart
      // response prioritization is no longer necessary. Otherwise, the
      // initial touchmove should preserve the touchstart response pending
      // state.
      if (GetAnyThread().awaiting_touch_start_response &&
          GetCompositorThreadOnly().last_input_type ==
              blink::WebInputEvent::kTouchMove) {
        GetAnyThread().awaiting_touch_start_response = false;
      }
      break;

    case blink::WebInputEvent::kGesturePinchUpdate:
    case blink::WebInputEvent::kGestureScrollUpdate:
      // If we see events for an established gesture, we can lock it to the
      // appropriate thread as the gesture can no longer be cancelled.
      GetAnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      GetAnyThread().awaiting_touch_start_response = false;
      GetAnyThread().default_gesture_prevented = false;
      break;

    case blink::WebInputEvent::kGestureFlingCancel:
      GetAnyThread().fling_compositor_escalation_deadline = base::TimeTicks();
      break;

    case blink::WebInputEvent::kGestureTapDown:
    case blink::WebInputEvent::kGestureShowPress:
    case blink::WebInputEvent::kGestureScrollEnd:
      // With no observable effect, these meta events do not indicate a
      // meaningful touchstart response and should not impact task priority.
      break;

    case blink::WebInputEvent::kMouseDown:
      // Reset tracking state at the start of a new mouse drag gesture.
      GetAnyThread().last_gesture_was_compositor_driven = false;
      GetAnyThread().default_gesture_prevented = true;
      break;

    case blink::WebInputEvent::kMouseMove:
      // Consider mouse movement with the left button held down (see
      // ShouldPrioritizeInputEvent) similarly to a touch gesture.
      GetAnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      GetAnyThread().awaiting_touch_start_response = false;
      break;

    case blink::WebInputEvent::kMouseWheel:
      GetAnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      GetAnyThread().awaiting_touch_start_response = false;
      GetAnyThread().have_seen_a_potentially_blocking_gesture = true;
      // If the event was sent to the main thread, assume the default gesture is
      // prevented until we see evidence otherwise.
      GetAnyThread().default_gesture_prevented =
          !GetAnyThread().last_gesture_was_compositor_driven;
      break;

    case blink::WebInputEvent::kUndefined:
      break;

    default:
      GetAnyThread().awaiting_touch_start_response = false;
      break;
  }

  // Avoid unnecessary policy updates if the use case did not change.
  UseCase use_case = ComputeCurrentUseCase(now, &unused_policy_duration);

  if (use_case != previous_use_case ||
      was_awaiting_touch_start_response !=
          GetAnyThread().awaiting_touch_start_response) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  GetCompositorThreadOnly().last_input_type = type;
}

void RendererSchedulerImpl::DidHandleInputEventOnMainThread(
    const WebInputEvent& web_input_event,
    WebInputEventResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(any_thread_lock_);
    GetAnyThread().user_model.DidFinishProcessingInputEvent(
        helper_.scheduler_tqm_delegate()->NowTicks());

    // If we were waiting for a touchstart response and the main thread has
    // prevented the default gesture, consider the gesture established. This
    // ensures single-event gestures such as button presses are promptly
    // detected.
    if (GetAnyThread().awaiting_touch_start_response &&
        result == WebInputEventResult::kHandledApplication) {
      GetAnyThread().awaiting_touch_start_response = false;
      GetAnyThread().default_gesture_prevented = true;
      UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
    }
  }
}

base::TimeDelta RendererSchedulerImpl::MostRecentExpectedQueueingTime() {
  return GetMainThreadOnly().most_recent_expected_queueing_time;
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart, synchronized gesture and main-thread gesture use cases
  // indicate a strong likelihood of high-priority work in the near future.
  UseCase use_case = GetMainThreadOnly().current_use_case;
  return GetMainThreadOnly().touchstart_expected_soon ||
         use_case == UseCase::TOUCHSTART ||
         use_case == UseCase::MAIN_THREAD_GESTURE ||
         use_case == UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING ||
         use_case == UseCase::SYNCHRONIZED_GESTURE;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if there's a urgent task to be run now, or we are expecting
  // one soon (touch start).
  // Note: even though the control queue has the highest priority we don't yield
  // for it since these tasks are not user-provided work and they are only
  // intended to run before the next task, not interrupt the tasks.
  switch (GetMainThreadOnly().current_use_case) {
    case UseCase::COMPOSITOR_GESTURE:
    case UseCase::NONE:
      return GetMainThreadOnly().touchstart_expected_soon;

    case UseCase::MAIN_THREAD_GESTURE:
    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
    case UseCase::SYNCHRONIZED_GESTURE:
      return compositor_task_queue_->HasTaskToRunImmediately() ||
             GetMainThreadOnly().touchstart_expected_soon;

    case UseCase::TOUCHSTART:
      return true;

    case UseCase::LOADING:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void RendererSchedulerImpl::RunIdleTasksForTesting(
    const base::Closure& callback) {
  GetMainThreadOnly().in_idle_period_for_testing = true;
  IdleTaskRunner()->PostIdleTask(
      FROM_HERE, base::Bind(&RendererSchedulerImpl::EndIdlePeriodForTesting,
                            weak_factory_.GetWeakPtr(), callback));
  idle_helper_.EnableLongIdlePeriod();
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const tracked_objects::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  any_thread_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_queue_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

namespace {

void UpdatePolicyDuration(base::TimeTicks now,
                          base::TimeTicks policy_expiration,
                          base::TimeDelta* policy_duration) {
  if (policy_expiration <= now)
    return;

  if (policy_duration->is_zero()) {
    *policy_duration = policy_expiration - now;
    return;
  }

  *policy_duration = std::min(*policy_duration, policy_expiration - now);
}

}  // namespace

void RendererSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.scheduler_tqm_delegate()->NowTicks();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta expected_use_case_duration;
  UseCase use_case = ComputeCurrentUseCase(now, &expected_use_case_duration);
  GetMainThreadOnly().current_use_case = use_case;

  base::TimeDelta touchstart_expected_flag_valid_for_duration;
  // TODO(skyostil): Consider handlers for all types of blocking gestures (e.g.,
  // mouse wheel) instead of just touchstart.
  bool touchstart_expected_soon = false;
  if (GetMainThreadOnly().has_visible_render_widget_with_touch_handler) {
    touchstart_expected_soon = GetAnyThread().user_model.IsGestureExpectedSoon(
        now, &touchstart_expected_flag_valid_for_duration);
  }
  GetMainThreadOnly().touchstart_expected_soon = touchstart_expected_soon;

  base::TimeDelta longest_jank_free_task_duration =
      EstimateLongestJankFreeTaskDuration();
  GetMainThreadOnly().longest_jank_free_task_duration =
      longest_jank_free_task_duration;

  bool loading_tasks_seem_expensive = false;
  bool timer_tasks_seem_expensive = false;
  loading_tasks_seem_expensive =
      GetMainThreadOnly().loading_task_cost_estimator.expected_task_duration() >
      longest_jank_free_task_duration;
  timer_tasks_seem_expensive =
      GetMainThreadOnly().timer_task_cost_estimator.expected_task_duration() >
      longest_jank_free_task_duration;
  GetMainThreadOnly().timer_tasks_seem_expensive = timer_tasks_seem_expensive;
  GetMainThreadOnly().loading_tasks_seem_expensive =
      loading_tasks_seem_expensive;

  // The |new_policy_duration| is the minimum of |expected_use_case_duration|
  // and |touchstart_expected_flag_valid_for_duration| unless one is zero in
  // which case we choose the other.
  base::TimeDelta new_policy_duration = expected_use_case_duration;
  if (new_policy_duration.is_zero() ||
      (touchstart_expected_flag_valid_for_duration > base::TimeDelta() &&
       new_policy_duration > touchstart_expected_flag_valid_for_duration)) {
    new_policy_duration = touchstart_expected_flag_valid_for_duration;
  }

  // Do not throttle while audio is playing or for a short period after that
  // to make sure that pages playing short audio clips powered by timers
  // work.
  if (GetMainThreadOnly().last_audio_state_change &&
      !GetMainThreadOnly().is_audio_playing) {
    UpdatePolicyDuration(now,
                         GetMainThreadOnly().last_audio_state_change.value() +
                             kThrottlingDelayAfterAudioIsPlayed,
                         &new_policy_duration);
  }

  bool timer_queue_newly_suspended = false;
  if (GetMainThreadOnly().renderer_backgrounded &&
      GetMainThreadOnly().timer_queue_suspension_when_backgrounded_enabled) {
    base::TimeTicks suspend_timers_at =
        GetMainThreadOnly().background_status_changed_at +
        base::TimeDelta::FromMilliseconds(
            kSuspendTimersWhenBackgroundedDelayMillis);

    timer_queue_newly_suspended =
        !GetMainThreadOnly().timer_queue_suspended_when_backgrounded;
    GetMainThreadOnly().timer_queue_suspended_when_backgrounded =
        now >= suspend_timers_at || GetMainThreadOnly().renderer_suspended;
    timer_queue_newly_suspended &=
        GetMainThreadOnly().timer_queue_suspended_when_backgrounded;

    if (!GetMainThreadOnly().timer_queue_suspended_when_backgrounded)
      UpdatePolicyDuration(now, suspend_timers_at, &new_policy_duration);
  } else {
    GetMainThreadOnly().timer_queue_suspended_when_backgrounded = false;
  }

  if (new_policy_duration > base::TimeDelta()) {
    GetMainThreadOnly().current_policy_expiration_time =
        now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    GetMainThreadOnly().current_policy_expiration_time = base::TimeTicks();
  }

  // Avoid prioritizing main thread compositing (e.g., rAF) if it is extremely
  // slow, because that can cause starvation in other task sources.
  bool main_thread_compositing_is_fast =
      GetMainThreadOnly().idle_time_estimator.GetExpectedIdleDuration(
          GetMainThreadOnly().compositor_frame_interval) >
      GetMainThreadOnly().compositor_frame_interval *
          kFastCompositingIdleTimeThreshold;

  Policy new_policy;
  ExpensiveTaskPolicy expensive_task_policy = ExpensiveTaskPolicy::RUN;
  new_policy.rail_mode() = v8::PERFORMANCE_ANIMATION;

  switch (use_case) {
    case UseCase::COMPOSITOR_GESTURE:
      if (touchstart_expected_soon) {
        new_policy.rail_mode() = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
        new_policy.compositor_queue_policy().priority =
            TaskQueue::HIGH_PRIORITY;
      } else {
        // What we really want to do is priorize loading tasks, but that doesn't
        // seem to be safe. Instead we do that by proxy by deprioritizing
        // compositor tasks. This should be safe since we've already gone to the
        // pain of fixing ordering issues with them.
        new_policy.compositor_queue_policy().priority = TaskQueue::LOW_PRIORITY;
      }
      break;

    case UseCase::SYNCHRONIZED_GESTURE:
      new_policy.compositor_queue_policy().priority =
          main_thread_compositing_is_fast ? TaskQueue::HIGH_PRIORITY
                                          : TaskQueue::NORMAL_PRIORITY;
      if (touchstart_expected_soon) {
        new_policy.rail_mode() = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      } else {
        expensive_task_policy = ExpensiveTaskPolicy::THROTTLE;
      }
      break;

    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
      // In main thread input handling scenarios we don't have perfect knowledge
      // about which things we should be prioritizing, so we don't attempt to
      // block expensive tasks because we don't know whether they were integral
      // to the page's functionality or not.
      new_policy.compositor_queue_policy().priority =
          main_thread_compositing_is_fast ? TaskQueue::HIGH_PRIORITY
                                          : TaskQueue::NORMAL_PRIORITY;
      break;

    case UseCase::MAIN_THREAD_GESTURE:
      // A main thread gesture is for example a scroll gesture which is handled
      // by the main thread. Since we know the established gesture type, we can
      // be a little more aggressive about prioritizing compositing and input
      // handling over other tasks.
      new_policy.compositor_queue_policy().priority = TaskQueue::HIGH_PRIORITY;
      if (touchstart_expected_soon) {
        new_policy.rail_mode() = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      } else {
        expensive_task_policy = ExpensiveTaskPolicy::THROTTLE;
      }
      break;

    case UseCase::TOUCHSTART:
      new_policy.rail_mode() = v8::PERFORMANCE_RESPONSE;
      new_policy.compositor_queue_policy().priority = TaskQueue::HIGH_PRIORITY;
      new_policy.loading_queue_policy().is_blocked = true;
      new_policy.timer_queue_policy().is_blocked = true;
      // NOTE this is a nop due to the above.
      expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      break;

    case UseCase::NONE:
      // It's only safe to block tasks that if we are expecting a compositor
      // driven gesture.
      if (touchstart_expected_soon &&
          GetAnyThread().last_gesture_was_compositor_driven) {
        new_policy.rail_mode() = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      }
      break;

    case UseCase::LOADING:
      new_policy.rail_mode() = v8::PERFORMANCE_LOAD;
      // TODO(skyostil): Experiment with increasing loading and default queue
      // priorities and throttling rendering frame rate.
      break;

    default:
      NOTREACHED();
  }

  // TODO(skyostil): Add an idle state for foreground tabs too.
  if (GetMainThreadOnly().renderer_hidden)
    new_policy.rail_mode() = v8::PERFORMANCE_IDLE;

  if (expensive_task_policy == ExpensiveTaskPolicy::BLOCK &&
      (!GetMainThreadOnly().have_seen_a_begin_main_frame ||
       GetMainThreadOnly().navigation_task_expected_count > 0)) {
    expensive_task_policy = ExpensiveTaskPolicy::RUN;
  }

  switch (expensive_task_policy) {
    case ExpensiveTaskPolicy::RUN:
      break;

    case ExpensiveTaskPolicy::BLOCK:
      if (loading_tasks_seem_expensive)
        new_policy.loading_queue_policy().is_blocked = true;
      if (timer_tasks_seem_expensive)
        new_policy.timer_queue_policy().is_blocked = true;
      break;

    case ExpensiveTaskPolicy::THROTTLE:
      if (loading_tasks_seem_expensive) {
        new_policy.loading_queue_policy().is_throttled = true;
      }
      if (timer_tasks_seem_expensive) {
        new_policy.timer_queue_policy().is_throttled = true;
      }
      break;
  }
  GetMainThreadOnly().expensive_task_policy = expensive_task_policy;

  if (GetMainThreadOnly().timer_queue_suspend_count != 0 ||
      GetMainThreadOnly().timer_queue_suspended_when_backgrounded) {
    new_policy.timer_queue_policy().is_suspended = true;
  }

  if (GetMainThreadOnly().renderer_suspended) {
    new_policy.loading_queue_policy().is_suspended = true;
    new_policy.timer_queue_policy().is_suspended = true;
  }

  if (GetMainThreadOnly().use_virtual_time) {
    new_policy.compositor_queue_policy().use_virtual_time = true;
    new_policy.default_queue_policy().use_virtual_time = true;
    new_policy.loading_queue_policy().use_virtual_time = true;
    new_policy.timer_queue_policy().use_virtual_time = true;
  }

  new_policy.should_disable_throttling() =
      ShouldDisableThrottlingBecauseOfAudio(now) ||
      GetMainThreadOnly().use_virtual_time;

  // Tracing is done before the early out check, because it's quite possible we
  // will otherwise miss this information in traces.
  CreateTraceEventObjectSnapshotLocked();
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "use_case",
                 use_case);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "rail_mode",
                 new_policy.rail_mode());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "touchstart_expected_soon",
                 GetMainThreadOnly().touchstart_expected_soon);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "expensive_task_policy", expensive_task_policy);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.loading_tasks_seem_expensive",
                 GetMainThreadOnly().loading_tasks_seem_expensive);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.timer_tasks_seem_expensive",
                 GetMainThreadOnly().timer_tasks_seem_expensive);

  // TODO(alexclarke): Can we get rid of force update now?
  if (update_type == UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED &&
      new_policy == GetMainThreadOnly().current_policy) {
    return;
  }

  for (const auto& pair : task_runners_) {
    MainThreadTaskQueue::QueueClass queue_class = pair.first->queue_class();

    ApplyTaskQueuePolicy(
        pair.first.get(), pair.second.get(),
        GetMainThreadOnly().current_policy.GetQueuePolicy(queue_class),
        new_policy.GetQueuePolicy(queue_class));
  }

  if (GetMainThreadOnly().rail_mode_observer &&
      new_policy.rail_mode() !=
          GetMainThreadOnly().current_policy.rail_mode()) {
    GetMainThreadOnly().rail_mode_observer->OnRAILModeChanged(
        new_policy.rail_mode());
  }

  if (new_policy.should_disable_throttling() !=
      GetMainThreadOnly().current_policy.should_disable_throttling()) {
    if (new_policy.should_disable_throttling()) {
      task_queue_throttler()->DisableThrottling();
    } else {
      task_queue_throttler()->EnableThrottling();
    }
  }

  DCHECK(compositor_task_queue_->IsQueueEnabled());
  GetMainThreadOnly().current_policy = new_policy;

  if (timer_queue_newly_suspended)
    Platform::Current()->RequestPurgeMemory();
}

void RendererSchedulerImpl::ApplyTaskQueuePolicy(
    MainThreadTaskQueue* task_queue,
    TaskQueue::QueueEnabledVoter* task_queue_enabled_voter,
    const TaskQueuePolicy& old_task_queue_policy,
    const TaskQueuePolicy& new_task_queue_policy) const {
  DCHECK(old_task_queue_policy.IsQueueEnabled(task_queue) ||
         task_queue_enabled_voter);
  if (task_queue_enabled_voter) {
    task_queue_enabled_voter->SetQueueEnabled(
        new_task_queue_policy.IsQueueEnabled(task_queue));
  }

  // Make sure if there's no voter that the task queue is enabled.
  DCHECK(task_queue_enabled_voter ||
         old_task_queue_policy.IsQueueEnabled(task_queue));

  task_queue->SetQueuePriority(new_task_queue_policy.GetPriority(task_queue));

  TimeDomainType old_time_domain_type =
      old_task_queue_policy.GetTimeDomainType(task_queue);
  TimeDomainType new_time_domain_type =
      new_task_queue_policy.GetTimeDomainType(task_queue);

  if (old_time_domain_type != new_time_domain_type) {
    if (old_time_domain_type == TimeDomainType::THROTTLED) {
      task_queue->SetTimeDomain(real_time_domain());
      task_queue_throttler_->DecreaseThrottleRefCount(task_queue);
    } else if (new_time_domain_type == TimeDomainType::THROTTLED) {
      task_queue->SetTimeDomain(real_time_domain());
      task_queue_throttler_->IncreaseThrottleRefCount(task_queue);
    } else if (new_time_domain_type == TimeDomainType::VIRTUAL) {
      DCHECK(virtual_time_domain_);
      task_queue->SetTimeDomain(virtual_time_domain_.get());
    } else {
      task_queue->SetTimeDomain(real_time_domain());
    }
  }
}

RendererSchedulerImpl::UseCase RendererSchedulerImpl::ComputeCurrentUseCase(
    base::TimeTicks now,
    base::TimeDelta* expected_use_case_duration) const {
  any_thread_lock_.AssertAcquired();
  // Special case for flings. This is needed because we don't get notification
  // of a fling ending (although we do for cancellation).
  if (GetAnyThread().fling_compositor_escalation_deadline > now &&
      !GetAnyThread().awaiting_touch_start_response) {
    *expected_use_case_duration =
        GetAnyThread().fling_compositor_escalation_deadline - now;
    return UseCase::COMPOSITOR_GESTURE;
  }
  // Above all else we want to be responsive to user input.
  *expected_use_case_duration =
      GetAnyThread().user_model.TimeLeftInUserGesture(now);
  if (*expected_use_case_duration > base::TimeDelta()) {
    // Has a gesture been fully established?
    if (GetAnyThread().awaiting_touch_start_response) {
      // No, so arrange for compositor tasks to be run at the highest priority.
      return UseCase::TOUCHSTART;
    }

    // Yes a gesture has been established.  Based on how the gesture is handled
    // we need to choose between one of four use cases:
    // 1. COMPOSITOR_GESTURE where the gesture is processed only on the
    //    compositor thread.
    // 2. MAIN_THREAD_GESTURE where the gesture is processed only on the main
    //    thread.
    // 3. MAIN_THREAD_CUSTOM_INPUT_HANDLING where the main thread processes a
    //    stream of input events and has prevented a default gesture from being
    //    started.
    // 4. SYNCHRONIZED_GESTURE where the gesture is processed on both threads.
    if (GetAnyThread().last_gesture_was_compositor_driven) {
      if (GetAnyThread().begin_main_frame_on_critical_path) {
        return UseCase::SYNCHRONIZED_GESTURE;
      } else {
        return UseCase::COMPOSITOR_GESTURE;
      }
    }
    if (GetAnyThread().default_gesture_prevented) {
      return UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING;
    } else {
      return UseCase::MAIN_THREAD_GESTURE;
    }
  }

  // Occasionally the meaningful paint fails to be detected, so as a fallback we
  // treat the presence of input as an indirect signal that there is meaningful
  // content on the page.
  if (GetAnyThread().waiting_for_meaningful_paint &&
      !GetAnyThread().have_seen_input_since_navigation) {
    return UseCase::LOADING;
  }
  return UseCase::NONE;
}

base::TimeDelta RendererSchedulerImpl::EstimateLongestJankFreeTaskDuration()
    const {
  switch (GetMainThreadOnly().current_use_case) {
    case UseCase::TOUCHSTART:
    case UseCase::COMPOSITOR_GESTURE:
    case UseCase::LOADING:
    case UseCase::NONE:
      return base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis);

    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
    case UseCase::MAIN_THREAD_GESTURE:
    case UseCase::SYNCHRONIZED_GESTURE:
      return GetMainThreadOnly().idle_time_estimator.GetExpectedIdleDuration(
          GetMainThreadOnly().compositor_frame_interval);

    default:
      NOTREACHED();
      return base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis);
  }
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (GetMainThreadOnly().current_use_case == UseCase::TOUCHSTART) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out =
        std::max(base::TimeDelta(),
                 GetMainThreadOnly().current_policy_expiration_time - now);
    return false;
  }
  return true;
}

MainThreadSchedulerHelper*
RendererSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

TaskCostEstimator*
RendererSchedulerImpl::GetLoadingTaskCostEstimatorForTesting() {
  return &GetMainThreadOnly().loading_task_cost_estimator;
}

TaskCostEstimator*
RendererSchedulerImpl::GetTimerTaskCostEstimatorForTesting() {
  return &GetMainThreadOnly().timer_task_cost_estimator;
}

IdleTimeEstimator* RendererSchedulerImpl::GetIdleTimeEstimatorForTesting() {
  return &GetMainThreadOnly().idle_time_estimator;
}

WakeUpBudgetPool* RendererSchedulerImpl::GetWakeUpBudgetPoolForTesting() {
  return GetMainThreadOnly().wake_up_budget_pool;
}

void RendererSchedulerImpl::SuspendTimerQueue() {
  GetMainThreadOnly().timer_queue_suspend_count++;
  ForceUpdatePolicy();
#ifndef NDEBUG
  DCHECK(!default_timer_task_queue_->IsQueueEnabled());
  for (const auto& pair : task_runners_) {
    if (pair.first->queue_class() == MainThreadTaskQueue::QueueClass::TIMER &&
        pair.first->CanBeSuspended()) {
      DCHECK(!pair.first->IsQueueEnabled());
    }
  }
#endif
}

void RendererSchedulerImpl::ResumeTimerQueue() {
  GetMainThreadOnly().timer_queue_suspend_count--;
  DCHECK_GE(GetMainThreadOnly().timer_queue_suspend_count, 0);
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::VirtualTimePaused() {
  DCHECK(!GetMainThreadOnly().virtual_time_paused);
  GetMainThreadOnly().virtual_time_paused = true;
  for (const auto& pair : task_runners_) {
    if (pair.first->queue_class() == MainThreadTaskQueue::QueueClass::TIMER) {
      DCHECK(!task_queue_throttler_->IsThrottled(pair.first.get()));
      DCHECK(!pair.first->HasFence());
      pair.first->InsertFence(TaskQueue::InsertFencePosition::NOW);
    }
  }
}

void RendererSchedulerImpl::VirtualTimeResumed() {
  DCHECK(GetMainThreadOnly().virtual_time_paused);
  GetMainThreadOnly().virtual_time_paused = false;
  for (const auto& pair : task_runners_) {
    if (pair.first->queue_class() == MainThreadTaskQueue::QueueClass::TIMER) {
      DCHECK(!task_queue_throttler_->IsThrottled(pair.first.get()));
      DCHECK(pair.first->HasFence());
      pair.first->RemoveFence();
    }
  }
}

void RendererSchedulerImpl::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {
  // Note that this will only take effect for the next backgrounded signal.
  GetMainThreadOnly().timer_queue_suspension_when_backgrounded_enabled =
      enabled;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValue(base::TimeTicks optional_now) const {
  base::AutoLock lock(any_thread_lock_);
  return AsValueLocked(optional_now);
}

void RendererSchedulerImpl::CreateTraceEventObjectSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.scheduler_tqm_delegate()->NowTicks()));
}

void RendererSchedulerImpl::CreateTraceEventObjectSnapshotLocked() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(helper_.scheduler_tqm_delegate()->NowTicks()));
}

// static
const char* RendererSchedulerImpl::ExpensiveTaskPolicyToString(
    ExpensiveTaskPolicy expensive_task_policy) {
  switch (expensive_task_policy) {
    case ExpensiveTaskPolicy::RUN:
      return "RUN";
    case ExpensiveTaskPolicy::BLOCK:
      return "BLOCK";
    case ExpensiveTaskPolicy::THROTTLE:
      return "THROTTLE";
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.scheduler_tqm_delegate()->NowTicks();
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetBoolean(
      "has_visible_render_widget_with_touch_handler",
      GetMainThreadOnly().has_visible_render_widget_with_touch_handler);
  state->SetString("current_use_case",
                   UseCaseToString(GetMainThreadOnly().current_use_case));
  state->SetBoolean("loading_tasks_seem_expensive",
                    GetMainThreadOnly().loading_tasks_seem_expensive);
  state->SetBoolean("timer_tasks_seem_expensive",
                    GetMainThreadOnly().timer_tasks_seem_expensive);
  state->SetBoolean("begin_frame_not_expected_soon",
                    GetMainThreadOnly().begin_frame_not_expected_soon);
  state->SetBoolean(
      "compositor_will_send_main_frame_not_expected",
      GetMainThreadOnly().compositor_will_send_main_frame_not_expected);
  state->SetBoolean("touchstart_expected_soon",
                    GetMainThreadOnly().touchstart_expected_soon);
  state->SetString("idle_period_state",
                   IdleHelper::IdlePeriodStateToString(
                       idle_helper_.SchedulerIdlePeriodState()));
  state->SetBoolean("renderer_hidden", GetMainThreadOnly().renderer_hidden);
  state->SetBoolean("have_seen_a_begin_main_frame",
                    GetMainThreadOnly().have_seen_a_begin_main_frame);
  state->SetBoolean("waiting_for_meaningful_paint",
                    GetAnyThread().waiting_for_meaningful_paint);
  state->SetBoolean("have_seen_input_since_navigation",
                    GetAnyThread().have_seen_input_since_navigation);
  state->SetBoolean("have_reported_blocking_intervention_in_current_policy",
                    GetMainThreadOnly()
                        .have_reported_blocking_intervention_in_current_policy);
  state->SetBoolean(
      "have_reported_blocking_intervention_since_navigation",
      GetMainThreadOnly().have_reported_blocking_intervention_since_navigation);
  state->SetBoolean("renderer_backgrounded",
                    GetMainThreadOnly().renderer_backgrounded);
  state->SetBoolean(
      "timer_queue_suspended_when_backgrounded",
      GetMainThreadOnly().timer_queue_suspended_when_backgrounded);
  state->SetInteger("timer_queue_suspend_count",
                    GetMainThreadOnly().timer_queue_suspend_count);
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "fling_compositor_escalation_deadline",
      (GetAnyThread().fling_compositor_escalation_deadline - base::TimeTicks())
          .InMillisecondsF());
  state->SetInteger("navigation_task_expected_count",
                    GetMainThreadOnly().navigation_task_expected_count);
  state->SetDouble(
      "last_idle_period_end_time",
      (GetAnyThread().last_idle_period_end_time - base::TimeTicks())
          .InMillisecondsF());
  state->SetBoolean("awaiting_touch_start_response",
                    GetAnyThread().awaiting_touch_start_response);
  state->SetBoolean("begin_main_frame_on_critical_path",
                    GetAnyThread().begin_main_frame_on_critical_path);
  state->SetBoolean("last_gesture_was_compositor_driven",
                    GetAnyThread().last_gesture_was_compositor_driven);
  state->SetBoolean("default_gesture_prevented",
                    GetAnyThread().default_gesture_prevented);
  state->SetDouble("expected_loading_task_duration",
                   GetMainThreadOnly()
                       .loading_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  state->SetDouble("expected_timer_task_duration",
                   GetMainThreadOnly()
                       .timer_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  state->SetBoolean("is_audio_playing", GetMainThreadOnly().is_audio_playing);
  state->SetBoolean("virtual_time_paused",
                    GetMainThreadOnly().virtual_time_paused);

  state->BeginDictionary("web_view_schedulers");
  for (WebViewSchedulerImpl* web_view_scheduler :
       GetMainThreadOnly().web_view_schedulers) {
    state->BeginDictionaryWithCopiedName(
        trace_helper::PointerToString(web_view_scheduler));
    web_view_scheduler->AsValueInto(state.get());
    state->EndDictionary();
  }
  state->EndDictionary();

  state->BeginDictionary("policy");
  GetMainThreadOnly().current_policy.AsValueInto(state.get());
  state->EndDictionary();

  // TODO(skyostil): Can we somehow trace how accurate these estimates were?
  state->SetDouble(
      "longest_jank_free_task_duration",
      GetMainThreadOnly().longest_jank_free_task_duration.InMillisecondsF());
  state->SetDouble(
      "compositor_frame_interval",
      GetMainThreadOnly().compositor_frame_interval.InMillisecondsF());
  state->SetDouble(
      "estimated_next_frame_begin",
      (GetMainThreadOnly().estimated_next_frame_begin - base::TimeTicks())
          .InMillisecondsF());
  state->SetBoolean("in_idle_period", GetAnyThread().in_idle_period);

  state->SetString(
      "expensive_task_policy",
      ExpensiveTaskPolicyToString(GetMainThreadOnly().expensive_task_policy));

  GetAnyThread().user_model.AsValueInto(state.get());
  render_widget_scheduler_signals_.AsValueInto(state.get());

  state->BeginDictionary("task_queue_throttler");
  task_queue_throttler_->AsValueInto(state.get(), optional_now);
  state->EndDictionary();

  return std::move(state);
}

bool RendererSchedulerImpl::TaskQueuePolicy::IsQueueEnabled(
    MainThreadTaskQueue* task_queue) const {
  if (!is_enabled)
    return false;
  if (is_suspended && task_queue->CanBeSuspended())
    return false;
  if (is_blocked && task_queue->CanBeBlocked())
    return false;
  return true;
}

TaskQueue::QueuePriority RendererSchedulerImpl::TaskQueuePolicy::GetPriority(
    MainThreadTaskQueue* task_queue) const {
  return priority;
}

RendererSchedulerImpl::TimeDomainType
RendererSchedulerImpl::TaskQueuePolicy::GetTimeDomainType(
    MainThreadTaskQueue* task_queue) const {
  if (use_virtual_time)
    return TimeDomainType::VIRTUAL;
  if (is_throttled && task_queue->CanBeThrottled())
    return TimeDomainType::THROTTLED;
  return TimeDomainType::REAL;
}

void RendererSchedulerImpl::TaskQueuePolicy::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("is_enabled", is_enabled);
  state->SetBoolean("is_suspended", is_suspended);
  state->SetBoolean("is_throttled", is_throttled);
  state->SetBoolean("is_blocked", is_blocked);
  state->SetBoolean("use_virtual_time", use_virtual_time);
  state->SetString("priority", TaskQueue::PriorityToString(priority));
}

void RendererSchedulerImpl::Policy::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->BeginDictionary("compositor_queue_policy");
  compositor_queue_policy().AsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("loading_queue_policy");
  loading_queue_policy().AsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("timer_queue_policy");
  timer_queue_policy().AsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("default_queue_policy");
  default_queue_policy().AsValueInto(state);
  state->EndDictionary();

  state->SetString("rail_mode", RAILModeToString(rail_mode()));
  state->SetBoolean("should_disable_throttling", should_disable_throttling());
}

void RendererSchedulerImpl::OnIdlePeriodStarted() {
  base::AutoLock lock(any_thread_lock_);
  GetAnyThread().in_idle_period = true;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnIdlePeriodEnded() {
  base::AutoLock lock(any_thread_lock_);
  GetAnyThread().last_idle_period_end_time =
      helper_.scheduler_tqm_delegate()->NowTicks();
  GetAnyThread().in_idle_period = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnPendingTasksChanged(bool has_tasks) {
  if (has_tasks ==
      GetMainThreadOnly().compositor_will_send_main_frame_not_expected)
    return;

  GetMainThreadOnly().compositor_will_send_main_frame_not_expected = has_tasks;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnPendingTasksChanged", "has_tasks",
               has_tasks);
  for (WebViewSchedulerImpl* web_view_scheduler :
       GetMainThreadOnly().web_view_schedulers) {
    web_view_scheduler->RequestBeginMainFrameNotExpected(has_tasks);
  }
}

void RendererSchedulerImpl::AddPendingNavigation(NavigatingFrameType type) {
  helper_.CheckOnValidThread();
  if (type == NavigatingFrameType::kMainFrame) {
    GetMainThreadOnly().navigation_task_expected_count++;
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::RemovePendingNavigation(NavigatingFrameType type) {
  helper_.CheckOnValidThread();
  DCHECK_GT(GetMainThreadOnly().navigation_task_expected_count, 0);
  if (type == NavigatingFrameType::kMainFrame &&
      GetMainThreadOnly().navigation_task_expected_count > 0) {
    GetMainThreadOnly().navigation_task_expected_count--;
    UpdatePolicy();
  }
}

std::unique_ptr<base::SingleSampleMetric>
RendererSchedulerImpl::CreateMaxQueueingTimeMetric() {
  return base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
      "RendererScheduler.MaxQueueingTime", 1, 10000, 50);
}

void RendererSchedulerImpl::OnNavigate() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnNavigate");
  base::AutoLock lock(any_thread_lock_);
  ResetForNavigationLocked();
}

void RendererSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidStartProvisionalLoad");
  if (is_main_frame) {
    base::AutoLock lock(any_thread_lock_);
    ResetForNavigationLocked();
  }
}

void RendererSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnDidCommitProvisionalLoad");
  // Initialize |max_queueing_time_metric| lazily so that
  // |SingleSampleMetricsFactory::SetFactory()| is called before
  // |SingleSampleMetricsFactory::Get()|
  if (!GetMainThreadOnly().max_queueing_time_metric) {
    GetMainThreadOnly().max_queueing_time_metric =
        CreateMaxQueueingTimeMetric();
  }
  GetMainThreadOnly().max_queueing_time_metric.reset();
  GetMainThreadOnly().max_queueing_time = base::TimeDelta();
  GetMainThreadOnly().has_navigated = true;

  // If this either isn't a history inert commit or it's a reload then we must
  // reset the task cost estimators.
  if (is_main_frame && (!is_web_history_inert_commit || is_reload)) {
    base::AutoLock lock(any_thread_lock_);
    ResetForNavigationLocked();
  }
}

void RendererSchedulerImpl::OnFirstMeaningfulPaint() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnFirstMeaningfulPaint");
  base::AutoLock lock(any_thread_lock_);
  GetAnyThread().waiting_for_meaningful_paint = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ResetForNavigationLocked() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::ResetForNavigationLocked");
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  GetAnyThread().user_model.Reset(helper_.scheduler_tqm_delegate()->NowTicks());
  GetAnyThread().have_seen_a_potentially_blocking_gesture = false;
  GetAnyThread().waiting_for_meaningful_paint = true;
  GetAnyThread().have_seen_input_since_navigation = false;
  GetMainThreadOnly().loading_task_cost_estimator.Clear();
  GetMainThreadOnly().timer_task_cost_estimator.Clear();
  GetMainThreadOnly().idle_time_estimator.Clear();
  GetMainThreadOnly().have_seen_a_begin_main_frame = false;
  GetMainThreadOnly().have_reported_blocking_intervention_since_navigation =
      false;
  for (WebViewSchedulerImpl* web_view_scheduler :
       GetMainThreadOnly().web_view_schedulers) {
    web_view_scheduler->OnNavigation();
  }
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {
  // Any task that runs in the default task runners belongs to the context of
  // all frames (as opposed to a particular frame). Note that the task itself
  // may still enter a more specific blame context if necessary.
  //
  // Per-frame task runners (loading, timers, etc.) are configured with a more
  // specific blame context by WebFrameSchedulerImpl.
  control_task_queue_->SetBlameContext(blame_context);
  DefaultTaskQueue()->SetBlameContext(blame_context);
  default_loading_task_queue_->SetBlameContext(blame_context);
  default_timer_task_queue_->SetBlameContext(blame_context);
  compositor_task_queue_->SetBlameContext(blame_context);
  idle_helper_.IdleTaskRunner()->SetBlameContext(blame_context);
}

void RendererSchedulerImpl::SetRAILModeObserver(RAILModeObserver* observer) {
  GetMainThreadOnly().rail_mode_observer = observer;
}

bool RendererSchedulerImpl::MainThreadSeemsUnresponsive(
    base::TimeDelta main_thread_responsiveness_threshold) {
  base::TimeTicks now = tick_clock()->NowTicks();
  base::TimeDelta estimated_queueing_time;

  bool can_read = false;

  base::subtle::Atomic32 version;
  seqlock_queueing_time_estimator_.seqlock.TryRead(&can_read, &version);

  // If we fail to determine if the main thread is busy, assume whether or not
  // it's busy hasn't change since the last time we asked.
  if (!can_read)
    return GetCompositorThreadOnly().main_thread_seems_unresponsive;

  QueueingTimeEstimator::State queueing_time_estimator_state =
      seqlock_queueing_time_estimator_.data.GetState();

  // If we fail to determine if the main thread is busy, assume whether or not
  // it's busy hasn't change since the last time we asked.
  if (seqlock_queueing_time_estimator_.seqlock.ReadRetry(version))
    return GetCompositorThreadOnly().main_thread_seems_unresponsive;

  QueueingTimeEstimator queueing_time_estimator(queueing_time_estimator_state);

  estimated_queueing_time =
      queueing_time_estimator.EstimateQueueingTimeIncludingCurrentTask(now);

  bool main_thread_seems_unresponsive =
      estimated_queueing_time > main_thread_responsiveness_threshold;
  GetCompositorThreadOnly().main_thread_seems_unresponsive =
      main_thread_seems_unresponsive;

  return main_thread_seems_unresponsive;
}

void RendererSchedulerImpl::RegisterTimeDomain(TimeDomain* time_domain) {
  helper_.RegisterTimeDomain(time_domain);
}

void RendererSchedulerImpl::UnregisterTimeDomain(TimeDomain* time_domain) {
  helper_.UnregisterTimeDomain(time_domain);
}

base::TickClock* RendererSchedulerImpl::tick_clock() const {
  return helper_.scheduler_tqm_delegate().get();
}

void RendererSchedulerImpl::AddWebViewScheduler(
    WebViewSchedulerImpl* web_view_scheduler) {
  GetMainThreadOnly().web_view_schedulers.insert(web_view_scheduler);
}

void RendererSchedulerImpl::RemoveWebViewScheduler(
    WebViewSchedulerImpl* web_view_scheduler) {
  DCHECK(GetMainThreadOnly().web_view_schedulers.find(web_view_scheduler) !=
         GetMainThreadOnly().web_view_schedulers.end());
  GetMainThreadOnly().web_view_schedulers.erase(web_view_scheduler);
}

void RendererSchedulerImpl::BroadcastIntervention(const std::string& message) {
  helper_.CheckOnValidThread();
  for (auto* web_view_scheduler : GetMainThreadOnly().web_view_schedulers)
    web_view_scheduler->ReportIntervention(message);
}

void RendererSchedulerImpl::OnTriedToExecuteBlockedTask() {
  if (GetMainThreadOnly().current_use_case == UseCase::TOUCHSTART ||
      GetMainThreadOnly().longest_jank_free_task_duration <
          base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis) ||
      GetMainThreadOnly().timer_queue_suspend_count ||
      GetMainThreadOnly().timer_queue_suspended_when_backgrounded) {
    return;
  }
  if (!GetMainThreadOnly().timer_tasks_seem_expensive &&
      !GetMainThreadOnly().loading_tasks_seem_expensive) {
    return;
  }
  if (!GetMainThreadOnly()
           .have_reported_blocking_intervention_in_current_policy) {
    GetMainThreadOnly().have_reported_blocking_intervention_in_current_policy =
        true;
    TRACE_EVENT_INSTANT0("renderer.scheduler",
                         "RendererSchedulerImpl::TaskBlocked",
                         TRACE_EVENT_SCOPE_THREAD);
  }

  if (!GetMainThreadOnly()
           .have_reported_blocking_intervention_since_navigation) {
    {
      base::AutoLock lock(any_thread_lock_);
      if (!GetAnyThread().have_seen_a_potentially_blocking_gesture)
        return;
    }
    GetMainThreadOnly().have_reported_blocking_intervention_since_navigation =
        true;
    BroadcastIntervention(
        "Blink deferred a task in order to make scrolling smoother. "
        "Your timer and network tasks should take less than 50ms to run "
        "to avoid this. Please see "
        "https://developers.google.com/web/tools/chrome-devtools/profile/"
        "evaluate-performance/rail"
        " and https://crbug.com/574343#c40 for more information.");
  }
}

void RendererSchedulerImpl::WillProcessTask(double start_time) {
  base::TimeTicks start_time_ticks =
      MonotonicTimeInSecondsToTimeTicks(start_time);
  GetMainThreadOnly().current_task_start_time = start_time_ticks;
  seqlock_queueing_time_estimator_.seqlock.WriteBegin();
  seqlock_queueing_time_estimator_.data.OnTopLevelTaskStarted(start_time_ticks);
  seqlock_queueing_time_estimator_.seqlock.WriteEnd();
}

void RendererSchedulerImpl::DidProcessTask(double start_time, double end_time) {
  DCHECK_LE(start_time, end_time);
  // TODO(scheduler-dev): Remove conversions when Blink starts using
  // base::TimeTicks instead of doubles for time.
  base::TimeTicks end_time_ticks = MonotonicTimeInSecondsToTimeTicks(end_time);

  seqlock_queueing_time_estimator_.seqlock.WriteBegin();
  seqlock_queueing_time_estimator_.data.OnTopLevelTaskCompleted(end_time_ticks);
  seqlock_queueing_time_estimator_.seqlock.WriteEnd();
}

void RendererSchedulerImpl::OnTaskCompleted(MainThreadTaskQueue* queue,
                                            const TaskQueue::Task& task,
                                            base::TimeTicks start,
                                            base::TimeTicks end) {
  task_queue_throttler()->OnTaskRunTimeReported(queue, start, end);

  // TODO(altimin): Per-page metrics should also be considered.
  RecordTaskMetrics(queue->queue_type(), start, end);
}

namespace {

// Calculates the length of the intersection of two given time intervals.
base::TimeDelta DurationOfIntervalOverlap(base::TimeTicks start1,
                                          base::TimeTicks end1,
                                          base::TimeTicks start2,
                                          base::TimeTicks end2) {
  DCHECK_LE(start1, end1);
  DCHECK_LE(start2, end2);
  return std::max(std::min(end1, end2) - std::max(start1, start2),
                  base::TimeDelta());
}

}  // namespace

void RendererSchedulerImpl::RecordTaskMetrics(
    MainThreadTaskQueue::QueueType queue_type,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  base::TimeDelta duration = end_time - start_time;
  if (duration > kLongTaskDiscardingThreshold)
    return;

  // Discard anomalously long idle periods.
  if (GetMainThreadOnly().last_reported_task &&
      start_time - GetMainThreadOnly().last_reported_task.value() >
          kLongIdlePeriodDiscardingThreshold) {
    GetMainThreadOnly().main_thread_load_tracker.Reset(end_time);
    GetMainThreadOnly().foreground_main_thread_load_tracker.Reset(end_time);
    GetMainThreadOnly().background_main_thread_load_tracker.Reset(end_time);
    return;
  }

  GetMainThreadOnly().last_reported_task = end_time;

  UMA_HISTOGRAM_CUSTOM_COUNTS("RendererScheduler.TaskTime2",
                              duration.InMicroseconds(), 1, 1000 * 1000, 50);

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  GetMainThreadOnly().main_thread_load_tracker.RecordTaskTime(start_time,
                                                              end_time);
  GetMainThreadOnly().foreground_main_thread_load_tracker.RecordTaskTime(
      start_time, end_time);
  GetMainThreadOnly().background_main_thread_load_tracker.RecordTaskTime(
      start_time, end_time);

  // TODO(altimin): See whether this metric is still useful after
  // adding RendererScheduler.TaskDurationPerQueueType.
  UMA_HISTOGRAM_ENUMERATION(
      "RendererScheduler.NumberOfTasksPerQueueType2",
      static_cast<int>(queue_type),
      static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));

  GetMainThreadOnly().task_duration_reporter.RecordTask(queue_type, duration);

  if (GetMainThreadOnly().renderer_backgrounded) {
    GetMainThreadOnly().background_task_duration_reporter.RecordTask(queue_type,
                                                                     duration);

    // Collect detailed breakdown for first five minutes given that we suspend
    // timers on mobile after five minutes.
    base::TimeTicks backgrounded_at =
        GetMainThreadOnly().background_status_changed_at;

    GetMainThreadOnly()
        .background_first_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time, backgrounded_at,
                            backgrounded_at + base::TimeDelta::FromMinutes(1)));

    GetMainThreadOnly()
        .background_second_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            backgrounded_at + base::TimeDelta::FromMinutes(1),
                            backgrounded_at + base::TimeDelta::FromMinutes(2)));

    GetMainThreadOnly()
        .background_third_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            backgrounded_at + base::TimeDelta::FromMinutes(2),
                            backgrounded_at + base::TimeDelta::FromMinutes(3)));

    GetMainThreadOnly()
        .background_fourth_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            backgrounded_at + base::TimeDelta::FromMinutes(3),
                            backgrounded_at + base::TimeDelta::FromMinutes(4)));

    GetMainThreadOnly()
        .background_fifth_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            backgrounded_at + base::TimeDelta::FromMinutes(4),
                            backgrounded_at + base::TimeDelta::FromMinutes(5)));

    GetMainThreadOnly()
        .background_after_fifth_minute_task_duration_reporter.RecordTask(
            queue_type,
            DurationOfIntervalOverlap(
                start_time, end_time,
                backgrounded_at + base::TimeDelta::FromMinutes(5),
                std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                         end_time)));
  } else {
    GetMainThreadOnly().foreground_task_duration_reporter.RecordTask(queue_type,
                                                                     duration);

    // For foreground tabs we do not expect such a notable difference as it is
    // the case with background tabs, so we limit breakdown to three minutes.
    base::TimeTicks foregrounded_at =
        GetMainThreadOnly().background_status_changed_at;

    GetMainThreadOnly()
        .foreground_first_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time, foregrounded_at,
                            foregrounded_at + base::TimeDelta::FromMinutes(1)));

    GetMainThreadOnly()
        .foreground_second_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            foregrounded_at + base::TimeDelta::FromMinutes(1),
                            foregrounded_at + base::TimeDelta::FromMinutes(2)));

    GetMainThreadOnly()
        .foreground_third_minute_task_duration_reporter.RecordTask(
            queue_type, DurationOfIntervalOverlap(
                            start_time, end_time,
                            foregrounded_at + base::TimeDelta::FromMinutes(2),
                            foregrounded_at + base::TimeDelta::FromMinutes(3)));

    GetMainThreadOnly()
        .foreground_after_third_minute_task_duration_reporter.RecordTask(
            queue_type,
            DurationOfIntervalOverlap(
                start_time, end_time,
                foregrounded_at + base::TimeDelta::FromMinutes(3),
                std::max(foregrounded_at + base::TimeDelta::FromMinutes(3),
                         end_time)));
  }

  if (GetMainThreadOnly().renderer_hidden) {
    GetMainThreadOnly().hidden_task_duration_reporter.RecordTask(queue_type,
                                                                 duration);

    if (ShouldDisableThrottlingBecauseOfAudio(start_time)) {
      GetMainThreadOnly().hidden_music_task_duration_reporter.RecordTask(
          queue_type, duration);
    }
  } else {
    GetMainThreadOnly().visible_task_duration_reporter.RecordTask(queue_type,
                                                                  duration);
  }
}

void RendererSchedulerImpl::RecordMainThreadTaskLoad(base::TimeTicks time,
                                                     double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  UMA_HISTOGRAM_PERCENTAGE("RendererScheduler.RendererMainThreadLoad3",
                           load_percentage);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad", load_percentage);
}

void RendererSchedulerImpl::RecordForegroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  UMA_HISTOGRAM_PERCENTAGE(
      "RendererScheduler.RendererMainThreadLoad3.Foreground", load_percentage);

  if (time - GetMainThreadOnly().background_status_changed_at >
      base::TimeDelta::FromMinutes(1)) {
    UMA_HISTOGRAM_PERCENTAGE(
        "RendererScheduler.RendererMainThreadLoad3.Foreground.AfterFirstMinute",
        load_percentage);
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad.Foreground",
                 load_percentage);
}

void RendererSchedulerImpl::RecordBackgroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  UMA_HISTOGRAM_PERCENTAGE(
      "RendererScheduler.RendererMainThreadLoad3.Background", load_percentage);

  if (time - GetMainThreadOnly().background_status_changed_at >
      base::TimeDelta::FromMinutes(1)) {
    UMA_HISTOGRAM_PERCENTAGE(
        "RendererScheduler.RendererMainThreadLoad3.Background.AfterFirstMinute",
        load_percentage);
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad.Background",
                 load_percentage);
}

void RendererSchedulerImpl::OnBeginNestedRunLoop() {
  seqlock_queueing_time_estimator_.seqlock.WriteBegin();
  seqlock_queueing_time_estimator_.data.OnBeginNestedRunLoop();
  seqlock_queueing_time_estimator_.seqlock.WriteEnd();
}

void RendererSchedulerImpl::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.AddTaskTimeObserver(task_time_observer);
}

void RendererSchedulerImpl::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.RemoveTaskTimeObserver(task_time_observer);
}

void RendererSchedulerImpl::OnQueueingTimeForWindowEstimated(
    base::TimeDelta queueing_time,
    base::TimeTicks window_start_time) {
  GetMainThreadOnly().most_recent_expected_queueing_time = queueing_time;

  if (GetMainThreadOnly().has_navigated) {
    if (GetMainThreadOnly().max_queueing_time < queueing_time) {
      if (!GetMainThreadOnly().max_queueing_time_metric) {
        GetMainThreadOnly().max_queueing_time_metric =
            CreateMaxQueueingTimeMetric();
      }
      GetMainThreadOnly().max_queueing_time_metric->SetSample(
          queueing_time.InMilliseconds());
      GetMainThreadOnly().max_queueing_time = queueing_time;
    }
  }

  // RendererScheduler reports the queueing time once per window's duration.
  //          |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
  // Report:  |-------window EQT------|
  // Discard:         |-------window EQT------|
  // Discard:                 |-------window EQT------|
  // Report:                          |-------window EQT------|
  if (window_start_time -
          GetMainThreadOnly().uma_last_queueing_time_report_window_start_time <
      kQueueingTimeWindowDuration) {
    return;
  }
  UMA_HISTOGRAM_TIMES("RendererScheduler.ExpectedTaskQueueingDuration",
                      queueing_time);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "estimated_queueing_time_for_window",
                 queueing_time.InMillisecondsF());
  GetMainThreadOnly().uma_last_queueing_time_report_window_start_time =
      window_start_time;
}

AutoAdvancingVirtualTimeDomain* RendererSchedulerImpl::GetVirtualTimeDomain() {
  if (!virtual_time_domain_) {
    virtual_time_domain_.reset(
        new AutoAdvancingVirtualTimeDomain(tick_clock()->NowTicks()));
    RegisterTimeDomain(virtual_time_domain_.get());
  }
  return virtual_time_domain_.get();
}

void RendererSchedulerImpl::EnableVirtualTime() {
  GetMainThreadOnly().use_virtual_time = true;

  DCHECK(!virtual_time_control_task_queue_);
  virtual_time_control_task_queue_ =
      helper_.NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::CONTROL));
  virtual_time_control_task_queue_->SetQueuePriority(
      TaskQueue::CONTROL_PRIORITY);
  virtual_time_control_task_queue_->SetTimeDomain(GetVirtualTimeDomain());

  ForceUpdatePolicy();
}

void RendererSchedulerImpl::DisableVirtualTimeForTesting() {
  GetMainThreadOnly().use_virtual_time = false;

  virtual_time_control_task_queue_->UnregisterTaskQueue();
  virtual_time_control_task_queue_ = nullptr;

  ForceUpdatePolicy();
}

bool RendererSchedulerImpl::ShouldDisableThrottlingBecauseOfAudio(
    base::TimeTicks now) {
  if (!GetMainThreadOnly().last_audio_state_change)
    return false;

  if (GetMainThreadOnly().is_audio_playing)
    return true;

  return GetMainThreadOnly().last_audio_state_change.value() +
             kThrottlingDelayAfterAudioIsPlayed >
         now;
}

void RendererSchedulerImpl::AddQueueToWakeUpBudgetPool(
    MainThreadTaskQueue* queue) {
  if (!GetMainThreadOnly().wake_up_budget_pool) {
    GetMainThreadOnly().wake_up_budget_pool =
        task_queue_throttler()->CreateWakeUpBudgetPool("renderer_wake_up_pool");
    GetMainThreadOnly().wake_up_budget_pool->SetWakeUpRate(1);
    GetMainThreadOnly().wake_up_budget_pool->SetWakeUpDuration(
        base::TimeDelta());
  }
  GetMainThreadOnly().wake_up_budget_pool->AddQueue(tick_clock()->NowTicks(),
                                                    queue);
}

TimeDomain* RendererSchedulerImpl::GetActiveTimeDomain() {
  if (GetMainThreadOnly().use_virtual_time) {
    return GetVirtualTimeDomain();
  } else {
    return real_time_domain();
  }
}

void RendererSchedulerImpl::OnTraceLogEnabled() {
  CreateTraceEventObjectSnapshot();
}

void RendererSchedulerImpl::OnTraceLogDisabled() {}

// static
const char* RendererSchedulerImpl::UseCaseToString(UseCase use_case) {
  switch (use_case) {
    case UseCase::NONE:
      return "none";
    case UseCase::COMPOSITOR_GESTURE:
      return "compositor_gesture";
    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
      return "main_thread_custom_input_handling";
    case UseCase::SYNCHRONIZED_GESTURE:
      return "synchronized_gesture";
    case UseCase::TOUCHSTART:
      return "touchstart";
    case UseCase::LOADING:
      return "loading";
    case UseCase::MAIN_THREAD_GESTURE:
      return "main_thread_gesture";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* RendererSchedulerImpl::RAILModeToString(v8::RAILMode rail_mode) {
  switch (rail_mode) {
    case v8::PERFORMANCE_RESPONSE:
      return "response";
    case v8::PERFORMANCE_ANIMATION:
      return "animation";
    case v8::PERFORMANCE_IDLE:
      return "idle";
    case v8::PERFORMANCE_LOAD:
      return "load";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* RendererSchedulerImpl::TimeDomainTypeToString(
    TimeDomainType domain_type) {
  switch (domain_type) {
    case TimeDomainType::REAL:
      return "real";
    case TimeDomainType::THROTTLED:
      return "throttled";
    case TimeDomainType::VIRTUAL:
      return "virtual";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
