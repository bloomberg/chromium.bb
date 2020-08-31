// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/blame_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/web_scheduling_task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

namespace blink {

namespace scheduler {

using base::sequence_manager::TaskQueue;
using QueueTraits = MainThreadTaskQueue::QueueTraits;

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

// Used to update the priority of task_queue. Note that this function is
// used for queues associated with a frame.
void UpdatePriority(MainThreadTaskQueue* task_queue) {
  if (!task_queue)
    return;

  FrameSchedulerImpl* frame_scheduler = task_queue->GetFrameScheduler();
  DCHECK(frame_scheduler);
  task_queue->SetQueuePriority(frame_scheduler->ComputePriority(task_queue));
}

}  // namespace

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    PauseSubresourceLoadingHandleImpl(
        base::WeakPtr<FrameSchedulerImpl> frame_scheduler)
    : frame_scheduler_(std::move(frame_scheduler)) {
  DCHECK(frame_scheduler_);
  frame_scheduler_->AddPauseSubresourceLoadingHandle();
}

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    ~PauseSubresourceLoadingHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->RemovePauseSubresourceLoadingHandle();
}

std::unique_ptr<FrameSchedulerImpl> FrameSchedulerImpl::Create(
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type) {
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler(new FrameSchedulerImpl(
      parent_page_scheduler->GetMainThreadScheduler(), parent_page_scheduler,
      delegate, blame_context, frame_type));
  parent_page_scheduler->RegisterFrameSchedulerImpl(frame_scheduler.get());
  return frame_scheduler;
}

FrameSchedulerImpl::FrameSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type)
    : frame_type_(frame_type),
      is_ad_frame_(false),
      main_thread_scheduler_(main_thread_scheduler),
      parent_page_scheduler_(parent_page_scheduler),
      delegate_(delegate),
      blame_context_(blame_context),
      throttling_state_(SchedulingLifecycleState::kNotThrottled),
      frame_visible_(true,
                     "FrameScheduler.FrameVisible",
                     this,
                     &tracing_controller_,
                     VisibilityStateToString),
      frame_paused_(false,
                    "FrameScheduler.FramePaused",
                    this,
                    &tracing_controller_,
                    PausedStateToString),
      frame_origin_type_(frame_type == FrameType::kMainFrame
                             ? FrameOriginType::kMainFrame
                             : FrameOriginType::kSameOriginToMainFrame,
                         "FrameScheduler.Origin",
                         this,
                         &tracing_controller_,
                         FrameOriginTypeToString),
      subresource_loading_paused_(false,
                                  "FrameScheduler.SubResourceLoadingPaused",
                                  this,
                                  &tracing_controller_,
                                  PausedStateToString),
      url_tracer_("FrameScheduler.URL", this),
      task_queues_throttled_(false,
                             "FrameScheduler.TaskQueuesThrottled",
                             this,
                             &tracing_controller_,
                             YesNoStateToString),
      preempted_for_cooperative_scheduling_(
          false,
          "FrameScheduler.PreemptedForCooperativeScheduling",
          this,
          &tracing_controller_,
          YesNoStateToString),
      aggressive_throttling_opt_out_count(0),
      opted_out_from_aggressive_throttling_(
          false,
          "FrameScheduler.AggressiveThrottlingDisabled",
          this,
          &tracing_controller_,
          YesNoStateToString),
      subresource_loading_pause_count_(0u),
      opted_out_from_back_forward_cache_(
          false,
          "FrameScheduler.OptedOutFromBackForwardCache",
          this,
          &tracing_controller_,
          YesNoStateToString),
      page_frozen_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->IsFrozen() : true,
          "FrameScheduler.PageFrozen",
          this,
          &tracing_controller_,
          FrozenStateToString),
      page_visibility_for_tracing_(
          parent_page_scheduler_ && parent_page_scheduler_->IsPageVisible()
              ? PageVisibilityState::kVisible
              : PageVisibilityState::kHidden,
          "FrameScheduler.PageVisibility",
          this,
          &tracing_controller_,
          PageVisibilityStateToString),
      page_keep_active_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->KeepActive() : false,
          "FrameScheduler.KeepActive",
          this,
          &tracing_controller_,
          KeepActiveStateToString),
      waiting_for_contentful_paint_(true,
                                    "FrameScheduler.WaitingForContentfulPaint",
                                    this,
                                    &tracing_controller_,
                                    YesNoStateToString),
      waiting_for_meaningful_paint_(true,
                                    "FrameScheduler.WaitingForMeaningfulPaint",
                                    this,
                                    &tracing_controller_,
                                    YesNoStateToString) {
  frame_task_queue_controller_.reset(
      new FrameTaskQueueController(main_thread_scheduler_, this, this));
}

