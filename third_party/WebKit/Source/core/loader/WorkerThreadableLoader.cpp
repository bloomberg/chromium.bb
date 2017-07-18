/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/WorkerThreadableLoader.h"

#include <memory>
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/SafePoint.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/debug/Alias.h"

namespace blink {

namespace {

std::unique_ptr<Vector<char>> CreateVectorFromMemoryRegion(
    const char* data,
    unsigned data_length) {
  std::unique_ptr<Vector<char>> buffer =
      WTF::MakeUnique<Vector<char>>(data_length);
  memcpy(buffer->data(), data, data_length);
  return buffer;
}

}  // namespace

class WorkerThreadableLoader::AsyncTaskForwarder final
    : public WorkerThreadableLoader::TaskForwarder {
 public:
  explicit AsyncTaskForwarder(RefPtr<WebTaskRunner> worker_loading_task_runner)
      : worker_loading_task_runner_(std::move(worker_loading_task_runner)) {
    DCHECK(IsMainThread());
  }
  ~AsyncTaskForwarder() override { DCHECK(IsMainThread()); }

  void ForwardTask(const WebTraceLocation& location,
                   std::unique_ptr<CrossThreadClosure> task) override {
    DCHECK(IsMainThread());
    worker_loading_task_runner_->PostTask(location, std::move(task));
  }
  void ForwardTaskWithDoneSignal(
      const WebTraceLocation& location,
      std::unique_ptr<CrossThreadClosure> task) override {
    DCHECK(IsMainThread());
    worker_loading_task_runner_->PostTask(location, std::move(task));
  }
  void Abort() override { DCHECK(IsMainThread()); }

 private:
  RefPtr<WebTaskRunner> worker_loading_task_runner_;
};

struct WorkerThreadableLoader::TaskWithLocation final {
  TaskWithLocation(const WebTraceLocation& location,
                   std::unique_ptr<CrossThreadClosure> task)
      : location_(location), task_(std::move(task)) {}
  TaskWithLocation(TaskWithLocation&& task)
      : TaskWithLocation(task.location_, std::move(task.task_)) {}
  ~TaskWithLocation() = default;

