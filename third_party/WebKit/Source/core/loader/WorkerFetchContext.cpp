// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/WorkerFetchContext.h"

#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Supplementable.h"
#include "platform/WebTaskRunner.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebWorkerFetchContext.h"

namespace blink {

namespace {

// WorkerFetchContextHolder is used to pass the WebWorkerFetchContext from the
// main thread to the worker thread by attaching to the WorkerClients as a
// Supplement.
class WorkerFetchContextHolder final
    : public GarbageCollectedFinalized<WorkerFetchContextHolder>,
      public Supplement<WorkerClients> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerFetchContextHolder);

 public:
  static WorkerFetchContextHolder* From(WorkerClients& clients) {
    return static_cast<WorkerFetchContextHolder*>(
        Supplement<WorkerClients>::From(clients, SupplementName()));
  }
  static const char* SupplementName() { return "WorkerFetchContextHolder"; }

  explicit WorkerFetchContextHolder(
      std::unique_ptr<WebWorkerFetchContext> web_context)
      : web_context_(std::move(web_context)) {}
  virtual ~WorkerFetchContextHolder() {}

  std::unique_ptr<WebWorkerFetchContext> TakeContext() {
    return std::move(web_context_);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<WorkerClients>::Trace(visitor); }

 private:
  std::unique_ptr<WebWorkerFetchContext> web_context_;
};

}  // namespace

WorkerFetchContext::~WorkerFetchContext() {}

WorkerFetchContext* WorkerFetchContext::Create(
    WorkerGlobalScope& worker_global_scope) {
  DCHECK(worker_global_scope.IsContextThread());
  WorkerClients* worker_clients = worker_global_scope.Clients();
  DCHECK(worker_clients);
  WorkerFetchContextHolder* holder =
      static_cast<WorkerFetchContextHolder*>(Supplement<WorkerClients>::From(
          *worker_clients, WorkerFetchContextHolder::SupplementName()));
  if (!holder)
    return nullptr;
  std::unique_ptr<WebWorkerFetchContext> web_context = holder->TakeContext();
  DCHECK(web_context);
  return new WorkerFetchContext(std::move(web_context));
}

WorkerFetchContext::WorkerFetchContext(
    std::unique_ptr<WebWorkerFetchContext> web_context)
    : web_context_(std::move(web_context)) {
  web_context_->InitializeOnWorkerThread(Platform::Current()
                                             ->CurrentThread()
                                             ->Scheduler()
                                             ->LoadingTaskRunner()
                                             ->ToSingleThreadTaskRunner());
}

std::unique_ptr<WebURLLoader> WorkerFetchContext::CreateURLLoader() {
  return web_context_->CreateURLLoader();
}

bool WorkerFetchContext::IsControlledByServiceWorker() const {
  return web_context_->IsControlledByServiceWorker();
}

void WorkerFetchContext::PrepareRequest(ResourceRequest& request,
                                        RedirectType) {
  WrappedResourceRequest webreq(request);
  web_context_->WillSendRequest(webreq);
}

void ProvideWorkerFetchContextToWorker(
    WorkerClients* clients,
    std::unique_ptr<WebWorkerFetchContext> web_context) {
  DCHECK(clients);
  WorkerFetchContextHolder::ProvideTo(
      *clients, WorkerFetchContextHolder::SupplementName(),
      new WorkerFetchContextHolder(std::move(web_context)));
}

}  // namespace blink