FrameSchedulerImpl::FrameSchedulerImpl()
    : FrameSchedulerImpl(nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         FrameType::kSubframe) {}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  DCHECK(queue);

  queue->DetachFromMainThreadScheduler();
  DCHECK(!queue->GetFrameScheduler());
  queue->SetBlameContext(nullptr);
}

}  // namespace

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
          task_queue_and_voter.first);
    }
    CleanUpQueue(task_queue_and_voter.first);
  }

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (opted_out_from_aggressive_throttling())
      parent_page_scheduler_->OnAggressiveThrottlingStatusUpdated();
  }

  // Can be null in tests.
  if (main_thread_scheduler_)
    main_thread_scheduler_->OnFrameSchedulerDestroyed(this);
}

void FrameSchedulerImpl::DetachFromPageScheduler() {
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
          task_queue_and_voter.first);
    }
  }

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
    MainThreadTaskQueue* task_queue) {
  DCHECK(task_queue);
  DCHECK(task_queue->CanBeThrottled());

  if (!parent_page_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_page_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  // On tests, the scheduler helper might already be shut down and tick is not
  // available.
  base::TimeTicks now;
  if (main_thread_scheduler_->tick_clock())
    now = main_thread_scheduler_->tick_clock()->NowTicks();
  else
    now = base::TimeTicks::Now();
  time_budget_pool->RemoveQueue(now, task_queue);
}

void FrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  DCHECK(parent_page_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  UMA_HISTOGRAM_BOOLEAN("RendererScheduler.IPC.FrameVisibility", frame_visible);
  frame_visible_ = frame_visible;
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void FrameSchedulerImpl::SetCrossOriginToMainFrame(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }
  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginToMainFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginToMainFrame;
  }
  UpdatePolicy();
}

