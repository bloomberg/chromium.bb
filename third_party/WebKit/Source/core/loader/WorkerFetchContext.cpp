// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/WorkerFetchContext.h"

#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Supplementable.h"
#include "platform/WebTaskRunner.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURLRequest.h"
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
  return new WorkerFetchContext(worker_global_scope, std::move(web_context));
}

WorkerFetchContext::WorkerFetchContext(
    WorkerGlobalScope& worker_global_scope,
    std::unique_ptr<WebWorkerFetchContext> web_context)
    : BaseFetchContext(&worker_global_scope),
      worker_global_scope_(worker_global_scope),
      web_context_(std::move(web_context)),
      loading_task_runner_(Platform::Current()
                               ->CurrentThread()
                               ->Scheduler()
                               ->LoadingTaskRunner()) {
  web_context_->InitializeOnWorkerThread(
      loading_task_runner_->ToSingleThreadTaskRunner());
}

ResourceFetcher* WorkerFetchContext::GetResourceFetcher() {
  if (resource_fetcher_)
    return resource_fetcher_;
  resource_fetcher_ = ResourceFetcher::Create(this);
  return resource_fetcher_;
}

ContentSettingsClient* WorkerFetchContext::GetContentSettingsClient() const {
  // TODO(horo): Implement this.
  return nullptr;
}

Settings* WorkerFetchContext::GetSettings() const {
  // TODO(horo): Implement this.
  return nullptr;
}

SubresourceFilter* WorkerFetchContext::GetSubresourceFilter() const {
  // TODO(horo): Implement this.
  return nullptr;
}

SecurityContext* WorkerFetchContext::GetParentSecurityContext() const {
  // TODO(horo): Implement this.
  return nullptr;
}

bool WorkerFetchContext::ShouldBlockRequestByInspector(
    const ResourceRequest& resource_request) const {
  // TODO(horo): Implement this.
  return false;
}

void WorkerFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason) const {
  // TODO(horo): Implement this.
}

void WorkerFetchContext::ReportLocalLoadFailed(const KURL&) const {
  // TODO(horo): Implement this.
}

bool WorkerFetchContext::ShouldBypassMainWorldCSP() const {
  // TODO(horo): Implement this.
  return false;
}

bool WorkerFetchContext::IsSVGImageChromeClient() const {
  return false;
}

void WorkerFetchContext::CountUsage(UseCounter::Feature feature) const {
  UseCounter::Count(worker_global_scope_, feature);
}

void WorkerFetchContext::CountDeprecation(UseCounter::Feature feature) const {
  Deprecation::CountDeprecation(worker_global_scope_, feature);
}

bool WorkerFetchContext::ShouldBlockFetchByMixedContentCheck(
    const ResourceRequest& resource_request,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  // TODO(horo): Implement this.
  return false;
}

std::unique_ptr<WebURLLoader> WorkerFetchContext::CreateURLLoader() {
  return web_context_->CreateURLLoader();
}

bool WorkerFetchContext::IsControlledByServiceWorker() const {
  return web_context_->IsControlledByServiceWorker();
}

void WorkerFetchContext::PrepareRequest(ResourceRequest& request,
                                        RedirectType) {
  request.OverrideLoadingIPCType(WebURLRequest::LoadingIPCType::kMojo);
  WrappedResourceRequest webreq(request);
  web_context_->WillSendRequest(webreq);
}

RefPtr<WebTaskRunner> WorkerFetchContext::LoadingTaskRunner() const {
  return loading_task_runner_;
}

void WorkerFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                     FetchResourceType type) {
  BaseFetchContext::AddAdditionalRequestHeaders(request, type);

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (web_context_->IsDataSaverEnabled())
    request.SetHTTPHeaderField("Save-Data", "on");
}

void WorkerFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  WorkerGlobalScopePerformance::performance(*worker_global_scope_)
      ->AddResourceTiming(info);
}

DEFINE_TRACE(WorkerFetchContext) {
  visitor->Trace(worker_global_scope_);
  visitor->Trace(resource_fetcher_);
  BaseFetchContext::Trace(visitor);
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
