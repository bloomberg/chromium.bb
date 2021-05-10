// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/time_domain.h"
#include "net/base/request_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}
}  // namespace base

namespace blink {
namespace scheduler {

using TaskQueue = base::sequence_manager::TaskQueue;

namespace main_thread_scheduler_impl_unittest {
class MainThreadSchedulerImplTest;
}

namespace agent_interference_recorder_test {
class AgentInterferenceRecorderTest;
}

namespace task_queue_throttler_unittest {
class TaskQueueThrottlerTest;
}

class FrameSchedulerImpl;
class MainThreadSchedulerImpl;

// TODO(kdillon): Remove ref-counting of MainThreadTaskQueues as it's no longer
// needed.
class PLATFORM_EXPORT MainThreadTaskQueue
    : public base::RefCountedThreadSafe<MainThreadTaskQueue> {
 public:
  enum class QueueType {
    // Keep MainThreadTaskQueue::NameForQueueType in sync.
    // This enum is used for a histogram and it should not be re-numbered.
    // TODO(altimin): Clean up obsolete names and use a new histogram when
    // the situation settles.
    kControl = 0,
    kDefault = 1,

    // 2 was used for default loading task runner but this was deprecated.

    // 3 was used for default timer task runner but this was deprecated.

    // 4: kUnthrottled, obsolete.

    kFrameLoading = 5,
    // 6 : kFrameThrottleable, replaced with FRAME_THROTTLEABLE.
    // 7 : kFramePausable, replaced with kFramePausable
    kCompositor = 8,
    kIdle = 9,
    kTest = 10,
    kFrameLoadingControl = 11,
    kFrameThrottleable = 12,
    kFrameDeferrable = 13,
    kFramePausable = 14,
    kFrameUnpausable = 15,
    kV8 = 16,
    // 17 : kIPC, obsolete
    kInput = 18,

    // Detached is used in histograms for tasks which are run after frame
    // is detached and task queue is gracefully shutdown.
    // TODO(altimin): Move to the top when histogram is renumbered.
    kDetached = 19,

    // 20 : kCleanup, obsolete.
    // 21 : kWebSchedulingUserInteraction, obsolete.
    // 22 : kWebSchedulingBestEffort, obsolete.

    kWebScheduling = 24,
    kNonWaking = 25,

    kIPCTrackingForCachedPages = 26,

    // Used to group multiple types when calculating Expected Queueing Time.
    kOther = 23,
    kCount = 27
  };

  // The ThrottleHandle controls throttling and unthrottling the queue. When
  // a caller requests a queue to be throttled, this handle is returned and
  // the queue will remain throttled as long as the handle is alive.
  class ThrottleHandle {
   public:
    ThrottleHandle(base::WeakPtr<TaskQueue> task_queue,
                   base::WeakPtr<TaskQueueThrottler> throttler)
        : task_queue_(std::move(task_queue)), throttler_(std::move(throttler)) {
      if (task_queue_ && throttler_)
        throttler_->IncreaseThrottleRefCount(task_queue_.get());
    }
    ~ThrottleHandle() {
      if (task_queue_ && throttler_)
        throttler_->DecreaseThrottleRefCount(task_queue_.get());
    }

    // Move-only.
    ThrottleHandle(ThrottleHandle&& other)
        : task_queue_(std::move(other.task_queue_)),
          throttler_(std::move(other.throttler_)) {
      other.task_queue_ = nullptr;
      other.throttler_ = nullptr;
    }
    ThrottleHandle& operator=(ThrottleHandle&&);

   private:
    base::WeakPtr<TaskQueue> task_queue_;
    base::WeakPtr<TaskQueueThrottler> throttler_;
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // Returns true if task queues of the given queue type can be created on a
  // per-frame basis, and false if they are only created on a shared basis for
  // the entire main thread.
  static bool IsPerFrameTaskQueue(QueueType);

  using QueueTraitsKeyType = int;

  // QueueTraits represent the deferrable, throttleable, pausable, and freezable
  // properties of a MainThreadTaskQueue. For non-loading task queues, there
  // will be at most one task queue with a specific set of QueueTraits, and the
  // the QueueTraits determine which queues should be used to run which task
  // types.
  struct QueueTraits {
    QueueTraits()
        : can_be_deferred(false),
          can_be_throttled(false),
          can_be_intensively_throttled(false),
          can_be_paused(false),
          can_be_frozen(false),
          can_run_in_background(true),
          can_run_when_virtual_time_paused(true),
          can_be_paused_for_android_webview(false) {}

    // Separate enum class for handling prioritisation decisions in task queues.
    enum class PrioritisationType {
      kInternalScriptContinuation = 0,
      kBestEffort = 1,
      kRegular = 2,
      kLoading = 3,
      kLoadingControl = 4,
      kFindInPage = 5,
      kExperimentalDatabase = 6,
      kJavaScriptTimer = 7,
      kHighPriorityLocalFrame = 8,
      kCompositor = 9,  // Main-thread only.
      kInput = 10,

      kCount = 11
    };

    // kPrioritisationTypeWidthBits is the number of bits required
    // for PrioritisationType::kCount - 1, which is the number of bits needed
    // to represent |prioritisation_type| in QueueTraitKeyType.
    // We need to update it whenever there is a change in
    // PrioritisationType::kCount.
    // TODO(sreejakshetty) make the number of bits calculation automated.
    static constexpr int kPrioritisationTypeWidthBits = 4;
    static_assert(static_cast<int>(PrioritisationType::kCount) <=
                      (1 << kPrioritisationTypeWidthBits),
                  "Wrong Instanstiation for kPrioritisationTypeWidthBits");

    QueueTraits(const QueueTraits&) = default;

    QueueTraits SetCanBeDeferred(bool value) {
      can_be_deferred = value;
      return *this;
    }

    QueueTraits SetCanBeThrottled(bool value) {
      can_be_throttled = value;
      return *this;
    }

    QueueTraits SetCanBeIntensivelyThrottled(bool value) {
      can_be_intensively_throttled = value;
      return *this;
    }

    QueueTraits SetCanBePaused(bool value) {
      can_be_paused = value;
      return *this;
    }

    QueueTraits SetCanBeFrozen(bool value) {
      can_be_frozen = value;
      return *this;
    }

    QueueTraits SetCanRunInBackground(bool value) {
      can_run_in_background = value;
      return *this;
    }

    QueueTraits SetCanRunWhenVirtualTimePaused(bool value) {
      can_run_when_virtual_time_paused = value;
      return *this;
    }

    QueueTraits SetPrioritisationType(PrioritisationType type) {
      prioritisation_type = type;
      return *this;
    }

    QueueTraits SetCanBePausedForAndroidWebview(bool value) {
      can_be_paused_for_android_webview = value;
      return *this;
    }

    bool operator==(const QueueTraits& other) const {
      return can_be_deferred == other.can_be_deferred &&
             can_be_throttled == other.can_be_throttled &&
             can_be_intensively_throttled ==
                 other.can_be_intensively_throttled &&
             can_be_paused == other.can_be_paused &&
             can_be_frozen == other.can_be_frozen &&
             can_run_in_background == other.can_run_in_background &&
             can_run_when_virtual_time_paused ==
                 other.can_run_when_virtual_time_paused &&
             prioritisation_type == other.prioritisation_type &&
             can_be_paused_for_android_webview ==
                 other.can_be_paused_for_android_webview;
    }

    // Return a key suitable for WTF::HashMap.
    QueueTraitsKeyType Key() const {
      // offset for shifting bits to compute |key|.
      // |key| starts at 1 since 0 and -1 are used for empty/deleted values.
      int offset = 0;
      int key = 1 << (offset++);
      key |= can_be_deferred << (offset++);
      key |= can_be_throttled << (offset++);
      key |= can_be_intensively_throttled << (offset++);
      key |= can_be_paused << (offset++);
      key |= can_be_frozen << (offset++);
      key |= can_run_in_background << (offset++);
      key |= can_run_when_virtual_time_paused << (offset++);
      key |= can_be_paused_for_android_webview << (offset++);
      key |= static_cast<int>(prioritisation_type) << offset;
      offset += kPrioritisationTypeWidthBits;
      return key;
    }

    void WriteIntoTracedValue(perfetto::TracedValue context) const;

    bool can_be_deferred : 1;
    bool can_be_throttled : 1;
    bool can_be_intensively_throttled : 1;
    bool can_be_paused : 1;
    bool can_be_frozen : 1;
    bool can_run_in_background : 1;
    bool can_run_when_virtual_time_paused : 1;
    bool can_be_paused_for_android_webview : 1;
    PrioritisationType prioritisation_type = PrioritisationType::kRegular;
  };

  struct QueueCreationParams {
    explicit QueueCreationParams(QueueType queue_type)
        : queue_type(queue_type),
          spec(NameForQueueType(queue_type)),
          agent_group_scheduler(nullptr),
          frame_scheduler(nullptr),
          freeze_when_keep_active(false) {}

    QueueCreationParams SetFreezeWhenKeepActive(bool value) {
      freeze_when_keep_active = value;
      return *this;
    }

    QueueCreationParams SetWebSchedulingPriority(
        base::Optional<WebSchedulingPriority> priority) {
      web_scheduling_priority = priority;
      return *this;
    }

    QueueCreationParams SetAgentGroupScheduler(
        AgentGroupSchedulerImpl* scheduler) {
      agent_group_scheduler = scheduler;
      return *this;
    }

    QueueCreationParams SetFrameScheduler(FrameSchedulerImpl* scheduler) {
      frame_scheduler = scheduler;
      return *this;
    }

    // Forwarded calls to |queue_traits|

    QueueCreationParams SetCanBeDeferred(bool value) {
      queue_traits = queue_traits.SetCanBeDeferred(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBeThrottled(bool value) {
      queue_traits = queue_traits.SetCanBeThrottled(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBePaused(bool value) {
      queue_traits = queue_traits.SetCanBePaused(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBeFrozen(bool value) {
      queue_traits = queue_traits.SetCanBeFrozen(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanRunInBackground(bool value) {
      queue_traits = queue_traits.SetCanRunInBackground(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanRunWhenVirtualTimePaused(bool value) {
      queue_traits = queue_traits.SetCanRunWhenVirtualTimePaused(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetPrioritisationType(
        QueueTraits::PrioritisationType type) {
      queue_traits = queue_traits.SetPrioritisationType(type);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetQueueTraits(QueueTraits value) {
      queue_traits = value;
      ApplyQueueTraitsToSpec();
      return *this;
    }

    // Forwarded calls to |spec|.

    QueueCreationParams SetShouldMonitorQuiescence(bool should_monitor) {
      spec = spec.SetShouldMonitorQuiescence(should_monitor);
      return *this;
    }

    QueueCreationParams SetShouldNotifyObservers(bool run_observers) {
      spec = spec.SetShouldNotifyObservers(run_observers);
      return *this;
    }

    QueueCreationParams SetTimeDomain(
        base::sequence_manager::TimeDomain* domain) {
      spec = spec.SetTimeDomain(domain);
      return *this;
    }

    QueueType queue_type;
    TaskQueue::Spec spec;
    AgentGroupSchedulerImpl* agent_group_scheduler;
    FrameSchedulerImpl* frame_scheduler;
    QueueTraits queue_traits;
    bool freeze_when_keep_active;
    base::Optional<WebSchedulingPriority> web_scheduling_priority;

   private:
    void ApplyQueueTraitsToSpec() {
      spec = spec.SetDelayedFencesAllowed(queue_traits.can_be_throttled);
    }
  };

  QueueType queue_type() const { return queue_type_; }

  bool CanBeDeferred() const { return queue_traits_.can_be_deferred; }

  bool CanBeThrottled() const { return queue_traits_.can_be_throttled; }

  bool CanBeIntensivelyThrottled() const {
    return queue_traits_.can_be_intensively_throttled;
  }

  bool CanBePaused() const { return queue_traits_.can_be_paused; }

  // Used for WebView's pauseTimers API. This API expects layout, parsing, and
  // Javascript timers to be paused. Though this suggests we should pause
  // loading (where parsing happens) as well, there are some expectations of JS
  // still being able to run during pause. Because of this we only pause timers
  // as well as any other pausable frame task queue.
  // https://developer.android.com/reference/android/webkit/WebView#pauseTimers()
  bool CanBePausedForAndroidWebview() const {
    return queue_traits_.can_be_paused_for_android_webview;
  }

  bool CanBeFrozen() const { return queue_traits_.can_be_frozen; }

  bool CanRunInBackground() const {
    return queue_traits_.can_run_in_background;
  }

  bool CanRunWhenVirtualTimePaused() const {
    return queue_traits_.can_run_when_virtual_time_paused;
  }

  bool FreezeWhenKeepActive() const { return freeze_when_keep_active_; }

  QueueTraits GetQueueTraits() const { return queue_traits_; }

  QueueTraits::PrioritisationType GetPrioritisationType() const {
    return queue_traits_.prioritisation_type;
  }

  void OnTaskReady(const void* frame_scheduler,
                   const base::sequence_manager::Task& task,
                   base::sequence_manager::LazyNow* lazy_now);

  void OnTaskStarted(const base::sequence_manager::Task& task,
                     const TaskQueue::TaskTiming& task_timing);

  void OnTaskCompleted(const base::sequence_manager::Task& task,
                       TaskQueue::TaskTiming* task_timing,
                       base::sequence_manager::LazyNow* lazy_now);

  void SetOnIPCTaskPosted(
      base::RepeatingCallback<void(const base::sequence_manager::Task&)>
          on_ipc_task_posted_callback);
  void DetachOnIPCTaskPostedWhileInBackForwardCache();

  void DetachFromMainThreadScheduler();

  void ShutdownTaskQueue();

  WebAgentGroupScheduler* GetAgentGroupScheduler();

  FrameSchedulerImpl* GetFrameScheduler() const;

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) {
    return task_queue_->CreateTaskRunner(static_cast<int>(task_type));
  }

  void SetNetRequestPriority(net::RequestPriority net_request_priority);
  base::Optional<net::RequestPriority> net_request_priority() const;

  void SetWebSchedulingPriority(WebSchedulingPriority priority);
  base::Optional<WebSchedulingPriority> web_scheduling_priority() const;

  // TODO(kdillon): Improve MTTQ API surface so that we no longer
  // need to expose the raw pointer to the queue.
  TaskQueue* GetTaskQueue() { return task_queue_.get(); }

  // This method returns the default task runner with task type kTaskTypeNone
  // and is mostly used for tests. For most use cases, you'll want a more
  // specific task runner and should use the 'CreateTaskRunner' method and pass
  // the desired task type.
  const scoped_refptr<base::SingleThreadTaskRunner>&
  GetTaskRunnerWithDefaultTaskType() {
    return task_queue_->task_runner();
  }

  bool IsThrottled() const;

  // Throttles the task queue as long as the handle is kept alive.
  MainThreadTaskQueue::ThrottleHandle Throttle();

  // Called when a task finished running to update cpu-based throttling.
  void OnTaskRunTimeReported(TaskQueue::TaskTiming* task_timing);

  // Methods for setting and resetting budget pools for this task queue.
  // Note that a task queue can be in multiple budget pools so a pool must
  // be specified when resetting.
  void AddToBudgetPool(base::TimeTicks now, BudgetPool* pool);
  void RemoveFromBudgetPool(base::TimeTicks now, BudgetPool* pool);

  // This method is only used for tests. If this queue is throttled it will
  // notify the throttler that this queue should wake immediately.
  void SetImmediateWakeUpForTest();

  base::WeakPtr<MainThreadTaskQueue> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void WriteIntoTracedValue(perfetto::TracedValue context) const;

 protected:
  void SetFrameSchedulerForTest(FrameSchedulerImpl* frame_scheduler);

  // Returns the underlying task queue. Only to be used for tests that need to
  // test functionality of the task queue specifically without the wrapping
  // MainThreadTaskQueue (ex TaskQueueThrottlerTest).
  TaskQueue* GetTaskQueueForTest() { return task_queue_.get(); }

  // TODO(kdillon): Remove references to TaskQueueImpl once TaskQueueImpl
  // inherits from TaskQueue.
  MainThreadTaskQueue(
      std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
      const TaskQueue::Spec& spec,
      const QueueCreationParams& params,
      MainThreadSchedulerImpl* main_thread_scheduler);

  ~MainThreadTaskQueue();

 private:
  friend class base::RefCountedThreadSafe<MainThreadTaskQueue>;
  friend class base::sequence_manager::SequenceManager;
  friend class blink::scheduler::main_thread_scheduler_impl_unittest::
      MainThreadSchedulerImplTest;
  friend class agent_interference_recorder_test::AgentInterferenceRecorderTest;
  friend class blink::scheduler::task_queue_throttler_unittest::
      TaskQueueThrottlerTest;

  // Clear references to main thread scheduler and frame scheduler and dispatch
  // appropriate notifications. This is the common part of ShutdownTaskQueue and
  // DetachFromMainThreadScheduler.
  void ClearReferencesToSchedulers();

  scoped_refptr<TaskQueue> task_queue_;

  const QueueType queue_type_;
  const QueueTraits queue_traits_;
  const bool freeze_when_keep_active_;

  // Warning: net_request_priority is not the same as the priority of the queue.
  // It is the priority (at the loading stack level) of the resource associated
  // to the queue, if one exists.
  //
  // Used to track UMA metrics for resource loading tasks split by net priority.
  base::Optional<net::RequestPriority> net_request_priority_;

  // |web_scheduling_priority_| is the priority of the task queue within the web
  // scheduling API. This priority is used in conjunction with the frame
  // scheduling policy to determine the task queue priority.
  base::Optional<WebSchedulingPriority> web_scheduling_priority_;

  // Needed to notify renderer scheduler about completed tasks.
  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED

  AgentGroupSchedulerImpl* agent_group_scheduler_{nullptr};  // NOT OWNED

  // Set in the constructor. Cleared in ClearReferencesToSchedulers(). Can never
  // be set to a different value afterwards (except in tests).
  FrameSchedulerImpl* frame_scheduler_;  // NOT OWNED

  base::WeakPtrFactory<MainThreadTaskQueue> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
