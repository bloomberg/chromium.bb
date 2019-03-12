// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/scheduler/test/web_mock_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace scheduler {

namespace {

// TODO(altimin,yutak): Merge with SimpleThread in platform.cc.
class SimpleThread : public Thread {
 public:
  explicit SimpleThread(ThreadScheduler* scheduler) : scheduler_(scheduler) {}
  ~SimpleThread() override {}

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  ThreadScheduler* Scheduler() override { return scheduler_; }

  void Init() override {}
  bool IsCurrentThread() const { return WTF::IsMainThread(); }

 private:
  ThreadScheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(SimpleThread);
};

class SimpleFrameScheduler : public FrameScheduler {
 public:
  explicit SimpleFrameScheduler(PageScheduler* page_scheduler)
      : page_scheduler_(page_scheduler) {}
  ~SimpleFrameScheduler() override {}

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  PageScheduler* GetPageScheduler() const override { return page_scheduler_; }

  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return true; }
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetCrossOrigin(bool) override {}
  bool IsCrossOrigin() const override { return false; }
  void SetIsAdFrame() override {}
  bool IsAdFrame() const override { return false; }
  void TraceUrlChange(const String&) override {}
  FrameType GetFrameType() const override { return FrameType::kMainFrame; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad(bool is_main_frame) override {}
  void DidCommitProvisionalLoad(bool, FrameScheduler::NavigationType) override {
  }
  void OnFirstMeaningfulPaint() override {}
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override {
    return nullptr;
  }
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override {
    return nullptr;
  }
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override {
    return WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
        base::ThreadTaskRunnerHandle::Get());
  }
  ukm::SourceId GetUkmSourceId() override { return ukm::kInvalidSourceId; }

 private:
  PageScheduler* page_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFrameScheduler);
};

class SimplePageScheduler : public PageScheduler {
 public:
  SimplePageScheduler() {}
  ~SimplePageScheduler() override {}

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) override {
    return std::make_unique<SimpleFrameScheduler>(this);
  }

  void SetPageVisible(bool) override {}
  void SetPageFrozen(bool) override {}
  void SetKeepActive(bool) override {}
  bool IsMainFrameLocal() const override { return true; }
  void SetIsMainFrameLocal(bool) override {}
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
  bool HasActiveConnectionForTest() const override { return false; }
  bool RequestBeginMainFrameNotExpected(bool) override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimplePageScheduler);
};

class SimpleThreadScheduler : public ThreadScheduler {
 public:
  SimpleThreadScheduler() {}
  ~SimpleThreadScheduler() override {}

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

  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return std::make_unique<SimplePageScheduler>();
  }

  // ThreadScheduler implementation:
  bool ShouldYieldForHighPriorityWork() override { return false; }
  bool CanExceedIdleDeadlineIfRequired() const override { return false; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  void AddRAILModeObserver(WebRAILModeObserver*) override {}
  std::unique_ptr<WebThreadScheduler::RendererPauseHandle> PauseScheduler()
      override {
    return nullptr;
  }
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks::Now();
  }
  void AddTaskObserver(base::MessageLoop::TaskObserver*) override {}
  void RemoveTaskObserver(base::MessageLoop::TaskObserver*) override {}
  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }
};

class SimpleMainThreadScheduler : public WebThreadScheduler,
                                  public SimpleThreadScheduler {
 public:
  SimpleMainThreadScheduler() {}
  ~SimpleMainThreadScheduler() override {}

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

  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
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

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return std::make_unique<SimplePageScheduler>();
  }
};

class SimpleMockMainThreadScheduler : public WebMockThreadScheduler {
 public:
  SimpleMockMainThreadScheduler() {}
  ~SimpleMockMainThreadScheduler() override {}

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<Thread> CreateMainThread() override {
    return std::make_unique<SimpleThread>(&simple_thread_scheduler_);
  }

 private:
  SimpleThreadScheduler simple_thread_scheduler_;
};

}  // namespace

std::unique_ptr<WebThreadScheduler> CreateWebMainThreadSchedulerForTests() {
  return std::make_unique<SimpleMainThreadScheduler>();
}

std::unique_ptr<WebMockThreadScheduler>
CreateMockWebMainThreadSchedulerForTests() {
  return std::make_unique<SimpleMockMainThreadScheduler>();
}

void RunIdleTasksForTesting(WebThreadScheduler* scheduler,
                            base::OnceClosure callback) {
  MainThreadSchedulerImpl* scheduler_impl =
      static_cast<MainThreadSchedulerImpl*>(scheduler);
  scheduler_impl->RunIdleTasksForTesting(std::move(callback));
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunnerForTesting() {
  return base::SequencedTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
GetSingleThreadTaskRunnerForTesting() {
  return base::ThreadTaskRunnerHandle::Get();
}

}  // namespace scheduler
}  // namespace blink