void FrameSchedulerImpl::SetIsAdFrame() {
  is_ad_frame_ = true;
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsAdFrame() const {
  return is_ad_frame_;
}

bool FrameSchedulerImpl::IsCrossOriginToMainFrame() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginToMainFrame;
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

void FrameSchedulerImpl::AddTaskTime(base::TimeDelta time) {
  // The duration of task time under which AddTaskTime buffers rather than
  // sending the task time update to the delegate.
  constexpr base::TimeDelta kTaskDurationSendThreshold =
      base::TimeDelta::FromMilliseconds(100);
  if (!delegate_)
    return;
  task_time_ += time;
  if (task_time_ >= kTaskDurationSendThreshold) {
    delegate_->UpdateTaskTime(task_time_);
    task_time_ = base::TimeDelta();
  }
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

// static
QueueTraits FrameSchedulerImpl::CreateQueueTraitsForTaskType(TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  // TODO(sreejakshetty): Clean up the PrioritisationType QueueTrait and
  // QueueType for kInternalContinueScriptLoading and kInternalContentCapture.
  switch (type) {
    case TaskType::kInternalContentCapture:
      return ThrottleableTaskQueueTraits().SetPrioritisationType(
            QueueTraits::PrioritisationType::kBestEffort);
    case TaskType::kJavascriptTimer:
      return ThrottleableTaskQueueTraits();
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
      return LoadingTaskQueueTraits();
    case TaskType::kNetworkingControl:
      return LoadingControlTaskQueueTraits();
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
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
    case TaskType::kInternalDefault:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kFontLoading:
    case TaskType::kApplicationLifeCycle:
    case TaskType::kBackgroundFetch:
    case TaskType::kPermission:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return DeferrableTaskQueueTraits();
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    case TaskType::kServiceWorkerClientMessage:
    case TaskType::kWorkerAnimation:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
      return PausableTaskQueueTraits();
    case TaskType::kInternalFindInPage:
      return FindInPageTaskQueueTraits();
    case TaskType::kInternalContinueScriptLoading:
      return PausableTaskQueueTraits().SetPrioritisationType(
            QueueTraits::PrioritisationType::kVeryHigh);
    case TaskType::kDatabaseAccess:
      if (base::FeatureList::IsEnabled(kHighPriorityDatabaseTaskType)) {
        return PausableTaskQueueTraits().SetPrioritisationType(
            QueueTraits::PrioritisationType::kExperimentalDatabase);
      } else {
        return PausableTaskQueueTraits();
      }
    case TaskType::kInternalNavigationAssociated:
      return FreezableTaskQueueTraits();
    // Some tasks in the tests need to run when objects are paused e.g. to hook
    // when recovering from debugger JavaScript statetment.
    case TaskType::kInternalTest:
    // kWebLocks can be frozen if for entire page, but not for individual
    // frames. See https://crrev.com/c/1687716
    case TaskType::kWebLocks:
    case TaskType::kInternalFrameLifecycleControl:
      return UnpausableTaskQueueTraits();
    case TaskType::kInternalTranslation:
      return ForegroundOnlyTaskQueueTraits();
    // The TaskType of Inspector tasks need to be unpausable and should not use
    // virtual time because they need to run on a paused page or when virtual
    // time is paused.
    case TaskType::kInternalInspector:
    // Navigation IPCs do not run using virtual time to avoid hanging.
    case TaskType::kInternalNavigationAssociatedUnfreezable:
      return DoesNotUseVirtualTimeTaskQueueTraits();
    case TaskType::kDeprecatedNone:
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueIPC:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueCleanup:
    case TaskType::kMainThreadTaskQueueMemoryPurge:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueNonWaking:
    // The web scheduling API task types are used by WebSchedulingTaskQueues.
    // The associated TaskRunner should be obtained by creating a
    // WebSchedulingTaskQueue with CreateWebSchedulingTaskQueue().
    case TaskType::kExperimentalWebScheduling:
    case TaskType::kCount:
      // Not a valid frame-level TaskType.
      NOTREACHED();
      return QueueTraits();
  }
  // This method is called for all values between 0 and kCount. TaskType,
  // however, has numbering gaps, so even though all enumerated TaskTypes are
  // handled in the switch and return a value, we fall through for some values
  // of |type|.
  NOTREACHED();
  return QueueTraits();
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(type);
  DCHECK(task_queue);
  return task_queue->CreateTaskRunner(type);
}

scoped_refptr<MainThreadTaskQueue> FrameSchedulerImpl::GetTaskQueue(
    TaskType type) {
  QueueTraits queue_traits = CreateQueueTraitsForTaskType(type);
  return frame_task_queue_controller_->GetTaskQueue(queue_traits);
}

std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandle() {
  return CreateResourceLoadingTaskRunnerHandleImpl();
}

std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandleImpl() {
  if (main_thread_scheduler_->scheduling_settings()
          .use_resource_fetch_priority ||
      (parent_page_scheduler_ && parent_page_scheduler_->IsLoading() &&
       main_thread_scheduler_->scheduling_settings()
           .use_resource_priorities_only_during_loading)) {
    scoped_refptr<MainThreadTaskQueue> task_queue =
        frame_task_queue_controller_->NewResourceLoadingTaskQueue();
    resource_loading_task_queue_priorities_.insert(
        task_queue, task_queue->GetQueuePriority());
    return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(task_queue);
  }

  return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(GetTaskQueue
      (TaskType::kNetworkingWithURLLoaderAnnotation));
}

void FrameSchedulerImpl::DidChangeResourceLoadingPriority(
    scoped_refptr<MainThreadTaskQueue> task_queue,
    net::RequestPriority priority) {
  // This check is done since in some cases (when kUseResourceFetchPriority
  // feature isn't enabled) we use the loading task queue for resource loading
  // and the priority of this queue shouldn't be affected by resource
  // priorities.
  auto queue_priority_pair =
      resource_loading_task_queue_priorities_.find(task_queue);
  if (queue_priority_pair != resource_loading_task_queue_priorities_.end()) {
    task_queue->SetNetRequestPriority(priority);
    queue_priority_pair->value = main_thread_scheduler_->scheduling_settings()
                                     .net_to_blink_priority[priority];
    auto* voter =
        frame_task_queue_controller_->GetQueueEnabledVoter(task_queue);
    UpdateQueuePolicy(task_queue.get(), voter);
  }
}

void FrameSchedulerImpl::OnShutdownResourceLoadingTaskQueue(
    scoped_refptr<MainThreadTaskQueue> task_queue) {
  // This check is done since in some cases (when kUseResourceFetchPriority
  // feature isn't enabled) we use the loading task queue for resource loading,
  // and the lifetime of this queue isn't bound to one resource.
  auto iter = resource_loading_task_queue_priorities_.find(task_queue);
  if (iter != resource_loading_task_queue_priorities_.end()) {
    resource_loading_task_queue_priorities_.erase(iter);
    bool removed = frame_task_queue_controller_->RemoveResourceLoadingTaskQueue(
        task_queue);
    DCHECK(removed);
    CleanUpQueue(task_queue.get());
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
FrameSchedulerImpl::ControlTaskRunner() {
  DCHECK(parent_page_scheduler_);
  return main_thread_scheduler_->ControlTaskRunner();
}

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  main_thread_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    NavigationType navigation_type) {
  bool is_main_frame = GetFrameType() == FrameType::kMainFrame;
  if (is_main_frame && navigation_type != NavigationType::kSameDocument)
    task_time_ = base::TimeDelta();
  main_thread_scheduler_->DidCommitProvisionalLoad(
      is_web_history_inert_commit, navigation_type == NavigationType::kReload,
      is_main_frame);
  if (navigation_type != NavigationType::kSameDocument)
    ResetForNavigation();
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

void FrameSchedulerImpl::ResetForNavigation() {
  document_bound_weak_factory_.InvalidateWeakPtrs();

  for (const auto& it : back_forward_cache_opt_out_counts_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(it.first)));
  }

  back_forward_cache_opt_out_counts_.clear();
  back_forward_cache_opt_outs_.reset();
  last_uploaded_active_features_ = 0;
}

void FrameSchedulerImpl::OnStartedUsingFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy) {
  uint64_t old_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (policy.disable_aggressive_throttling)
    OnAddedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache) {
    OnAddedBackForwardCacheOptOut(feature);
  }

  uint64_t new_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (old_mask != new_mask) {
    NotifyDelegateAboutFeaturesAfterCurrentTask();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(feature)),
        "feature", FeatureToString(feature));
  }
}

