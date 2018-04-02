// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/blame_context.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/default_params.h"
#include "platform/scheduler/child/page_visibility_state.h"
#include "platform/scheduler/child/task_runner_impl.h"
#include "platform/scheduler/child/worker_scheduler_proxy.h"
#include "platform/scheduler/common/throttling/budget_pool.h"
#include "platform/scheduler/main_thread/main_thread_scheduler.h"
#include "platform/scheduler/main_thread/page_scheduler_impl.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/util/tracing_helper.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebString.h"

namespace blink {
namespace scheduler {

namespace {

const char* VisibilityStateToString(bool is_visible) {
  if (is_visible) {
    return "visible";
  } else {
    return "hidden";
  }
}

const char* PausedStateToString(bool is_paused) {
  if (is_paused) {
    return "paused";
  } else {
    return "running";
  }
}

const char* FrozenStateToString(bool is_frozen) {
  if (is_frozen) {
    return "frozen";
  } else {
    return "running";
  }
}

const char* KeepActiveStateToString(bool keep_active) {
  if (keep_active) {
    return "keep_active";
  } else {
    return "no_keep_active";
  }
}

}  // namespace

FrameSchedulerImpl::ActiveConnectionHandleImpl::ActiveConnectionHandleImpl(
    FrameSchedulerImpl* frame_scheduler)
    : frame_scheduler_(frame_scheduler->GetWeakPtr()) {
  frame_scheduler->DidOpenActiveConnection();
}

FrameSchedulerImpl::ActiveConnectionHandleImpl::~ActiveConnectionHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->DidCloseActiveConnection();
}

FrameSchedulerImpl::ThrottlingObserverHandleImpl::ThrottlingObserverHandleImpl(
    FrameSchedulerImpl* frame_scheduler,
    Observer* observer)
    : frame_scheduler_(frame_scheduler->GetWeakPtr()), observer_(observer) {}

FrameSchedulerImpl::ThrottlingObserverHandleImpl::
    ~ThrottlingObserverHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->RemoveThrottlingObserver(observer_);
}

FrameSchedulerImpl::FrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    PageSchedulerImpl* parent_page_scheduler,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type)
    : frame_type_(frame_type),
      renderer_scheduler_(renderer_scheduler),
      parent_page_scheduler_(parent_page_scheduler),
      blame_context_(blame_context),
      throttling_state_(FrameScheduler::ThrottlingState::kNotThrottled),
      frame_visible_(true,
                     "FrameScheduler.FrameVisible",
                     this,
                     &tracing_controller_,
                     VisibilityStateToString),
      page_visibility_(kDefaultPageVisibility,
                       "FrameScheduler.PageVisibility",
                       this,
                       &tracing_controller_,
                       PageVisibilityStateToString),
      page_frozen_(false,
                   "FrameScheduler.PageFrozen",
                   this,
                   &tracing_controller_,
                   FrozenStateToString),
      keep_active_(renderer_scheduler->SchedulerKeepActive(),
                   "FrameScheduler.KeepActive",
                   this,
                   &tracing_controller_,
                   KeepActiveStateToString),
      frame_paused_(false,
                    "FrameScheduler.FramePaused",
                    this,
                    &tracing_controller_,
                    PausedStateToString),
      frame_origin_type_(frame_type == FrameType::kMainFrame
                             ? FrameOriginType::kMainFrame
                             : FrameOriginType::kSameOriginFrame,
                         "FrameScheduler.Origin",
                         this,
                         &tracing_controller_,
                         FrameOriginTypeToString),
      url_tracer_("FrameScheduler.URL", this),
      task_queue_throttled_(false,
                            "FrameScheduler.TaskQueueThrottled",
                            this,
                            &tracing_controller_,
                            YesNoStateToString),
      active_connection_count_(0),
      has_active_connection_(false,
                             "FrameScheduler.HasActiveConnection",
                             this,
                             &tracing_controller_,
                             YesNoStateToString),
      weak_factory_(this) {
  DCHECK_EQ(throttling_state_, CalculateThrottlingState());
}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  if (!queue)
    return;
  queue->DetachFromRendererScheduler();
  queue->SetFrameScheduler(nullptr);
  queue->SetBlameContext(nullptr);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
}

}  // namespace

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  CleanUpQueue(loading_task_queue_.get());
  CleanUpQueue(loading_control_task_queue_.get());
  CleanUpQueue(throttleable_task_queue_.get());
  CleanUpQueue(deferrable_task_queue_.get());
  CleanUpQueue(pausable_task_queue_.get());
  CleanUpQueue(unpausable_task_queue_.get());

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (has_active_connection())
      parent_page_scheduler_->OnConnectionUpdated();
  }
}