  WebTraceLocation location_;
  std::unique_ptr<CrossThreadClosure> task_;
};

// Observing functions and wait() need to be called on the worker thread.
// Setting functions and signal() need to be called on the main thread.
// All observing functions must be called after wait() returns, and all
// setting functions must be called before signal() is called.
class WorkerThreadableLoader::WaitableEventWithTasks final
    : public ThreadSafeRefCounted<WaitableEventWithTasks> {
 public:
  static PassRefPtr<WaitableEventWithTasks> Create() {
    return AdoptRef(new WaitableEventWithTasks);
  }

  void Signal() {
    DCHECK(IsMainThread());
    CHECK(!is_signal_called_);
    is_signal_called_ = true;
    event_.Signal();
  }
  void Wait() {
    DCHECK(!IsMainThread());
    CHECK(!is_wait_done_);
    event_.Wait();
    is_wait_done_ = true;
  }

  // Observing functions
  bool IsAborted() const {
    DCHECK(!IsMainThread());
    CHECK(is_wait_done_);
    return is_aborted_;
  }
  Vector<TaskWithLocation> Take() {
    DCHECK(!IsMainThread());
    CHECK(is_wait_done_);
    return std::move(tasks_);
  }

  // Setting functions
  void Append(TaskWithLocation task) {
    DCHECK(IsMainThread());
    CHECK(!is_signal_called_);
    tasks_.push_back(std::move(task));
  }
  void SetIsAborted() {
    DCHECK(IsMainThread());
    CHECK(!is_signal_called_);
    is_aborted_ = true;
  }

 private:
  WaitableEventWithTasks() {}

  WaitableEvent event_;
  Vector<TaskWithLocation> tasks_;
  bool is_aborted_ = false;
  bool is_signal_called_ = false;
  bool is_wait_done_ = false;
};

class WorkerThreadableLoader::SyncTaskForwarder final
    : public WorkerThreadableLoader::TaskForwarder {
 public:
  explicit SyncTaskForwarder(
      PassRefPtr<WaitableEventWithTasks> event_with_tasks)
      : event_with_tasks_(std::move(event_with_tasks)) {
    DCHECK(IsMainThread());
  }
  ~SyncTaskForwarder() override { DCHECK(IsMainThread()); }

  void ForwardTask(const WebTraceLocation& location,
                   std::unique_ptr<CrossThreadClosure> task) override {
    DCHECK(IsMainThread());
    event_with_tasks_->Append(TaskWithLocation(location, std::move(task)));
  }
  void ForwardTaskWithDoneSignal(
      const WebTraceLocation& location,
      std::unique_ptr<CrossThreadClosure> task) override {
    DCHECK(IsMainThread());
    event_with_tasks_->Append(TaskWithLocation(location, std::move(task)));
    event_with_tasks_->Signal();
  }
  void Abort() override {
    DCHECK(IsMainThread());
    event_with_tasks_->SetIsAborted();
    event_with_tasks_->Signal();
  }

 private:
  RefPtr<WaitableEventWithTasks> event_with_tasks_;
};

WorkerThreadableLoader::WorkerThreadableLoader(
    WorkerGlobalScope& worker_global_scope,
    ThreadableLoaderClient* client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options,
    BlockingBehavior blocking_behavior)
    : worker_global_scope_(&worker_global_scope),
      parent_frame_task_runners_(
          worker_global_scope.GetThread()->GetParentFrameTaskRunners()),
      client_(client),
      threadable_loader_options_(options),
      resource_loader_options_(resource_loader_options),
      blocking_behavior_(blocking_behavior) {
  DCHECK(client);
}

void WorkerThreadableLoader::LoadResourceSynchronously(
    WorkerGlobalScope& worker_global_scope,
    const ResourceRequest& request,
    ThreadableLoaderClient& client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options) {
  (new WorkerThreadableLoader(worker_global_scope, &client, options,
                              resource_loader_options, kLoadSynchronously))
      ->Start(request);
}

WorkerThreadableLoader::~WorkerThreadableLoader() {
  DCHECK(!main_thread_loader_holder_);
  DCHECK(!client_);
}

void WorkerThreadableLoader::Start(const ResourceRequest& original_request) {
  DCHECK(worker_global_scope_->IsContextThread());
  ResourceRequest request(original_request);
  if (!request.DidSetHTTPReferrer()) {
    request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        worker_global_scope_->GetReferrerPolicy(), request.Url(),
        worker_global_scope_->OutgoingReferrer()));
  }

  RefPtr<WaitableEventWithTasks> event_with_tasks;
  if (blocking_behavior_ == kLoadSynchronously)
    event_with_tasks = WaitableEventWithTasks::Create();

  WorkerThread* worker_thread = worker_global_scope_->GetThread();
  RefPtr<WebTaskRunner> worker_loading_task_runner = TaskRunnerHelper::Get(
      TaskType::kUnspecedLoading, worker_global_scope_.Get());
  parent_frame_task_runners_->Get(TaskType::kUnspecedLoading)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(
              &MainThreadLoaderHolder::CreateAndStart,
              WrapCrossThreadPersistent(this),
              WrapCrossThreadPersistent(worker_thread->GetLoadingContext()),
              std::move(worker_loading_task_runner),
              WrapCrossThreadPersistent(
                  worker_thread->GetWorkerThreadLifecycleContext()),
              request, threadable_loader_options_, resource_loader_options_,
              event_with_tasks));

  if (blocking_behavior_ == kLoadAsynchronously)
    return;

  event_with_tasks->Wait();

  if (event_with_tasks->IsAborted()) {
    // This thread is going to terminate.
    Cancel();
    return;
  }

  for (const auto& task : event_with_tasks->Take()) {
    // Store the program counter where the task is posted from, and alias
    // it to ensure it is stored in the crash dump.
    const void* program_counter = task.location_.program_counter();
    WTF::debug::Alias(&program_counter);

    (*task.task_)();
  }
}

void WorkerThreadableLoader::OverrideTimeout(
    unsigned long timeout_milliseconds) {
  DCHECK(!IsMainThread());
  if (!main_thread_loader_holder_)
    return;
  parent_frame_task_runners_->Get(TaskType::kUnspecedLoading)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&MainThreadLoaderHolder::OverrideTimeout,
                          main_thread_loader_holder_, timeout_milliseconds));
}

