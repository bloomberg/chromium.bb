// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

WorkerFetchContext::~WorkerFetchContext() = default;

WorkerFetchContext::WorkerFetchContext(
    const DetachableResourceFetcherProperties& properties,
    WorkerOrWorkletGlobalScope& global_scope,
    scoped_refptr<WebWorkerFetchContext> web_context,
    SubresourceFilter* subresource_filter,
    ContentSecurityPolicy& content_security_policy,
    WorkerResourceTimingNotifier& resource_timing_notifier)
    : BaseFetchContext(properties),
      global_scope_(global_scope),
      web_context_(std::move(web_context)),
      subresource_filter_(subresource_filter),
      content_security_policy_(&content_security_policy),
      resource_timing_notifier_(&resource_timing_notifier),
      save_data_enabled_(GetNetworkStateNotifier().SaveDataEnabled()) {
  DCHECK(global_scope.IsContextThread());
  DCHECK(web_context_);
}

net::SiteForCookies WorkerFetchContext::GetSiteForCookies() const {
  return web_context_->SiteForCookies();
}

scoped_refptr<const SecurityOrigin> WorkerFetchContext::GetTopFrameOrigin()
    const {
  base::Optional<WebSecurityOrigin> top_frame_origin =
      web_context_->TopFrameOrigin();

  // The top frame origin of shared and service workers is null.
  if (!top_frame_origin) {
    DCHECK(global_scope_->IsSharedWorkerGlobalScope() ||
           global_scope_->IsServiceWorkerGlobalScope());
    return scoped_refptr<const SecurityOrigin>();
  }

  return *top_frame_origin;
}

SubresourceFilter* WorkerFetchContext::GetSubresourceFilter() const {
  return subresource_filter_.Get();
}

PreviewsResourceLoadingHints*
WorkerFetchContext::GetPreviewsResourceLoadingHints() const {
  return nullptr;
}

bool WorkerFetchContext::AllowScriptFromSource(const KURL& url) const {
  if (!global_scope_->ContentSettingsClient()) {
    return true;
  }
  // If we're on a worker, script should be enabled, so no need to plumb
  // Settings::GetScriptEnabled() here.
  return global_scope_->ContentSettingsClient()->AllowScriptFromSource(true,
                                                                       url);
}

bool WorkerFetchContext::ShouldBlockRequestByInspector(const KURL& url) const {
  bool should_block_request = false;
  probe::ShouldBlockRequest(Probe(), url, &should_block_request);
  return should_block_request;
}

void WorkerFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason,
    ResourceType resource_type) const {
  probe::DidBlockRequest(Probe(), resource_request, nullptr, Url(),
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

CoreProbeSink* WorkerFetchContext::Probe() const {
  return probe::ToCoreProbeSink(static_cast<ExecutionContext*>(global_scope_));
}

bool WorkerFetchContext::ShouldBlockWebSocketByMixedContentCheck(
    const KURL& url) const {
  // Worklets don't support WebSocket.
  DCHECK(global_scope_->IsWorkerGlobalScope());
  return !MixedContentChecker::IsWebSocketAllowed(*this, url);
}

std::unique_ptr<WebSocketHandshakeThrottle>
WorkerFetchContext::CreateWebSocketHandshakeThrottle() {
  return web_context_->CreateWebSocketHandshakeThrottle(
      global_scope_->GetTaskRunner(blink::TaskType::kNetworking));
}

bool WorkerFetchContext::ShouldBlockFetchByMixedContentCheck(
    mojom::RequestContextType request_context,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    ReportingDisposition reporting_disposition,
    const base::Optional<String>& devtools_id) const {
  return MixedContentChecker::ShouldBlockFetchOnWorker(
      *this, request_context, redirect_status, url, reporting_disposition,
      global_scope_->IsWorkletGlobalScope());
}

bool WorkerFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  if ((!url.User().IsEmpty() || !url.Pass().IsEmpty()) &&
      resource_request.GetRequestContext() !=
          mojom::RequestContextType::XML_HTTP_REQUEST) {
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

const KURL& WorkerFetchContext::Url() const {
  return GetResourceFetcherProperties()
      .GetFetchClientSettingsObject()
      .GlobalObjectUrl();
}

const SecurityOrigin* WorkerFetchContext::GetParentSecurityOrigin() const {
  // This method was introduced to check the parent frame's security context
  // while loading iframe document resources. So this method is not suitable for
  // workers.
  NOTREACHED();
  return nullptr;
}

const ContentSecurityPolicy* WorkerFetchContext::GetContentSecurityPolicy()
    const {
  return content_security_policy_;
}

void WorkerFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  return global_scope_->AddConsoleMessage(message);
}

void WorkerFetchContext::PrepareRequest(
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info,
    WebScopedVirtualTimePauser&,
    ResourceType resource_type) {
  String user_agent = global_scope_->UserAgent();
  probe::ApplyUserAgentOverride(Probe(), &user_agent);
  DCHECK(!user_agent.IsNull());
  request.SetHTTPUserAgent(AtomicString(user_agent));

  WrappedResourceRequest webreq(request);
  web_context_->WillSendRequest(webreq);

  probe::PrepareRequest(Probe(), nullptr, request, initiator_info,
                        resource_type);
}

void WorkerFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request) {
  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (save_data_enabled_)
    request.SetHttpHeaderField(http_names::kSaveData, "on");
}

void WorkerFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // TODO(nhiroki): Add ResourceTiming API support once it's spec'ed for
  // worklets.
  if (global_scope_->IsWorkletGlobalScope())
    return;
  const SecurityOrigin* security_origin = GetResourceFetcherProperties()
                                              .GetFetchClientSettingsObject()
                                              .GetSecurityOrigin();
  mojom::blink::ResourceTimingInfoPtr mojo_info =
      Performance::GenerateResourceTiming(*security_origin, info,
                                          *global_scope_);
  // |info| is taken const-ref but this can make destructive changes to
  // WorkerTimingContainer on |info| when a page is controlled by a service
  // worker.
  resource_timing_notifier_->AddResourceTiming(std::move(mojo_info),
                                               info.InitiatorType(),
                                               info.TakeWorkerTimingReceiver());
}

void WorkerFetchContext::PopulateResourceRequest(
    ResourceType type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& out_request) {
  if (!GetResourceFetcherProperties().IsDetached())
    probe::SetDevToolsIds(Probe(), out_request);
  MixedContentChecker::UpgradeInsecureRequest(
      out_request,
      &GetResourceFetcherProperties().GetFetchClientSettingsObject(),
      global_scope_, mojom::RequestContextFrameType::kNone,
      global_scope_->ContentSettingsClient());
  SetFirstPartyCookie(out_request);
  if (!out_request.TopFrameOrigin())
    out_request.SetTopFrameOrigin(GetTopFrameOrigin());
}

mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
WorkerFetchContext::TakePendingWorkerTimingReceiver(int request_id) {
  mojo::ScopedMessagePipeHandle pipe =
      GetWebWorkerFetchContext()->TakePendingWorkerTimingReceiver(request_id);
  return mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>(
      std::move(pipe));
}

void WorkerFetchContext::SetFirstPartyCookie(ResourceRequest& out_request) {
  if (out_request.SiteForCookies().IsNull())
    out_request.SetSiteForCookies(GetSiteForCookies());
}

WorkerSettings* WorkerFetchContext::GetWorkerSettings() const {
  auto* scope = DynamicTo<WorkerGlobalScope>(*global_scope_);
  return scope ? scope->GetWorkerSettings() : nullptr;
}

bool WorkerFetchContext::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const KURL& url) const {
  if (!global_scope_->ContentSettingsClient())
    return enabled_per_settings;
  return global_scope_->ContentSettingsClient()->AllowRunningInsecureContent(
      enabled_per_settings, url);
}

void WorkerFetchContext::Trace(Visitor* visitor) {
  visitor->Trace(global_scope_);
  visitor->Trace(subresource_filter_);
  visitor->Trace(content_security_policy_);
  BaseFetchContext::Trace(visitor);
}

}  // namespace blink