void FrameSchedulerImpl::OnStoppedUsingFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy) {
  uint64_t old_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (policy.disable_aggressive_throttling)
    OnRemovedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache)
    OnRemovedBackForwardCacheOptOut(feature);

  uint64_t new_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (old_mask != new_mask) {
    NotifyDelegateAboutFeaturesAfterCurrentTask();
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(feature)));
  }
}

void FrameSchedulerImpl::NotifyDelegateAboutFeaturesAfterCurrentTask() {
  if (!delegate_)
    return;
  if (feature_report_scheduled_)
    return;
  feature_report_scheduled_ = true;

  main_thread_scheduler_->ExecuteAfterCurrentTask(
      base::BindOnce(&FrameSchedulerImpl::ReportFeaturesToDelegate,
                     document_bound_weak_factory_.GetWeakPtr()));
}

void FrameSchedulerImpl::ReportFeaturesToDelegate() {
  DCHECK(delegate_);
  feature_report_scheduled_ = false;
  uint64_t mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();
  if (mask == last_uploaded_active_features_)
    return;
  last_uploaded_active_features_ = mask;
  delegate_->UpdateActiveSchedulerTrackedFeatures(mask);
}

base::WeakPtr<FrameScheduler> FrameSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FrameSchedulerImpl::OnAddedAggressiveThrottlingOptOut() {
  ++aggressive_throttling_opt_out_count;
  opted_out_from_aggressive_throttling_ =
      static_cast<bool>(aggressive_throttling_opt_out_count);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnAggressiveThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnRemovedAggressiveThrottlingOptOut() {
  DCHECK_GT(aggressive_throttling_opt_out_count, 0);
  --aggressive_throttling_opt_out_count;
  opted_out_from_aggressive_throttling_ =
      static_cast<bool>(aggressive_throttling_opt_out_count);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnAggressiveThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnAddedBackForwardCacheOptOut(
    SchedulingPolicy::Feature feature) {
  ++back_forward_cache_opt_out_counts_[feature];
  back_forward_cache_opt_outs_.set(static_cast<size_t>(feature));
  opted_out_from_back_forward_cache_ = true;
}

void FrameSchedulerImpl::OnRemovedBackForwardCacheOptOut(
    SchedulingPolicy::Feature feature) {
  DCHECK_GT(back_forward_cache_opt_out_counts_[feature], 0);
  auto it = back_forward_cache_opt_out_counts_.find(feature);
  if (it->second == 1) {
    back_forward_cache_opt_out_counts_.erase(it);
    back_forward_cache_opt_outs_.reset(static_cast<size_t>(feature));
  } else {
    --it->second;
  }
  opted_out_from_back_forward_cache_ =
      !back_forward_cache_opt_out_counts_.empty();
}

void FrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", parent_page_scheduler_->IsPageVisible());
  state->SetBoolean("cross_origin_to_main_frame", IsCrossOriginToMainFrame());
  state->SetString("frame_type",
                   frame_type_ == FrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  state->SetBoolean(
      "disable_background_timer_throttling",
      !RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled());

  state->BeginDictionary("frame_task_queue_controller");
  frame_task_queue_controller_->AsValueInto(state);
  state->EndDictionary();

  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
  }
}

void FrameSchedulerImpl::SetPageVisibilityForTracing(
    PageVisibilityState page_visibility) {
  page_visibility_for_tracing_ = page_visibility;
}

bool FrameSchedulerImpl::IsPageVisible() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsPageVisible()
                                : true;
}

