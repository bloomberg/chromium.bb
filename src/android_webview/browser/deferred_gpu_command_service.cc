// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/deferred_gpu_command_service.h"

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/render_thread_manager.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "ui/gl/gl_share_group.h"

namespace android_webview {

base::LazyInstance<base::ThreadLocalBoolean>::DestructorAtExit
    ScopedAllowGL::allow_gl;

// static
bool ScopedAllowGL::IsAllowed() {
  return allow_gl.Get().Get();
}

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(!allow_gl.Get().Get());
  allow_gl.Get().Set(true);

  DeferredGpuCommandService* service = DeferredGpuCommandService::GetInstance();
  DCHECK(service);
  service->RunTasks();
}

ScopedAllowGL::~ScopedAllowGL() {
  allow_gl.Get().Set(false);

  DeferredGpuCommandService* service = DeferredGpuCommandService::GetInstance();
  DCHECK(service);
  service->RunTasks();
  if (service->IdleQueueSize()) {
    service->RequestProcessGL(true);
  }
}

// gpu::CommandBufferTaskExectuor::Sequence implementation that encapsulates a
// SyncPointOrderData, and posts tasks to the task executors global task queue.
class TaskForwardingSequence : public gpu::CommandBufferTaskExecutor::Sequence {
 public:
  explicit TaskForwardingSequence(DeferredGpuCommandService* service)
      : gpu::CommandBufferTaskExecutor::Sequence(),
        service_(service),
        sync_point_order_data_(
            service->sync_point_manager()->CreateSyncPointOrderData()),
        weak_ptr_factory_(this) {}
  ~TaskForwardingSequence() override { sync_point_order_data_->Destroy(); }

  // CommandBufferTaskExecutor::Sequence implementation.
  gpu::SequenceId GetSequenceId() override {
    return sync_point_order_data_->sequence_id();
  }

  bool ShouldYield() override { return false; }

  // Should not be called because BlockThreadOnWaitSyncToken() returns true,
  // and the client should not disable sequences to wait for sync tokens.
  void SetEnabled(bool enabled) override { NOTREACHED(); }

  void ScheduleTask(base::OnceClosure task,
                    std::vector<gpu::SyncToken> sync_token_fences) override {
    uint32_t order_num =
        sync_point_order_data_->GenerateUnprocessedOrderNumber();
    // Use a weak ptr because the task executor holds the tasks, and the
    // sequence will be destroyed before the task executor.
    service_->ScheduleTask(
        base::BindOnce(&TaskForwardingSequence::RunTask,
                       weak_ptr_factory_.GetWeakPtr(), std::move(task),
                       std::move(sync_token_fences), order_num),
        false /* out_of_order */);
  }

  // Should not be called because tasks aren't reposted to wait for sync tokens,
  // or for yielding execution since ShouldYield() returns false.
  void ContinueTask(base::OnceClosure task) override { NOTREACHED(); }

 private:
  // Method to wrap scheduled task with the order number processing required for
  // sync tokens.
  void RunTask(base::OnceClosure task,
               std::vector<gpu::SyncToken> sync_token_fences,
               uint32_t order_num) {
    // Block thread when waiting for sync token. This avoids blocking when we
    // encounter the wait command later.
    for (const auto& sync_token : sync_token_fences) {
      base::WaitableEvent completion;
      if (service_->sync_point_manager()->Wait(
              sync_token, sync_point_order_data_->sequence_id(), order_num,
              base::BindOnce(&base::WaitableEvent::Signal,
                             base::Unretained(&completion)))) {
        TRACE_EVENT0("android_webview",
                     "TaskForwardingSequence::RunTask::WaitSyncToken");
        completion.Wait();
      }
    }
    sync_point_order_data_->BeginProcessingOrderNumber(order_num);
    std::move(task).Run();
    sync_point_order_data_->FinishProcessingOrderNumber(order_num);
  }

  // Raw ptr is ok because the task executor (service) is guaranteed to outlive
  // its task sequences.
  DeferredGpuCommandService* const service_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
  base::WeakPtrFactory<TaskForwardingSequence> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(TaskForwardingSequence);
};

// static
DeferredGpuCommandService*
DeferredGpuCommandService::CreateDeferredGpuCommandService() {
  gpu::GPUInfo gpu_info;
  gpu::GpuFeatureInfo gpu_feature_info;
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  gpu::GpuPreferences gpu_preferences =
      content::GetGpuPreferencesFromCommandLine();
  bool success = gpu::InitializeGLThreadSafe(
      base::CommandLine::ForCurrentProcess(), gpu_preferences, &gpu_info,
      &gpu_feature_info);
  if (!success) {
    LOG(FATAL) << "gpu::InitializeGLThreadSafe() failed.";
  }
  return new DeferredGpuCommandService(
      std::make_unique<gpu::SyncPointManager>(), gpu_preferences, gpu_info,
      gpu_feature_info);
}