void FrameSchedulerImpl::DetachFromPageScheduler() {
  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::
    RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool() {
  if (!throttleable_task_queue_)
    return;

  if (!parent_page_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_page_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                throttleable_task_queue_.get());
}

std::unique_ptr<FrameScheduler::ThrottlingObserverHandle>
FrameSchedulerImpl::AddThrottlingObserver(ObserverType type,
                                          Observer* observer) {
  DCHECK(observer);
  observer->OnThrottlingStateChanged(CalculateThrottlingState());
  loader_observers_.insert(observer);
  return std::make_unique<ThrottlingObserverHandleImpl>(this, observer);
}

void FrameSchedulerImpl::RemoveThrottlingObserver(Observer* observer) {
  DCHECK(observer);
  const auto found = loader_observers_.find(observer);
  DCHECK(loader_observers_.end() != found);
  loader_observers_.erase(found);
}

void FrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  DCHECK(parent_page_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  UMA_HISTOGRAM_BOOLEAN("RendererScheduler.IPC.FrameVisibility", frame_visible);
  frame_visible_ = frame_visible;
  UpdateTaskQueueThrottling();
}

bool FrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void FrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }
  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginFrame;
  }
  UpdateTaskQueueThrottling();
}

bool FrameSchedulerImpl::IsCrossOrigin() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginFrame;
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::kJavascriptTimer:
      return TaskRunnerImpl::Create(ThrottleableTaskQueue(), type);
    case TaskType::kUnspecedLoading:
    case TaskType::kNetworking:
      return TaskRunnerImpl::Create(LoadingTaskQueue(), type);
    case TaskType::kNetworkingControl:
      return TaskRunnerImpl::Create(LoadingControlTaskQueue(), type);
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDatabaseAccess:
    case TaskType::kDOMManipulation:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kMicrotask:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kUnspecedTimer:
    case TaskType::kMiscPlatformAPI:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return TaskRunnerImpl::Create(DeferrableTaskQueue(), type);
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
      return TaskRunnerImpl::Create(PausableTaskQueue(), type);
    case TaskType::kUnthrottled:
    case TaskType::kInternalTest:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIPC:
      return TaskRunnerImpl::Create(UnpausableTaskQueue(), type);
    case TaskType::kDeprecatedNone:
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::LoadingTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!loading_task_queue_) {
    // TODO(panicker): Avoid adding this queue in RS task_runners_.
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoading);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_task_queue_->SetFrameScheduler(this);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::LoadingControlTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!loading_control_task_queue_) {
    loading_control_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoadingControl);
    loading_control_task_queue_->SetBlameContext(blame_context_);
    loading_control_task_queue_->SetFrameScheduler(this);
    loading_control_queue_enabled_voter_ =
        loading_control_task_queue_->CreateQueueEnabledVoter();
    loading_control_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_control_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::ThrottleableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!throttleable_task_queue_) {
    // TODO(panicker): Avoid adding this queue in RS task_runners_.
    throttleable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetCanBeThrottled(true)
            .SetCanBeStopped(true)
            .SetFreezeWhenKeepActive(true)
            .SetCanBeDeferred(true)
            .SetCanBePaused(true));
    throttleable_task_queue_->SetBlameContext(blame_context_);
    throttleable_task_queue_->SetFrameScheduler(this);
    throttleable_queue_enabled_voter_ =
        throttleable_task_queue_->CreateQueueEnabledVoter();
    throttleable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);

    CPUTimeBudgetPool* time_budget_pool =
        parent_page_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                 throttleable_task_queue_.get());
    }
    UpdateTaskQueueThrottling();
  }
  return throttleable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::DeferrableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!deferrable_task_queue_) {
    deferrable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameDeferrable)
            .SetCanBeDeferred(true)
            .SetCanBeStopped(
                RuntimeEnabledFeatures::StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true));
    deferrable_task_queue_->SetBlameContext(blame_context_);
    deferrable_task_queue_->SetFrameScheduler(this);
    deferrable_queue_enabled_voter_ =
        deferrable_task_queue_->CreateQueueEnabledVoter();
    deferrable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return deferrable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::PausableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!pausable_task_queue_) {
    pausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFramePausable)
            .SetCanBeStopped(
                RuntimeEnabledFeatures::StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true));
    pausable_task_queue_->SetBlameContext(blame_context_);
    pausable_task_queue_->SetFrameScheduler(this);
    pausable_queue_enabled_voter_ =
        pausable_task_queue_->CreateQueueEnabledVoter();
    pausable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return pausable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::UnpausableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!unpausable_task_queue_) {
    unpausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameUnpausable));
    unpausable_task_queue_->SetBlameContext(blame_context_);
    unpausable_task_queue_->SetFrameScheduler(this);
  }
  return unpausable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::ControlTaskQueue() {
  DCHECK(parent_page_scheduler_);
  return renderer_scheduler_->ControlTaskQueue();
}

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  renderer_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  renderer_scheduler_->DidCommitProvisionalLoad(is_web_history_inert_commit,
                                                is_reload, is_main_frame);
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(renderer_scheduler_, duration);
}