bool FrameSchedulerImpl::IsAudioPlaying() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsAudioPlaying()
                                : false;
}

void FrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_page_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  UpdatePolicy();
}

void FrameSchedulerImpl::SetShouldReportPostedTasksWhenDisabled(
    bool should_report) {
  // Forward this to all the task queues associated with this frame.
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    auto* task_queue = task_queue_and_voter.first;
    if (task_queue->CanBeFrozen())
      task_queue->SetShouldReportPostedTasksWhenDisabled(should_report);
  }
}

void FrameSchedulerImpl::SetPageFrozenForTracing(bool frozen) {
  page_frozen_for_tracing_ = frozen;
}

void FrameSchedulerImpl::SetPageKeepActiveForTracing(bool keep_active) {
  page_keep_active_for_tracing_ = keep_active;
}

void FrameSchedulerImpl::UpdatePolicy() {
  bool task_queues_were_throttled = task_queues_throttled_;
  task_queues_throttled_ = ShouldThrottleTaskQueues();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    UpdateQueuePolicy(task_queue_and_voter.first, task_queue_and_voter.second);
    if (task_queues_were_throttled != task_queues_throttled_) {
      UpdateTaskQueueThrottling(task_queue_and_voter.first,
                                task_queues_throttled_);
    }
  }

  NotifyLifecycleObservers();
}

void FrameSchedulerImpl::UpdateQueuePolicy(
    MainThreadTaskQueue* queue,
    TaskQueue::QueueEnabledVoter* voter) {
  DCHECK(queue);
  UpdatePriority(queue);
  if (!voter)
    return;
  DCHECK(parent_page_scheduler_);
  bool queue_disabled = false;
  queue_disabled |= frame_paused_ && queue->CanBePaused();
  queue_disabled |= preempted_for_cooperative_scheduling_;
  // Per-frame freezable task queues will be frozen after 5 mins in background
  // on Android, and if the browser freezes the page in the background. They
  // will be resumed when the page is visible.
  bool queue_frozen =
      parent_page_scheduler_->IsFrozen() && queue->CanBeFrozen();
  // Override freezing if keep-active is true.
  if (queue_frozen && !queue->FreezeWhenKeepActive())
    queue_frozen = !parent_page_scheduler_->KeepActive();
  queue_disabled |= queue_frozen;
  // Per-frame freezable queues of tasks which are specified as getting frozen
  // immediately when their frame becomes invisible get frozen. They will be
  // resumed when the frame becomes visible again.
  queue_disabled |= !frame_visible_ && !queue->CanRunInBackground();
  voter->SetVoteToEnable(!queue_disabled);
}

