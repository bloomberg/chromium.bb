// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/deferred_gpu_command_service.h"

#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager_factory.h"
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
}

ScopedAllowGL::~ScopedAllowGL() {
  DeferredGpuCommandService* service = DeferredGpuCommandService::GetInstance();
  DCHECK(service);
  service->RunAllTasks();
  allow_gl.Get().Set(false);
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
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool success = gpu::InitializeGLThreadSafe(command_line, gpu_preferences,
                                             &gpu_info, &gpu_feature_info);
  if (!success) {
    LOG(FATAL) << "gpu::InitializeGLThreadSafe() failed.";
  }
  auto sync_point_manager = std::make_unique<gpu::SyncPointManager>();
  auto mailbox_manager = gpu::gles2::CreateMailboxManager(gpu_preferences);
  // The shared_image_manager will be shared between renderer thread and GPU
  // main thread, so it should be thread safe.
  auto shared_image_manager =
      std::make_unique<gpu::SharedImageManager>(true /* thread_safe */);
  return new DeferredGpuCommandService(
      std::move(sync_point_manager), std::move(mailbox_manager),
      std::move(shared_image_manager), gpu_preferences, gpu_info,
      gpu_feature_info);
}

// static
DeferredGpuCommandService* DeferredGpuCommandService::GetInstance() {
  static DeferredGpuCommandService* service = CreateDeferredGpuCommandService();
  return service;
}

DeferredGpuCommandService::DeferredGpuCommandService(
    std::unique_ptr<gpu::SyncPointManager> sync_point_manager,
    std::unique_ptr<gpu::MailboxManager> mailbox_manager,
    std::unique_ptr<gpu::SharedImageManager> shared_image_manager,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : gpu::CommandBufferTaskExecutor(gpu_preferences,
                                     gpu_feature_info,
                                     sync_point_manager.get(),
                                     mailbox_manager.get(),
                                     nullptr,
                                     gl::GLSurfaceFormat(),
                                     shared_image_manager.get(),
                                     nullptr,
                                     nullptr),
      sync_point_manager_(std::move(sync_point_manager)),
      mailbox_manager_(std::move(mailbox_manager)),
      gpu_info_(gpu_info),
      shared_image_manager_(std::move(shared_image_manager)) {
  DETACH_FROM_THREAD(task_queue_thread_checker_);
}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  DCHECK(tasks_.empty());
}

// Called from different threads!
void DeferredGpuCommandService::ScheduleTask(base::OnceClosure task,
                                             bool out_of_order) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  LOG_IF(FATAL, !ScopedAllowGL::IsAllowed())
      << "ScheduleTask outside of ScopedAllowGL";
  if (out_of_order)
    tasks_.emplace_front(std::move(task));
  else
    tasks_.emplace_back(std::move(task));
  RunTasks();
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
  LOG_IF(FATAL, !ScopedAllowGL::IsAllowed())
      << "ScheduleDelayedWork outside of ScopedAllowGL";
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  idle_tasks_.push(std::make_pair(base::Time::Now(), std::move(callback)));
}

void DeferredGpuCommandService::PostNonNestableToClient(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  client_tasks_.emplace(std::move(callback));
}

void DeferredGpuCommandService::PerformAllIdleWork() {
  TRACE_EVENT0("android_webview",
               "DeferredGpuCommandService::PerformAllIdleWork");
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_idle_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_idle_tasks_, true);
  while (idle_tasks_.size()) {
    base::OnceClosure task = std::move(idle_tasks_.front().second);
    idle_tasks_.pop();
    std::move(task).Run();
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
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_tasks_, true);
  while (tasks_.size()) {
    std::move(tasks_.front()).Run();
    tasks_.pop_front();
  }
}

void DeferredGpuCommandService::RunAllTasks() {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  RunTasks();
  PerformAllIdleWork();
  DCHECK(tasks_.empty());
  DCHECK(idle_tasks_.empty());

  // Client tasks may generate more service tasks, so run this
  // in a loop.
  while (!client_tasks_.empty()) {
    base::queue<base::OnceClosure> local_client_tasks;
    local_client_tasks.swap(client_tasks_);
    while (!local_client_tasks.empty()) {
      std::move(local_client_tasks.front()).Run();
      local_client_tasks.pop();
    }

    RunTasks();
    PerformAllIdleWork();
    DCHECK(tasks_.empty());
    DCHECK(idle_tasks_.empty());
  }
}

bool DeferredGpuCommandService::CanSupportThreadedTextureMailbox() const {
  return gpu_info_.can_support_threaded_texture_mailbox;
}

}  // namespace android_webview