void WorkerThreadableLoader::Cancel() {
  DCHECK(!IsMainThread());
  if (main_thread_loader_holder_) {
    parent_frame_task_runners_->Get(TaskType::kUnspecedLoading)
        ->PostTask(BLINK_FROM_HERE,
                   CrossThreadBind(&MainThreadLoaderHolder::Cancel,
                                   main_thread_loader_holder_));
    main_thread_loader_holder_ = nullptr;
  }

  if (!client_)
    return;

  // If the client hasn't reached a termination state, then transition it
  // by sending a cancellation error.
  // Note: no more client callbacks will be done after this method -- the
  // clearClient() call ensures that.
  ResourceError error(String(), 0, String(), String());
  error.SetIsCancellation(true);
  DidFail(error);
  DCHECK(!client_);
}

void WorkerThreadableLoader::DidStart(
    MainThreadLoaderHolder* main_thread_loader_holder) {
  DCHECK(!IsMainThread());
  DCHECK(!main_thread_loader_holder_);
  DCHECK(main_thread_loader_holder);
  if (!client_) {
    // The thread is terminating.
    parent_frame_task_runners_->Get(TaskType::kUnspecedLoading)
        ->PostTask(BLINK_FROM_HERE,
                   CrossThreadBind(
                       &MainThreadLoaderHolder::Cancel,
                       WrapCrossThreadPersistent(main_thread_loader_holder)));
    return;
  }

  main_thread_loader_holder_ = main_thread_loader_holder;
}

void WorkerThreadableLoader::DidSendData(
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void WorkerThreadableLoader::DidReceiveRedirectTo(const KURL& url) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  client_->DidReceiveRedirectTo(url);
}

void WorkerThreadableLoader::DidReceiveResponse(
    unsigned long identifier,
    std::unique_ptr<CrossThreadResourceResponseData> response_data,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  ResourceResponse response(response_data.get());
  client_->DidReceiveResponse(identifier, response, std::move(handle));
}

void WorkerThreadableLoader::DidReceiveData(
    std::unique_ptr<Vector<char>> data) {
  DCHECK(!IsMainThread());
  CHECK_LE(data->size(), std::numeric_limits<unsigned>::max());
  if (!client_)
    return;
  client_->DidReceiveData(data->data(), data->size());
}

void WorkerThreadableLoader::DidReceiveCachedMetadata(
    std::unique_ptr<Vector<char>> data) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  client_->DidReceiveCachedMetadata(data->data(), data->size());
}

void WorkerThreadableLoader::DidFinishLoading(unsigned long identifier,
                                              double finish_time) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  auto* client = client_;
  client_ = nullptr;
  main_thread_loader_holder_ = nullptr;
  client->DidFinishLoading(identifier, finish_time);
}

void WorkerThreadableLoader::DidFail(const ResourceError& error) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  auto* client = client_;
  client_ = nullptr;
  main_thread_loader_holder_ = nullptr;
  client->DidFail(error);
}

void WorkerThreadableLoader::DidFailRedirectCheck() {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  auto* client = client_;
  client_ = nullptr;
  main_thread_loader_holder_ = nullptr;
  client->DidFailRedirectCheck();
}

void WorkerThreadableLoader::DidDownloadData(int data_length) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  client_->DidDownloadData(data_length);
}

void WorkerThreadableLoader::DidReceiveResourceTiming(
    std::unique_ptr<CrossThreadResourceTimingInfoData> timing_data) {
  DCHECK(!IsMainThread());
  if (!client_)
    return;
  RefPtr<ResourceTimingInfo> info(
      ResourceTimingInfo::Adopt(std::move(timing_data)));
  WorkerGlobalScopePerformance::performance(*worker_global_scope_)
      ->AddResourceTiming(*info);
  client_->DidReceiveResourceTiming(*info);
}

DEFINE_TRACE(WorkerThreadableLoader) {
  visitor->Trace(worker_global_scope_);
  ThreadableLoader::Trace(visitor);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::CreateAndStart(
    WorkerThreadableLoader* worker_loader,
    ThreadableLoadingContext* loading_context,
    RefPtr<WebTaskRunner> worker_loading_task_runner,
    WorkerThreadLifecycleContext* worker_thread_lifecycle_context,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options,
    PassRefPtr<WaitableEventWithTasks> event_with_tasks) {
  DCHECK(IsMainThread());
  TaskForwarder* forwarder;
  if (event_with_tasks)
    forwarder = new SyncTaskForwarder(std::move(event_with_tasks));
  else
    forwarder = new AsyncTaskForwarder(std::move(worker_loading_task_runner));

  MainThreadLoaderHolder* main_thread_loader_holder =
      new MainThreadLoaderHolder(forwarder, worker_thread_lifecycle_context);
  if (main_thread_loader_holder->WasContextDestroyedBeforeObserverCreation()) {
    // The thread is already terminating.
    forwarder->Abort();
    main_thread_loader_holder->forwarder_ = nullptr;
    return;
  }
  main_thread_loader_holder->worker_loader_ = worker_loader;
  forwarder->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidStart,
                      WrapCrossThreadPersistent(worker_loader),
                      WrapCrossThreadPersistent(main_thread_loader_holder)));
  main_thread_loader_holder->Start(*loading_context, std::move(request),
                                   options, resource_loader_options);
}

