// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/WorkerFetchContext.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/loader/MixedContentChecker.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "core/workers/WorkerClients.h"
#include "platform/Supplementable.h"
#include "platform/WebTaskRunner.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "public/platform/WebMixedContent.h"
#include "public/platform/WebMixedContentContextType.h"
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
    WorkerOrWorkletGlobalScope& global_scope) {
  DCHECK(global_scope.IsContextThread());
  DCHECK(!global_scope.IsMainThreadWorkletGlobalScope());
  WorkerClients* worker_clients = global_scope.Clients();
  DCHECK(worker_clients);
  WorkerFetchContextHolder* holder =
      static_cast<WorkerFetchContextHolder*>(Supplement<WorkerClients>::From(
          *worker_clients, WorkerFetchContextHolder::SupplementName()));
  if (!holder)
    return nullptr;
  std::unique_ptr<WebWorkerFetchContext> web_context = holder->TakeContext();
  DCHECK(web_context);
  return new WorkerFetchContext(global_scope, std::move(web_context));
}

WorkerFetchContext::WorkerFetchContext(
    WorkerOrWorkletGlobalScope& global_scope,
    std::unique_ptr<WebWorkerFetchContext> web_context)
    : global_scope_(global_scope),
      web_context_(std::move(web_context)),
      loading_task_runner_(
          TaskRunnerHelper::Get(TaskType::kUnspecedLoading, global_scope_)) {
  web_context_->InitializeOnWorkerThread(
      loading_task_runner_->ToSingleThreadTaskRunner());
}

ResourceFetcher* WorkerFetchContext::GetResourceFetcher() {
  if (resource_fetcher_)
    return resource_fetcher_;
  resource_fetcher_ = ResourceFetcher::Create(this, loading_task_runner_);
  return resource_fetcher_;
}

KURL WorkerFetchContext::FirstPartyForCookies() const {
  return web_context_->FirstPartyForCookies();
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

bool WorkerFetchContext::ShouldBypassMainWorldCSP() const {
  // This method was introduced to bypass the page's CSP while running the
  // script from an isolated world (ex: Chrome extensions). But worker threads
  // doesn't have any isolated world. So we can just return false.
  return false;
}

bool WorkerFetchContext::IsSVGImageChromeClient() const {
  return false;
}

void WorkerFetchContext::CountUsage(WebFeature feature) const {
  UseCounter::Count(global_scope_, feature);
}

void WorkerFetchContext::CountDeprecation(WebFeature feature) const {
  Deprecation::CountDeprecation(global_scope_, feature);
}

bool WorkerFetchContext::ShouldBlockFetchByMixedContentCheck(
    const ResourceRequest& resource_request,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  // TODO(horo): We need more detailed check which is implemented in
  // MixedContentChecker::ShouldBlockFetch().
  return MixedContentChecker::IsMixedContent(global_scope_->GetSecurityOrigin(),
                                             url);
}

ReferrerPolicy WorkerFetchContext::GetReferrerPolicy() const {
  return global_scope_->GetReferrerPolicy();
}

String WorkerFetchContext::GetOutgoingReferrer() const {
  return global_scope_->OutgoingReferrer();
}

const KURL& WorkerFetchContext::Url() const {
  return global_scope_->Url();
}

const SecurityOrigin* WorkerFetchContext::GetParentSecurityOrigin() const {
  // This method was introduced to check the parent frame's security context
  // while loading iframe document resources. So this method is not suitable for
  // workers.
  NOTREACHED();
  return nullptr;
}

Optional<WebAddressSpace> WorkerFetchContext::GetAddressSpace() const {
  return WTF::make_optional(global_scope_->GetSecurityContext().AddressSpace());
}

const ContentSecurityPolicy* WorkerFetchContext::GetContentSecurityPolicy()
    const {
  return global_scope_->GetContentSecurityPolicy();
}

void WorkerFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  return global_scope_->AddConsoleMessage(message);
}

SecurityOrigin* WorkerFetchContext::GetSecurityOrigin() const {
  return global_scope_->GetSecurityOrigin();
}

std::unique_ptr<WebURLLoader> WorkerFetchContext::CreateURLLoader(
    const ResourceRequest&) {
  auto loader = web_context_->CreateURLLoader();
  loader->SetLoadingTaskRunner(loading_task_runner_.Get());
  return loader;
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

void WorkerFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                     FetchResourceType type) {
  BaseFetchContext::AddAdditionalRequestHeaders(request, type);

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (web_context_->IsDataSaverEnabled())
    request.SetHTTPHeaderField("Save-Data", "on");
}

void WorkerFetchContext::DispatchDidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    Resource* resource,
    ResourceResponseType) {
  if (response.HasMajorCertificateErrors()) {
    WebMixedContentContextType context_type =
        WebMixedContent::ContextTypeFromRequestContext(
            request_context, false /* strictMixedContentCheckingForPlugin */);
    if (context_type == WebMixedContentContextType::kBlockable) {
      web_context_->DidRunContentWithCertificateErrors(response.Url());
    } else {
      web_context_->DidDisplayContentWithCertificateErrors(response.Url());
    }
  }
}

void WorkerFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // TODO(nhiroki): Add ResourceTiming API support once it's spec'ed for
  // worklets.
  if (global_scope_->IsWorkletGlobalScope())
    return;
  WorkerGlobalScopePerformance::performance(*ToWorkerGlobalScope(global_scope_))
      ->AddResourceTiming(info);
}

void WorkerFetchContext::PopulateResourceRequest(
    const KURL& url,
    Resource::Type type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest& out_request) {
  SetFirstPartyCookieAndRequestorOrigin(out_request);
}

void WorkerFetchContext::SetFirstPartyCookieAndRequestorOrigin(
    ResourceRequest& out_request) {
  if (out_request.FirstPartyForCookies().IsNull())
    out_request.SetFirstPartyForCookies(FirstPartyForCookies());
  // TODO(mkwst): It would be cleaner to adjust blink::ResourceRequest to
  // initialize itself with a `nullptr` initiator so that this can be a simple
  // `isNull()` check. https://crbug.com/625969
  if (out_request.RequestorOrigin()->IsUnique())
    out_request.SetRequestorOrigin(GetSecurityOrigin());
}

DEFINE_TRACE(WorkerFetchContext) {
  visitor->Trace(global_scope_);
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