void FrameSchedulerImpl::DidOpenActiveConnection() {
  ++active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::DidCloseActiveConnection() {
  DCHECK_GT(active_connection_count_, 0);
  --active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible",
                    page_visibility_ == PageVisibilityState::kVisible);
  state->SetBoolean("cross_origin", IsCrossOrigin());
  state->SetString("frame_type",
                   frame_type_ == FrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     PointerToString(loading_task_queue_.get()));
  }
  if (loading_control_task_queue_) {
    state->SetString("loading_control_task_queue",
                     PointerToString(loading_control_task_queue_.get()));
  }
  if (throttleable_task_queue_) {
    state->SetString("throttleable_task_queue",
                     PointerToString(throttleable_task_queue_.get()));
  }
  if (deferrable_task_queue_) {
    state->SetString("deferrable_task_queue",
                     PointerToString(deferrable_task_queue_.get()));
  }
  if (pausable_task_queue_) {
    state->SetString("pausable_task_queue",
                     PointerToString(pausable_task_queue_.get()));
  }
  if (unpausable_task_queue_) {
    state->SetString("unpausable_task_queue",
                     PointerToString(unpausable_task_queue_.get()));
  }
  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
  }
}

void FrameSchedulerImpl::SetPageVisibility(
    PageVisibilityState page_visibility) {
  DCHECK(parent_page_scheduler_);
  if (page_visibility_ == page_visibility)
    return;
  page_visibility_ = page_visibility;
  if (page_visibility_ == PageVisibilityState::kVisible)
    page_frozen_ = false;  // visible page must not be frozen.
  // TODO(altimin): Avoid having to call all these methods here.
  UpdateTaskQueues();
  UpdateTaskQueueThrottling();
  UpdateThrottlingState();
}

bool FrameSchedulerImpl::IsPageVisible() const {
  return page_visibility_ == PageVisibilityState::kVisible;
}

void FrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_page_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  UpdateTaskQueues();
}