SchedulingLifecycleState FrameSchedulerImpl::CalculateLifecycleState(
    ObserverType type) const {
  // Detached frames are not throttled.
  if (!parent_page_scheduler_)
    return SchedulingLifecycleState::kNotThrottled;

  if (parent_page_scheduler_->IsFrozen() &&
      !parent_page_scheduler_->KeepActive()) {
    DCHECK(!parent_page_scheduler_->IsPageVisible());
    return SchedulingLifecycleState::kStopped;
  }
  if (subresource_loading_paused_ && type == ObserverType::kLoader)
    return SchedulingLifecycleState::kStopped;
  if (type == ObserverType::kLoader &&
      parent_page_scheduler_->OptedOutFromAggressiveThrottling()) {
    return SchedulingLifecycleState::kNotThrottled;
  }
  if (parent_page_scheduler_->IsThrottled())
    return SchedulingLifecycleState::kThrottled;
  if (!parent_page_scheduler_->IsPageVisible())
    return SchedulingLifecycleState::kHidden;
  return SchedulingLifecycleState::kNotThrottled;
}

void FrameSchedulerImpl::OnFirstContentfulPaint() {
  waiting_for_contentful_paint_ = false;
  if (GetFrameType() == FrameScheduler::FrameType::kMainFrame)
    main_thread_scheduler_->OnMainFramePaint();
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint() {
  waiting_for_meaningful_paint_ = false;
  if (GetFrameType() == FrameScheduler::FrameType::kMainFrame)
    main_thread_scheduler_->OnMainFramePaint();
}

bool FrameSchedulerImpl::IsWaitingForContentfulPaint() const {
  return waiting_for_contentful_paint_;
}

bool FrameSchedulerImpl::IsWaitingForMeaningfulPaint() const {
  return waiting_for_meaningful_paint_;
}

bool FrameSchedulerImpl::ShouldThrottleTaskQueues() const {
  // TODO(crbug.com/1078387): Convert the CHECK to a DCHECK once enough time has
  // passed to confirm that it is correct. (November 2020).
  CHECK(parent_page_scheduler_);

  if (!RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled())
    return false;
  if (parent_page_scheduler_->IsAudioPlaying())
    return false;
  if (!parent_page_scheduler_->IsPageVisible())
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && IsCrossOriginToMainFrame();
}

void FrameSchedulerImpl::UpdateTaskQueueThrottling(
    MainThreadTaskQueue* task_queue,
    bool should_throttle) {
  if (!task_queue->CanBeThrottled())
    return;
  if (should_throttle) {
    main_thread_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        task_queue);
  } else {
    main_thread_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        task_queue);
  }
}

bool FrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return opted_out_from_aggressive_throttling();
}

