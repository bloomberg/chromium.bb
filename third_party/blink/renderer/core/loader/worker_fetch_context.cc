// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

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
    return Supplement<WorkerClients>::From<WorkerFetchContextHolder>(clients);
  }
  static const char kSupplementName[];

  explicit WorkerFetchContextHolder(
      std::unique_ptr<WebWorkerFetchContext> web_context)
      : web_context_(std::move(web_context)) {}
  virtual ~WorkerFetchContextHolder() = default;

  std::unique_ptr<WebWorkerFetchContext> TakeContext() {
    return std::move(web_context_);
  }

  void Trace(blink::Visitor* visitor) override {
    Supplement<WorkerClients>::Trace(visitor);
  }

 private:
  std::unique_ptr<WebWorkerFetchContext> web_context_;
};

}  // namespace

// static
const char WorkerFetchContextHolder::kSupplementName[] =
    "WorkerFetchContextHolder";

WorkerFetchContext::~WorkerFetchContext() = default;

WorkerFetchContext* WorkerFetchContext::Create(
    WorkerOrWorkletGlobalScope& global_scope) {
  DCHECK(global_scope.IsContextThread());
  DCHECK(!global_scope.IsMainThreadWorkletGlobalScope());
  WorkerClients* worker_clients = global_scope.Clients();
  DCHECK(worker_clients);
  WorkerFetchContextHolder* holder =
      Supplement<WorkerClients>::From<WorkerFetchContextHolder>(
          *worker_clients);
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
          global_scope_->GetTaskRunner(TaskType::kInternalLoading)),
      save_data_enabled_(GetNetworkStateNotifier().SaveDataEnabled()) {
  web_context_->InitializeOnWorkerThread();
  std::unique_ptr<blink::WebDocumentSubresourceFilter> web_filter =
      web_context_->TakeSubresourceFilter();
  if (web_filter) {
    subresource_filter_ =
        SubresourceFilter::Create(global_scope, std::move(web_filter));
  }
}

KURL WorkerFetchContext::GetSiteForCookies() const {
  return web_context_->SiteForCookies();
}

SubresourceFilter* WorkerFetchContext::GetSubresourceFilter() const {
  return subresource_filter_.Get();
}

bool WorkerFetchContext::AllowScriptFromSource(const KURL& url) const {
  WorkerContentSettingsClient* settings_client =
      WorkerContentSettingsClient::From(*global_scope_);
  // If we're on a worker, script should be enabled, so no need to plumb
  // Settings::GetScriptEnabled() here.
  return !settings_client || settings_client->AllowScriptFromSource(true, url);
}

bool WorkerFetchContext::ShouldBlockRequestByInspector(const KURL& url) const {
  bool should_block_request = false;
  probe::shouldBlockRequest(global_scope_, url, &should_block_request);
  return should_block_request;
}

void WorkerFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason,
    Resource::Type resource_type) const {
  probe::didBlockRequest(global_scope_, resource_request, nullptr,
                         fetch_initiator_info, blocked_reason, resource_type);
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

bool WorkerFetchContext::ShouldBlockWebSocketByMixedContentCheck(
    const KURL& url) const {
  // Worklets don't support WebSocket.
  DCHECK(global_scope_->IsWorkerGlobalScope());
  return !MixedContentChecker::IsWebSocketAllowed(
      ToWorkerGlobalScope(global_scope_), web_context_.get(), url);
}

std::unique_ptr<WebSocketHandshakeThrottle>
WorkerFetchContext::CreateWebSocketHandshakeThrottle() {
  return web_context_->CreateWebSocketHandshakeThrottle();
}

bool WorkerFetchContext::ShouldBlockFetchByMixedContentCheck(
    WebURLRequest::RequestContext request_context,
    network::mojom::RequestContextFrameType frame_type,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  return MixedContentChecker::ShouldBlockFetchOnWorker(
      global_scope_, web_context_.get(), request_context, frame_type,
      redirect_status, url, reporting_policy);
}

bool WorkerFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  if ((!url.User().IsEmpty() || !url.Pass().IsEmpty()) &&
      resource_request.GetRequestContext() !=
          WebURLRequest::kRequestContextXMLHttpRequest) {
    if (Url().User() != url.User() || Url().Pass() != url.Pass()) {
      CountDeprecation(
          WebFeature::kRequestedSubresourceWithEmbeddedCredentials);

      // TODO(mkwst): Remove the runtime check one way or the other once we're
      // sure it's going to stick (or that it's not).
      if (RuntimeEnabledFeatures::BlockCredentialedSubresourcesEnabled())
        return true;
    }
  }
  return false;
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

base::Optional<mojom::IPAddressSpace> WorkerFetchContext::GetAddressSpace()
    const {
  return base::make_optional(
      global_scope_->GetSecurityContext().AddressSpace());
}

const ContentSecurityPolicy* WorkerFetchContext::GetContentSecurityPolicy()
    const {
  return global_scope_->GetContentSecurityPolicy();
}

void WorkerFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  return global_scope_->AddConsoleMessage(message);
}

const SecurityOrigin* WorkerFetchContext::GetSecurityOrigin() const {
  return global_scope_->GetSecurityOrigin();
}