void FrameSchedulerImpl::SetPageFrozen(bool frozen) {
  DCHECK(page_visibility_ == PageVisibilityState::kHidden);
  page_frozen_ = frozen;
  UpdateTaskQueues();
  UpdateThrottlingState();
}

void FrameSchedulerImpl::SetKeepActive(bool keep_active) {
  keep_active_ = keep_active;
  UpdateTaskQueues();
}

void FrameSchedulerImpl::UpdateTaskQueues() {
  // Per-frame (stoppable) task queues will be stopped after 5mins in
  // background. They will be resumed when the page is visible.
  UpdateTaskQueue(throttleable_task_queue_,
                  throttleable_queue_enabled_voter_.get());
  UpdateTaskQueue(loading_task_queue_, loading_queue_enabled_voter_.get());
  UpdateTaskQueue(loading_control_task_queue_,
                  loading_control_queue_enabled_voter_.get());
  UpdateTaskQueue(deferrable_task_queue_,
                  deferrable_queue_enabled_voter_.get());
  UpdateTaskQueue(pausable_task_queue_, pausable_queue_enabled_voter_.get());
}

void FrameSchedulerImpl::UpdateTaskQueue(
    const scoped_refptr<MainThreadTaskQueue>& queue,
    TaskQueue::QueueEnabledVoter* voter) {
  if (!queue || !voter)
    return;
  bool queue_paused = frame_paused_ && queue->CanBePaused();
  bool queue_frozen = page_frozen_ && queue->CanBeStopped();
  // Override freezing if keep-active is true.
  if (queue_frozen && !queue->FreezeWhenKeepActive())
    queue_frozen = !keep_active_;
  voter->SetQueueEnabled(!queue_paused && !queue_frozen);
}

void FrameSchedulerImpl::UpdateThrottlingState() {
  FrameScheduler::ThrottlingState throttling_state = CalculateThrottlingState();
  if (throttling_state == throttling_state_)
    return;
  throttling_state_ = throttling_state;
  for (auto observer : loader_observers_)
    observer->OnThrottlingStateChanged(throttling_state_);
}

FrameScheduler::ThrottlingState FrameSchedulerImpl::CalculateThrottlingState()
    const {
  if (RuntimeEnabledFeatures::StopLoadingInBackgroundEnabled() &&
      page_frozen_) {
    DCHECK(page_visibility_ == PageVisibilityState::kHidden);
    return FrameScheduler::ThrottlingState::kStopped;
  }
  if (page_visibility_ == PageVisibilityState::kHidden)
    return FrameScheduler::ThrottlingState::kThrottled;
  return FrameScheduler::ThrottlingState::kNotThrottled;
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint() {
  renderer_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<FrameScheduler::ActiveConnectionHandle>
FrameSchedulerImpl::OnActiveConnectionCreated() {
  return std::make_unique<FrameSchedulerImpl::ActiveConnectionHandleImpl>(this);
}

bool FrameSchedulerImpl::ShouldThrottleTimers() const {
  if (page_visibility_ == PageVisibilityState::kHidden)
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && IsCrossOrigin();
}

void FrameSchedulerImpl::UpdateTaskQueueThrottling() {
  // Before we initialize a trottleable task queue, |task_queue_throttled_|
  // stays false and this function ensures it indicates whether are we holding
  // a queue reference for throttler or not.
  // Don't modify that value neither amend the reference counter anywhere else.
  if (!throttleable_task_queue_)
    return;
  bool should_throttle = ShouldThrottleTimers();
  if (task_queue_throttled_ == should_throttle)
    return;
  task_queue_throttled_ = should_throttle;

  if (should_throttle) {
    renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        throttleable_task_queue_.get());
  } else {
    renderer_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        throttleable_task_queue_.get());
  }
}

base::WeakPtr<FrameSchedulerImpl> FrameSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return has_active_connection();
}

}  // namespace scheduler
}  // namespace blink