// static
DeferredGpuCommandService* DeferredGpuCommandService::GetInstance() {
  static base::NoDestructor<scoped_refptr<DeferredGpuCommandService>> service(
      CreateDeferredGpuCommandService());
  return service->get();
}

DeferredGpuCommandService::DeferredGpuCommandService(
    std::unique_ptr<gpu::SyncPointManager> sync_point_manager,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : gpu::CommandBufferTaskExecutor(gpu_preferences,
                                     gpu_feature_info,
                                     sync_point_manager.get(),
                                     nullptr,
                                     nullptr,
                                     gl::GLSurfaceFormat()),
      sync_point_manager_(std::move(sync_point_manager)),
      gpu_info_(gpu_info) {}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

// This method can be called on any thread.
// static
void DeferredGpuCommandService::RequestProcessGL(bool for_idle) {
  RenderThreadManager* renderer_state =
      GLViewRendererManager::GetInstance()->GetMostRecentlyDrawn();
  if (!renderer_state) {
    LOG(ERROR) << "No hardware renderer. Deadlock likely";
    return;
  }
  renderer_state->ClientRequestInvokeGL(for_idle);
}

// Called from different threads!
void DeferredGpuCommandService::ScheduleTask(base::OnceClosure task,
                                             bool out_of_order) {
  {
    base::AutoLock lock(tasks_lock_);
    if (out_of_order)
      tasks_.emplace_front(std::move(task));
    else
      tasks_.emplace_back(std::move(task));
  }
  if (ScopedAllowGL::IsAllowed()) {
    RunTasks();
  } else {
    RequestProcessGL(false);
  }
}

size_t DeferredGpuCommandService::IdleQueueSize() {
  base::AutoLock lock(tasks_lock_);
  return idle_tasks_.size();
}

std::unique_ptr<gpu::CommandBufferTaskExecutor::Sequence>
DeferredGpuCommandService::CreateSequence() {
  return std::make_unique<TaskForwardingSequence>(this);
}

void DeferredGpuCommandService::ScheduleOutOfOrderTask(base::OnceClosure task) {
  ScheduleTask(std::move(task), true /* out_of_order */);
}

void DeferredGpuCommandService::ScheduleDelayedWork(
    base::OnceClosure callback) {
  {
    base::AutoLock lock(tasks_lock_);
    idle_tasks_.push(std::make_pair(base::Time::Now(), std::move(callback)));
  }
  RequestProcessGL(true);
}

void DeferredGpuCommandService::PerformIdleWork(bool is_idle) {
  TRACE_EVENT1("android_webview", "DeferredGpuCommandService::PerformIdleWork",
               "is_idle", is_idle);
  DCHECK(ScopedAllowGL::IsAllowed());
  static const base::TimeDelta kMaxIdleAge =
      base::TimeDelta::FromMilliseconds(16);

  const base::Time now = base::Time::Now();
  size_t queue_size = IdleQueueSize();
  while (queue_size--) {
    base::OnceClosure task;
    {
      base::AutoLock lock(tasks_lock_);
      if (!is_idle) {
        // Only run old tasks if we are not really idle right now.
        base::TimeDelta age(now - idle_tasks_.front().first);
        if (age < kMaxIdleAge)
          break;
      }
      task = std::move(idle_tasks_.front().second);
      idle_tasks_.pop();
    }
    std::move(task).Run();
  }
}

void DeferredGpuCommandService::PerformAllIdleWork() {
  TRACE_EVENT0("android_webview",
               "DeferredGpuCommandService::PerformAllIdleWork");
  while (IdleQueueSize()) {
    PerformIdleWork(true);
  }
}

bool DeferredGpuCommandService::ForceVirtualizedGLContexts() const {
  return true;
}

bool DeferredGpuCommandService::ShouldCreateMemoryTracker() const {
  return false;
}

void DeferredGpuCommandService::RunTasks() {
  TRACE_EVENT0("android_webview", "DeferredGpuCommandService::RunTasks");
  bool has_more_tasks;
  {
    base::AutoLock lock(tasks_lock_);
    has_more_tasks = tasks_.size() > 0;
  }

  while (has_more_tasks) {
    base::OnceClosure task;
    {
      base::AutoLock lock(tasks_lock_);
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    std::move(task).Run();
    {
      base::AutoLock lock(tasks_lock_);
      has_more_tasks = tasks_.size() > 0;
    }
  }
}

bool DeferredGpuCommandService::BlockThreadOnWaitSyncToken() const {
  return true;
}

bool DeferredGpuCommandService::CanSupportThreadedTextureMailbox() const {
  return gpu_info_.can_support_threaded_texture_mailbox;
}

}  // namespace android_webview