TaskQueue::QueuePriority FrameSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  FrameScheduler* frame_scheduler = task_queue->GetFrameScheduler();

  // Checks the task queue is associated with this frame scheduler.
  DCHECK_EQ(frame_scheduler, this);

  auto queue_priority_pair = resource_loading_task_queue_priorities_.find(
      base::WrapRefCounted(task_queue));
  if (queue_priority_pair != resource_loading_task_queue_priorities_.end())
    return queue_priority_pair->value;

  base::Optional<TaskQueue::QueuePriority> fixed_priority =
      task_queue->FixedPriority();

  if (fixed_priority)
    return fixed_priority.value();

  // TODO(shaseley): This should use lower priorities if the frame is
  // deprioritized. Change this once we refactor and add frame policy/priorities
  // and add a range of new priorities less than low.
  if (task_queue->web_scheduling_priority()) {
    switch (task_queue->web_scheduling_priority().value()) {
      case WebSchedulingPriority::kUserBlockingPriority:
        return TaskQueue::QueuePriority::kHighPriority;
      case WebSchedulingPriority::kUserVisiblePriority:
        return TaskQueue::QueuePriority::kNormalPriority;
      case WebSchedulingPriority::kBackgroundPriority:
        return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  if (!parent_page_scheduler_) {
    // Frame might be detached during its shutdown. Return a default priority
    // in that case.
    return TaskQueue::QueuePriority::kNormalPriority;
  }

  // A hidden page with no audio.
  if (parent_page_scheduler_->IsBackgrounded()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_background_page) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    if (main_thread_scheduler_->scheduling_settings()
            .best_effort_background_page) {
      return TaskQueue::QueuePriority::kBestEffortPriority;
    }
  }

  // If the page is loading or if the priority experiments should take place at
  // all times.
  if (parent_page_scheduler_->IsLoading() ||
      !main_thread_scheduler_->scheduling_settings()
           .use_frame_priorities_only_during_loading) {
    // Low priority feature enabled for hidden frame.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_hidden_frame &&
        !IsFrameVisible()) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    bool is_subframe = GetFrameType() == FrameScheduler::FrameType::kSubframe;
    bool is_throttleable_task_queue =
        task_queue->queue_type() ==
        MainThreadTaskQueue::QueueType::kFrameThrottleable;

    // Low priority feature enabled for sub-frame.
    if (main_thread_scheduler_->scheduling_settings().low_priority_subframe &&
        is_subframe) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    // Low priority feature enabled for sub-frame throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_subframe_throttleable &&
        is_subframe && is_throttleable_task_queue) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    // Low priority feature enabled for throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_throttleable &&
        is_throttleable_task_queue) {
      return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  // Ad frame experiment.
  if (IsAdFrame() && (parent_page_scheduler_->IsLoading() ||
                      !main_thread_scheduler_->scheduling_settings()
                           .use_adframe_priorities_only_during_loading)) {
    if (main_thread_scheduler_->scheduling_settings().low_priority_ad_frame) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    if (main_thread_scheduler_->scheduling_settings().best_effort_ad_frame) {
      return TaskQueue::QueuePriority::kBestEffortPriority;
    }
  }

  // Frame origin type experiment.
  if (IsCrossOriginToMainFrame()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_cross_origin ||
        (main_thread_scheduler_->scheduling_settings()
             .low_priority_cross_origin_only_during_loading &&
         parent_page_scheduler_->IsLoading())) {
      return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kLoadingControl) {
    return main_thread_scheduler_
                   ->should_prioritize_loading_with_compositing()
               ? TaskQueue::QueuePriority::kVeryHighPriority
               : TaskQueue::QueuePriority::kHighPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kLoading &&
      main_thread_scheduler_->should_prioritize_loading_with_compositing()) {
    return main_thread_scheduler_->compositor_priority();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kFindInPage) {
    return main_thread_scheduler_->find_in_page_priority();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::
          kExperimentalDatabase) {
    // TODO(shaseley): This decision should probably be based on Agent
    // visibility. Consider changing this before shipping anything.
    return parent_page_scheduler_->IsPageVisible()
               ? TaskQueue::QueuePriority::kHighPriority
               : TaskQueue::QueuePriority::kNormalPriority;
  }

  return TaskQueue::QueuePriority::kNormalPriority;
}

std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
FrameSchedulerImpl::GetPauseSubresourceLoadingHandle() {
  return std::make_unique<PauseSubresourceLoadingHandleImpl>(
      weak_factory_.GetWeakPtr());
}

void FrameSchedulerImpl::AddPauseSubresourceLoadingHandle() {
  ++subresource_loading_pause_count_;
  if (subresource_loading_pause_count_ != 1) {
    DCHECK(subresource_loading_paused_);
    return;
  }

  DCHECK(!subresource_loading_paused_);
  subresource_loading_paused_ = true;
  UpdatePolicy();
}

void FrameSchedulerImpl::RemovePauseSubresourceLoadingHandle() {
  DCHECK_LT(0u, subresource_loading_pause_count_);
  --subresource_loading_pause_count_;
  DCHECK(subresource_loading_paused_);
  if (subresource_loading_pause_count_ == 0) {
    subresource_loading_paused_ = false;
    UpdatePolicy();
  }
}

ukm::UkmRecorder* FrameSchedulerImpl::GetUkmRecorder() {
  if (!delegate_)
    return nullptr;
  return delegate_->GetUkmRecorder();
}

ukm::SourceId FrameSchedulerImpl::GetUkmSourceId() {
  if (!delegate_)
    return ukm::kInvalidSourceId;
  return delegate_->GetUkmSourceId();
}

