// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace scheduler {

namespace {

class DummyFrameScheduler : public FrameScheduler {
 public:
  DummyFrameScheduler() : page_scheduler_(CreateDummyPageScheduler()) {}
  ~DummyFrameScheduler() override = default;

  DummyFrameScheduler(const DummyFrameScheduler&) = delete;
  DummyFrameScheduler& operator=(const DummyFrameScheduler&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  PageScheduler* GetPageScheduler() const override {
    return page_scheduler_.get();
  }
  WebAgentGroupScheduler* GetAgentGroupScheduler() override {
    return &page_scheduler_->GetAgentGroupScheduler();
  }

  void SetPreemptedForCooperativeScheduling(Preempted) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return true; }
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetShouldReportPostedTasksWhenDisabled(bool) override {}
  void SetCrossOriginToMainFrame(bool) override {}
  bool IsCrossOriginToMainFrame() const override { return false; }
  void SetIsAdFrame(bool is_ad_frame) override {}
  bool IsAdFrame() const override { return false; }
  void TraceUrlChange(const String&) override {}
  void AddTaskTime(base::TimeDelta) override {}
  FrameType GetFrameType() const override { return FrameType::kMainFrame; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad(bool is_main_frame) override {}
  void DidCommitProvisionalLoad(bool, FrameScheduler::NavigationType) override {
  }
  void OnFirstContentfulPaintInMainFrame() override {}
  void OnFirstMeaningfulPaint() override {}
  void OnLoad() override {}
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override {
    return nullptr;
  }
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override {
    return WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
        base::ThreadTaskRunnerHandle::Get());
  }
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingMaybeUnfreezableTaskRunnerHandle() override {
    return WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
        base::ThreadTaskRunnerHandle::Get());
  }
  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingPriority) override {
    return nullptr;
  }
  ukm::SourceId GetUkmSourceId() override { return ukm::kInvalidSourceId; }
  void OnStartedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override {}
  void OnStoppedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override {}
  WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics() override {
    return WTF::HashSet<SchedulingPolicy::Feature>();
  }
  base::WeakPtr<FrameScheduler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void ReportActiveSchedulerTrackedFeatures() override {}

 private:
  std::unique_ptr<PageScheduler> page_scheduler_;
  base::WeakPtrFactory<FrameScheduler> weak_ptr_factory_{this};
};

class DummyPageScheduler : public PageScheduler {
 public:
  DummyPageScheduler()
      : agent_group_scheduler_(CreateDummyAgentGroupScheduler()) {}
  ~DummyPageScheduler() override = default;

  DummyPageScheduler(const DummyPageScheduler&) = delete;
  DummyPageScheduler& operator=(const DummyPageScheduler&) = delete;

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) override {
    return CreateDummyFrameScheduler();
  }

  void OnTitleOrFaviconUpdated() override {}
  void SetPageVisible(bool) override {}
  void SetPageFrozen(bool) override {}
  void SetPageBackForwardCached(bool) override {}
  void OnFocusChanged(bool focused) override {}
  void SetKeepActive(bool) override {}
  bool IsMainFrameLocal() const override { return true; }
  void SetIsMainFrameLocal(bool) override {}
  void OnLocalMainFrameNetworkAlmostIdle() override {}
  base::TimeTicks EnableVirtualTime() override { return base::TimeTicks(); }
  void DisableVirtualTimeForTesting() override {}
  bool VirtualTimeAllowedToAdvance() const override { return true; }
  void SetInitialVirtualTime(base::Time) override {}
  void SetInitialVirtualTimeOffset(base::TimeDelta) override {}
  void SetVirtualTimePolicy(VirtualTimePolicy) override {}
  void GrantVirtualTimeBudget(base::TimeDelta, base::OnceClosure) override {}
  void SetMaxVirtualTimeTaskStarvationCount(int) override {}
  void AudioStateChanged(bool is_audio_playing) override {}
  bool IsAudioPlaying() const override { return false; }
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  bool OptedOutFromAggressiveThrottlingForTest() const override {
    return false;
  }
  bool IsInBackForwardCache() const override { return false; }
  bool RequestBeginMainFrameNotExpected(bool) override { return false; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  WebAgentGroupScheduler& GetAgentGroupScheduler() override {
    return *agent_group_scheduler_;
  }

 private:
  std::unique_ptr<WebAgentGroupScheduler> agent_group_scheduler_;
};