WorkerThreadableLoader::MainThreadLoaderHolder::~MainThreadLoaderHolder() {
  DCHECK(IsMainThread());
  DCHECK(!worker_loader_);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::OverrideTimeout(
    unsigned long timeout_milliseconds) {
  DCHECK(IsMainThread());
  if (!main_thread_loader_)
    return;
  main_thread_loader_->OverrideTimeout(timeout_milliseconds);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::Cancel() {
  DCHECK(IsMainThread());
  worker_loader_ = nullptr;
  if (!main_thread_loader_)
    return;
  main_thread_loader_->Cancel();
  main_thread_loader_ = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidSendData(
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidSendData, worker_loader,
                      bytes_sent, total_bytes_to_be_sent));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidReceiveRedirectTo(
    const KURL& url) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidReceiveRedirectTo,
                      worker_loader, url));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidReceiveResponse,
                      worker_loader, identifier, response,
                      WTF::Passed(std::move(handle))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidReceiveData(
    const char* data,
    unsigned data_length) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &WorkerThreadableLoader::DidReceiveData, worker_loader,
          WTF::Passed(CreateVectorFromMemoryRegion(data, data_length))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidDownloadData(
    int data_length) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE, CrossThreadBind(&WorkerThreadableLoader::DidDownloadData,
                                       worker_loader, data_length));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidReceiveCachedMetadata(
    const char* data,
    int data_length) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &WorkerThreadableLoader::DidReceiveCachedMetadata, worker_loader,
          WTF::Passed(CreateVectorFromMemoryRegion(data, data_length))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidFinishLoading(
    unsigned long identifier,
    double finish_time) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Release();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidFinishLoading, worker_loader,
                      identifier, finish_time));
  forwarder_ = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidFail(
    const ResourceError& error) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Release();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidFail, worker_loader, error));
  forwarder_ = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidFailRedirectCheck() {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Release();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidFailRedirectCheck,
                      worker_loader));
  forwarder_ = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::DidReceiveResourceTiming(
    const ResourceTimingInfo& info) {
  DCHECK(IsMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> worker_loader =
      worker_loader_.Get();
  if (!worker_loader || !forwarder_)
    return;
  forwarder_->ForwardTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadableLoader::DidReceiveResourceTiming,
                      worker_loader, info));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::ContextDestroyed(
    WorkerThreadLifecycleContext*) {
  DCHECK(IsMainThread());
  if (forwarder_) {
    forwarder_->Abort();
    forwarder_ = nullptr;
  }
  Cancel();
}

DEFINE_TRACE(WorkerThreadableLoader::MainThreadLoaderHolder) {
  visitor->Trace(forwarder_);
  visitor->Trace(main_thread_loader_);
  WorkerThreadLifecycleObserver::Trace(visitor);
}

WorkerThreadableLoader::MainThreadLoaderHolder::MainThreadLoaderHolder(
    TaskForwarder* forwarder,
    WorkerThreadLifecycleContext* context)
    : WorkerThreadLifecycleObserver(context), forwarder_(forwarder) {
  DCHECK(IsMainThread());
}

void WorkerThreadableLoader::MainThreadLoaderHolder::Start(
    ThreadableLoadingContext& loading_context,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& original_resource_loader_options) {
  DCHECK(IsMainThread());
  ResourceLoaderOptions resource_loader_options =
      original_resource_loader_options;
  resource_loader_options.request_initiator_context = kWorkerContext;
  main_thread_loader_ = DocumentThreadableLoader::Create(
      loading_context, this, options, resource_loader_options);
  main_thread_loader_->Start(ResourceRequest(request.get()));
}

}  // namespace blink