void FrameSchedulerImpl::OnTaskQueueCreated(
    MainThreadTaskQueue* task_queue,
    base::sequence_manager::TaskQueue::QueueEnabledVoter* voter) {
  DCHECK(parent_page_scheduler_);

  task_queue->SetBlameContext(blame_context_);
  UpdateQueuePolicy(task_queue, voter);

  if (task_queue->CanBeThrottled()) {
    CPUTimeBudgetPool* time_budget_pool =
        parent_page_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(
          main_thread_scheduler_->tick_clock()->NowTicks(), task_queue);
    }
    if (task_queues_throttled_) {
      UpdateTaskQueueThrottling(task_queue, true);
    }
  }
}

WTF::HashSet<SchedulingPolicy::Feature>
FrameSchedulerImpl::GetActiveFeaturesTrackedForBackForwardCacheMetrics() {
  WTF::HashSet<SchedulingPolicy::Feature> result;
  for (const auto& it : back_forward_cache_opt_out_counts_)
    result.insert(it.first);
  return result;
}

uint64_t
FrameSchedulerImpl::GetActiveFeaturesTrackedForBackForwardCacheMetricsMask()
    const {
  auto result = back_forward_cache_opt_outs_.to_ullong();
  static_assert(static_cast<size_t>(SchedulingPolicy::Feature::kMaxValue) <
                    sizeof(result) * 8,
                "Number of the features should allow a bitmask to fit into "
                "64-bit integer");
  return result;
}

base::WeakPtr<FrameOrWorkerScheduler>
FrameSchedulerImpl::GetDocumentBoundWeakPtr() {
  return document_bound_weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebSchedulingTaskQueue>
FrameSchedulerImpl::CreateWebSchedulingTaskQueue(
    WebSchedulingPriority priority) {
  // Use QueueTraits here that are the same as postMessage, which is one current
  // method for scheduling script.
  scoped_refptr<MainThreadTaskQueue> task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          PausableTaskQueueTraits(), priority);
  return std::make_unique<WebSchedulingTaskQueueImpl>(task_queue->AsWeakPtr());
}

void FrameSchedulerImpl::OnWebSchedulingTaskQueuePriorityChanged(
    MainThreadTaskQueue* queue) {
  UpdateQueuePolicy(queue,
                    frame_task_queue_controller_->GetQueueEnabledVoter(queue));
}

const base::UnguessableToken& FrameSchedulerImpl::GetAgentClusterId() const {
  if (!delegate_)
    return base::UnguessableToken::Null();
  return delegate_->GetAgentClusterId();
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ThrottleableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeThrottled(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::DeferrableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeDeferred(true)
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::PausableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::FreezableTaskQueueTraits() {
  // Should not use VirtualTime because using VirtualTime would make the task
  // execution non-deterministic and produce timeouts failures.
  return QueueTraits().SetCanBeFrozen(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::UnpausableTaskQueueTraits() {
  return QueueTraits().SetCanRunWhenVirtualTimePaused(false);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ForegroundOnlyTaskQueueTraits() {
  return ThrottleableTaskQueueTraits()
      .SetCanRunInBackground(false)
      .SetCanRunWhenVirtualTimePaused(false);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::DoesNotUseVirtualTimeTaskQueueTraits() {
  return QueueTraits().SetCanRunWhenVirtualTimePaused(true);
}

void FrameSchedulerImpl::SetPreemptedForCooperativeScheduling(
    Preempted preempted) {
  DCHECK_NE(preempted.value(), preempted_for_cooperative_scheduling_);
  preempted_for_cooperative_scheduling_ = preempted.value();
  UpdatePolicy();
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::LoadingTaskQueueTraits() {
  return QueueTraits()
      .SetCanBePaused(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetPrioritisationType(
          QueueTraits::PrioritisationType::kLoading);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::LoadingControlTaskQueueTraits() {
  return QueueTraits()
      .SetCanBePaused(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetPrioritisationType(
          QueueTraits::PrioritisationType::kLoadingControl);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::FindInPageTaskQueueTraits() {
  return PausableTaskQueueTraits().SetPrioritisationType(
      QueueTraits::PrioritisationType::kFindInPage);
}
}  // namespace scheduler
}  // namespace blink