class DummyAgentGroupScheduler : public AgentGroupScheduler {
 public:
  DummyAgentGroupScheduler()
      : main_thread_scheduler_(CreateDummyWebThreadScheduler()) {}
  ~DummyAgentGroupScheduler() override = default;

  DummyAgentGroupScheduler(const DummyAgentGroupScheduler&) = delete;
  DummyAgentGroupScheduler& operator=(const DummyAgentGroupScheduler&) = delete;

  AgentGroupScheduler& AsAgentGroupScheduler() override { return *this; }
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return CreateDummyPageScheduler();
  }
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }
  WebThreadScheduler& GetMainThreadScheduler() override {
    return *main_thread_scheduler_;
  }
  void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote_broker)
      override {}
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override {
    return GetEmptyBrowserInterfaceBroker();
  }

 private:
  std::unique_ptr<WebThreadScheduler> main_thread_scheduler_;
};

// TODO(altimin,yutak): Merge with SimpleThread in platform.cc.
class SimpleThread : public Thread {
 public:
  explicit SimpleThread(ThreadScheduler* scheduler) : scheduler_(scheduler) {}
  ~SimpleThread() override {}

  SimpleThread(const SimpleThread&) = delete;
  SimpleThread& operator=(const SimpleThread&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  ThreadScheduler* Scheduler() override { return scheduler_; }

  bool IsCurrentThread() const { return WTF::IsMainThread(); }

 private:
  ThreadScheduler* scheduler_;
};

class DummyThreadScheduler : public ThreadScheduler {
 public:
  DummyThreadScheduler() {}
  ~DummyThreadScheduler() override {}

  void Shutdown() override {}

  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override {
    return CreateDummyAgentGroupScheduler();
  }

  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override {
    return nullptr;
  }

  // ThreadScheduler implementation:
  bool ShouldYieldForHighPriorityWork() override { return false; }
  bool CanExceedIdleDeadlineIfRequired() const override { return false; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta delay,
                           Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  void AddRAILModeObserver(RAILModeObserver*) override {}
  void RemoveRAILModeObserver(RAILModeObserver const*) override {}
  std::unique_ptr<WebThreadScheduler::RendererPauseHandle> PauseScheduler()
      override {
    return nullptr;
  }
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks::Now();
  }
  void AddTaskObserver(base::TaskObserver*) override {}
  void RemoveTaskObserver(base::TaskObserver*) override {}
  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }
  void SetV8Isolate(v8::Isolate* isolate) override {}
};

class DummyWebThreadScheduler : public WebThreadScheduler,
                                public DummyThreadScheduler {
 public:
  DummyWebThreadScheduler() {}
  ~DummyWebThreadScheduler() override {}

  // WebThreadScheduler implementation:
  void Shutdown() override {}

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<Thread> CreateMainThread() override {
    return std::make_unique<SimpleThread>(this);
  }

  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override {
    return CreateDummyAgentGroupScheduler();
  }

  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override {
    return nullptr;
  }
};

}  // namespace

std::unique_ptr<FrameScheduler> CreateDummyFrameScheduler() {
  return std::make_unique<DummyFrameScheduler>();
}

std::unique_ptr<PageScheduler> CreateDummyPageScheduler() {
  return std::make_unique<DummyPageScheduler>();
}

std::unique_ptr<AgentGroupScheduler> CreateDummyAgentGroupScheduler() {
  return std::make_unique<DummyAgentGroupScheduler>();
}

std::unique_ptr<ThreadScheduler> CreateDummyThreadScheduler() {
  return std::make_unique<DummyThreadScheduler>();
}

std::unique_ptr<WebThreadScheduler> CreateDummyWebThreadScheduler() {
  return std::make_unique<DummyWebThreadScheduler>();
}

}  // namespace scheduler
}  // namespace blink