std::unique_ptr<WebURLLoader> WorkerFetchContext::CreateURLLoader(
    const ResourceRequest& request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const ResourceLoaderOptions& options) {
  CountUsage(WebFeature::kOffMainThreadFetch);
  WrappedResourceRequest wrapped(request);

  network::mojom::blink::URLLoaderFactoryPtr url_loader_factory;
  if (options.url_loader_factory) {
    options.url_loader_factory->data->Clone(MakeRequest(&url_loader_factory));
  }
  // Resolve any blob: URLs that haven't been resolved yet. The XHR and fetch()
  // API implementations resolve blob URLs earlier because there can be
  // arbitrarily long delays between creating requests with those APIs and
  // actually creating the URL loader here. Other subresource loading will
  // immediately create the URL loader so resolving those blob URLs here is
  // simplest.
  if (request.Url().ProtocolIs("blob") &&
      RuntimeEnabledFeatures::MojoBlobURLsEnabled() && !url_loader_factory) {
    global_scope_->GetPublicURLManager().Resolve(
        request.Url(), MakeRequest(&url_loader_factory));
  }
  if (url_loader_factory) {
    return web_context_
        ->WrapURLLoaderFactory(url_loader_factory.PassInterface().PassHandle())
        ->CreateURLLoader(wrapped, task_runner);
  }

  if (!url_loader_factory_)
    url_loader_factory_ = web_context_->CreateURLLoaderFactory();
  return url_loader_factory_->CreateURLLoader(wrapped, task_runner);
}

bool WorkerFetchContext::IsControlledByServiceWorker() const {
  return web_context_->IsControlledByServiceWorker();
}

int WorkerFetchContext::ApplicationCacheHostID() const {
  return web_context_->ApplicationCacheHostID();
}

void WorkerFetchContext::PrepareRequest(ResourceRequest& request,
                                        RedirectType) {
  String user_agent = global_scope_->UserAgent();
  probe::applyUserAgentOverride(global_scope_, &user_agent);
  DCHECK(!user_agent.IsNull());
  request.SetHTTPUserAgent(AtomicString(user_agent));

  WrappedResourceRequest webreq(request);
  web_context_->WillSendRequest(webreq);
}

void WorkerFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                     FetchResourceType type) {
  BaseFetchContext::AddAdditionalRequestHeaders(request, type);

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (save_data_enabled_)
    request.SetHTTPHeaderField(HTTPNames::Save_Data, "on");
}

void WorkerFetchContext::DispatchWillSendRequest(
    unsigned long identifier,
    ResourceRequest& request,
    const ResourceResponse& redirect_response,
    Resource::Type resource_type,
    const FetchInitiatorInfo& initiator_info) {
  probe::willSendRequest(global_scope_, identifier, nullptr, request,
                         redirect_response, initiator_info, resource_type);
}

void WorkerFetchContext::DispatchDidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    network::mojom::RequestContextFrameType frame_type,
    WebURLRequest::RequestContext request_context,
    Resource* resource,
    ResourceResponseType) {
  if (response.HasMajorCertificateErrors()) {
    WebMixedContentContextType context_type =
        WebMixedContent::ContextTypeFromRequestContext(
            request_context, false /* strictMixedContentCheckingForPlugin */);
    if (context_type == WebMixedContentContextType::kBlockable) {
      web_context_->DidRunContentWithCertificateErrors();
    } else {
      web_context_->DidDisplayContentWithCertificateErrors();
    }
  }
  probe::didReceiveResourceResponse(global_scope_, identifier, nullptr,
                                    response, resource);
}

void WorkerFetchContext::DispatchDidReceiveData(unsigned long identifier,
                                                const char* data,
                                                int data_length) {
  probe::didReceiveData(global_scope_, identifier, nullptr, data, data_length);
}

void WorkerFetchContext::DispatchDidReceiveEncodedData(
    unsigned long identifier,
    int encoded_data_length) {
  probe::didReceiveEncodedDataLength(global_scope_, nullptr, identifier,
                                     encoded_data_length);
}

void WorkerFetchContext::DispatchDidFinishLoading(
    unsigned long identifier,
    TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length,
    bool blocked_cross_site_document) {
  probe::didFinishLoading(global_scope_, identifier, nullptr, finish_time,
                          encoded_data_length, decoded_body_length,
                          blocked_cross_site_document);
}

void WorkerFetchContext::DispatchDidFail(const KURL& url,
                                         unsigned long identifier,
                                         const ResourceError& error,
                                         int64_t encoded_data_length,
                                         bool is_internal_request) {
  probe::didFailLoading(global_scope_, identifier, nullptr, error);
  if (NetworkUtils::IsCertificateTransparencyRequiredError(error.ErrorCode())) {
    CountUsage(WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad);
  }
}

void WorkerFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // TODO(nhiroki): Add ResourceTiming API support once it's spec'ed for
  // worklets.
  if (global_scope_->IsWorkletGlobalScope())
    return;
  WorkerGlobalScopePerformance::performance(*ToWorkerGlobalScope(global_scope_))
      ->GenerateAndAddResourceTiming(info);
}

void WorkerFetchContext::PopulateResourceRequest(
    Resource::Type type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& out_request) {
  SetFirstPartyCookieAndRequestorOrigin(out_request);
}

void WorkerFetchContext::SetFirstPartyCookieAndRequestorOrigin(
    ResourceRequest& out_request) {
  if (out_request.SiteForCookies().IsNull())
    out_request.SetSiteForCookies(GetSiteForCookies());
  if (!out_request.RequestorOrigin())
    out_request.SetRequestorOrigin(GetSecurityOrigin());
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerFetchContext::GetLoadingTaskRunner() {
  return loading_task_runner_;
}

void WorkerFetchContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
  visitor->Trace(subresource_filter_);
  visitor->Trace(resource_fetcher_);
  BaseFetchContext::Trace(visitor);
}

void ProvideWorkerFetchContextToWorker(
    WorkerClients* clients,
    std::unique_ptr<WebWorkerFetchContext> web_context) {
  DCHECK(clients);
  WorkerFetchContextHolder::ProvideTo(
      *clients, new WorkerFetchContextHolder(std::move(web_context)));
}

}  // namespace blink
