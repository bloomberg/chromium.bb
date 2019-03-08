/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/network_hints_interface.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// If kAllowClientHintsToThirdParty is enabled, then device-memory,
// resource-width and viewport-width client hints can be sent to third-party
// origins if the first-party has opted in to receiving client hints.
#if defined(OS_ANDROID)
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Determines FetchCacheMode for |frame|. This FetchCacheMode should be a base
// policy to consider one of each resource belonging to the frame, and should
// not count resource specific conditions in.
mojom::FetchCacheMode DetermineFrameCacheMode(Frame* frame) {
  if (!frame)
    return mojom::FetchCacheMode::kDefault;
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame)
    return DetermineFrameCacheMode(frame->Tree().Parent());

  // Does not propagate cache policy for subresources after the load event.
  // TODO(toyoshim): We should be able to remove following parents' policy check
  // if each frame has a relevant WebFrameLoadType for reload and history
  // navigations.
  if (local_frame->GetDocument()->LoadEventFinished())
    return mojom::FetchCacheMode::kDefault;

  // Respects BypassingCache rather than parent's policy.
  WebFrameLoadType load_type =
      local_frame->Loader().GetDocumentLoader()->LoadType();
  if (load_type == WebFrameLoadType::kReloadBypassingCache)
    return mojom::FetchCacheMode::kBypassCache;

  // Respects parent's policy if it has a special one.
  mojom::FetchCacheMode parent_cache_mode =
      DetermineFrameCacheMode(frame->Tree().Parent());
  if (parent_cache_mode != mojom::FetchCacheMode::kDefault)
    return parent_cache_mode;

  // Otherwise, follows WebFrameLoadType.
  switch (load_type) {
    case WebFrameLoadType::kStandard:
    case WebFrameLoadType::kReplaceCurrentItem:
      return mojom::FetchCacheMode::kDefault;
    case WebFrameLoadType::kBackForward:
      // Mutates the policy for POST requests to avoid form resubmission.
      return mojom::FetchCacheMode::kForceCache;
    case WebFrameLoadType::kReload:
      return mojom::FetchCacheMode::kDefault;
    case WebFrameLoadType::kReloadBypassingCache:
      return mojom::FetchCacheMode::kBypassCache;
  }
  NOTREACHED();
  return mojom::FetchCacheMode::kDefault;
}

}  // namespace

struct FrameFetchContext::FrozenState final
    : GarbageCollectedFinalized<FrozenState> {
  FrozenState(const KURL& url,
              scoped_refptr<const SecurityOrigin> parent_security_origin,
              const ContentSecurityPolicy* content_security_policy,
              KURL site_for_cookies,
              scoped_refptr<const SecurityOrigin> top_frame_origin,
              const ClientHintsPreferences& client_hints_preferences,
              float device_pixel_ratio,
              const String& user_agent,
              const UserAgentMetadata& user_agent_metadata,
              bool is_svg_image_chrome_client)
      : url(url),
        parent_security_origin(std::move(parent_security_origin)),
        content_security_policy(content_security_policy),
        site_for_cookies(site_for_cookies),
        top_frame_origin(std::move(top_frame_origin)),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        user_agent_metadata(user_agent_metadata),
        is_svg_image_chrome_client(is_svg_image_chrome_client) {}

  const KURL url;
  const scoped_refptr<const SecurityOrigin> parent_security_origin;
  const Member<const ContentSecurityPolicy> content_security_policy;
  const KURL site_for_cookies;
  const scoped_refptr<const SecurityOrigin> top_frame_origin;
  const ClientHintsPreferences client_hints_preferences;
  const float device_pixel_ratio;
  const String user_agent;
  const UserAgentMetadata user_agent_metadata;
  const bool is_svg_image_chrome_client;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(content_security_policy);
  }
};

ResourceFetcher* FrameFetchContext::CreateFetcher(
    const FrameResourceFetcherProperties& properties) {
  const FrameOrImportedDocument& frame_or_imported_document =
      properties.GetFrameOrImportedDocument();
  LocalFrame& frame = frame_or_imported_document.GetFrame();
  ResourceFetcherInit init(
      properties,
      MakeGarbageCollected<FrameFetchContext>(frame_or_imported_document),
      frame.GetTaskRunner(TaskType::kNetworking),
      MakeGarbageCollected<LoaderFactoryForFrame>(frame_or_imported_document),
      frame.Console());
  // Frame loading should normally start with |kTight| throttling, as the
  // frame will be in layout-blocking state until the <body> tag is inserted
  init.initial_throttling_policy =
      ResourceLoadScheduler::ThrottlingPolicy::kTight;
  init.frame_scheduler = frame.GetFrameScheduler();
  return MakeGarbageCollected<ResourceFetcher>(init);
}

ResourceFetcher* FrameFetchContext::CreateFetcherForImportedDocument(
    Document* document) {
  DCHECK(document);
  // |document| is detached.
  DCHECK(!document->GetFrame());
  auto& frame_or_imported_document =
      *MakeGarbageCollected<FrameOrImportedDocument>(*document);
  LocalFrame& frame = frame_or_imported_document.GetFrame();
  ResourceFetcherInit init(
      *MakeGarbageCollected<FrameResourceFetcherProperties>(
          frame_or_imported_document),
      MakeGarbageCollected<FrameFetchContext>(frame_or_imported_document),
      document->GetTaskRunner(blink::TaskType::kNetworking),
      MakeGarbageCollected<LoaderFactoryForFrame>(frame_or_imported_document),
      frame.Console());
  init.frame_scheduler = frame.GetFrameScheduler();
  return MakeGarbageCollected<ResourceFetcher>(init);
}

FrameFetchContext::FrameFetchContext(
    const FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document),
      save_data_enabled_(
          GetNetworkStateNotifier().SaveDataEnabled() &&
          !GetFrame()->GetSettings()->GetDataSaverHoldbackWebApi()) {}

KURL FrameFetchContext::GetSiteForCookies() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->site_for_cookies;

  Document* document = frame_or_imported_document_->GetDocument();
  // Use |document| for subresource or nested frame cases,
  // GetFrame()->GetDocument() otherwise.
  if (!document)
    document = GetFrame()->GetDocument();
  return document->SiteForCookies();
}

scoped_refptr<const SecurityOrigin> FrameFetchContext::GetTopFrameOrigin()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->top_frame_origin;

  Document* document = frame_or_imported_document_->GetDocument();
  if (!document)
    document = GetFrame()->GetDocument();
  return document->TopFrameOrigin();
}

SubresourceFilter* FrameFetchContext::GetSubresourceFilter() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DocumentLoader* document_loader = MasterDocumentLoader();
  return document_loader ? document_loader->GetSubresourceFilter() : nullptr;
}

PreviewsResourceLoadingHints*
FrameFetchContext::GetPreviewsResourceLoadingHints() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DocumentLoader* document_loader = MasterDocumentLoader();
  if (!document_loader)
    return nullptr;
  return document_loader->GetPreviewsResourceLoadingHints();
}

LocalFrame* FrameFetchContext::GetFrame() const {
  return &frame_or_imported_document_->GetFrame();
}

LocalFrameClient* FrameFetchContext::GetLocalFrameClient() const {
  return GetFrame()->Client();
}

void FrameFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request) {
  BaseFetchContext::AddAdditionalRequestHeaders(request);

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (GetResourceFetcherProperties().IsDetached())
    return;

  // Reload should reflect the current data saver setting.
  if (IsReloadLoadType(MasterDocumentLoader()->LoadType()))
    request.ClearHTTPHeaderField(http_names::kSaveData);

  if (save_data_enabled_)
    request.SetHTTPHeaderField(http_names::kSaveData, "on");

  if (GetLocalFrameClient()->GetPreviewsStateForFrame() &
      WebURLRequest::kNoScriptOn) {
    request.AddHTTPHeaderField(
        "Intervention",
        "<https://www.chromestatus.com/features/4775088607985664>; "
        "level=\"warning\"");
  }

  if (GetLocalFrameClient()->GetPreviewsStateForFrame() &
      WebURLRequest::kResourceLoadingHintsOn) {
    request.AddHTTPHeaderField(
        "Intervention",
        "<https://www.chromestatus.com/features/4510564810227712>; "
        "level=\"warning\"");
  }

  if (GetLocalFrameClient()->GetPreviewsStateForFrame() &
      WebURLRequest::kClientLoFiOn) {
    request.AddHTTPHeaderField(
        "Intervention",
        "<https://www.chromestatus.com/features/6072546726248448>; "
        "level=\"warning\"");
  }
}

// TODO(toyoshim, arthursonzogni): PlzNavigate doesn't use this function to set
// the ResourceRequest's cache policy. The cache policy determination needs to
// be factored out from FrameFetchContext and moved to the FrameLoader for
// instance.
mojom::FetchCacheMode FrameFetchContext::ResourceRequestCachePolicy(
    const ResourceRequest& request,
    ResourceType type,
    FetchParameters::DeferOption defer) const {
  if (GetResourceFetcherProperties().IsDetached())
    return mojom::FetchCacheMode::kDefault;

  DCHECK(GetFrame());
  const auto cache_mode = DetermineFrameCacheMode(GetFrame());

  // TODO(toyoshim): Revisit to consider if this clause can be merged to
  // determineWebCachePolicy or determineFrameCacheMode.
  if (cache_mode == mojom::FetchCacheMode::kDefault &&
      request.IsConditional()) {
    return mojom::FetchCacheMode::kValidateCache;
  }
  return cache_mode;
}

DocumentLoader* FrameFetchContext::GetDocumentLoader() const {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return frame_or_imported_document_->GetDocumentLoader();
}

inline DocumentLoader* FrameFetchContext::MasterDocumentLoader() const {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return &frame_or_imported_document_->GetMasterDocumentLoader();
}

void FrameFetchContext::DispatchDidChangeResourcePriority(
    unsigned long identifier,
    ResourceLoadPriority load_priority,
    int intra_priority_value) {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  TRACE_EVENT1("devtools.timeline", "ResourceChangePriority", "data",
               inspector_change_resource_priority_event::Data(
                   MasterDocumentLoader(), identifier, load_priority));
  probe::DidChangeResourcePriority(GetFrame(), MasterDocumentLoader(),
                                   identifier, load_priority);
}

void FrameFetchContext::PrepareRequest(
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    RedirectType redirect_type,
    ResourceType resource_type) {
  // TODO(yhirano): Clarify which statements are actually needed when
  // |redirect_type| is |kForRedirect|.

  SetFirstPartyCookie(request);
  request.SetTopFrameOrigin(GetTopFrameOrigin());

  String user_agent = GetUserAgent();
  request.SetHTTPUserAgent(AtomicString(user_agent));

  if (GetResourceFetcherProperties().IsDetached())
    return;
  GetLocalFrameClient()->DispatchWillSendRequest(request);
  FrameScheduler* frame_scheduler = GetFrame()->GetFrameScheduler();
  if (redirect_type == FetchContext::RedirectType::kNotForRedirect &&
      frame_scheduler) {
    virtual_time_pauser = frame_scheduler->CreateWebScopedVirtualTimePauser(
        request.Url().GetString(),
        WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  }

  probe::PrepareRequest(Probe(), MasterDocumentLoader(), request,
                        initiator_info, resource_type);

  // ServiceWorker hook ups.
  if (MasterDocumentLoader()->GetServiceWorkerNetworkProvider()) {
    WrappedResourceRequest webreq(request);
    MasterDocumentLoader()->GetServiceWorkerNetworkProvider()->WillSendRequest(
        webreq);
  }

  // If it's not for redirect, hook up ApplicationCache here too.
  if (redirect_type == FetchContext::RedirectType::kNotForRedirect &&
      GetDocumentLoader() && !GetDocumentLoader()->Fetcher()->Archive() &&
      request.Url().IsValid()) {
    GetDocumentLoader()->GetApplicationCacheHost()->WillStartLoading(request);
  }
}

void FrameFetchContext::DispatchWillSendRequest(
    unsigned long identifier,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    GetFrame()->Loader().Progress().WillStartLoading(identifier,
                                                     request.Priority());
  }
  probe::WillSendRequest(Probe(), identifier, MasterDocumentLoader(), Url(),
                         request, redirect_response, initiator_info,
                         resource_type);
  if (IdlenessDetector* idleness_detector = GetFrame()->GetIdlenessDetector())
    idleness_detector->OnWillSendRequest(MasterDocumentLoader()->Fetcher());
  if (frame_or_imported_document_->GetDocument()) {
    InteractiveDetector* interactive_detector(
        InteractiveDetector::From(*frame_or_imported_document_->GetDocument()));
    if (interactive_detector) {
      interactive_detector->OnResourceLoadBegin(base::nullopt);
    }
  }
}

void FrameFetchContext::DispatchDidReceiveResponse(
    unsigned long identifier,
    const ResourceRequest& request,
    const ResourceResponse& response,
    Resource* resource,
    ResourceResponseType response_type) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  if (GetSubresourceFilter() && resource->GetResourceRequest().IsAdResource())
    GetSubresourceFilter()->ReportAdRequestId(response.RequestId());

  if (response.GetCTPolicyCompliance() ==
      ResourceResponse::kCTPolicyDoesNotComply) {
    CountUsage(
        GetFrame()->IsMainFrame()
            ? WebFeature::
                  kCertificateTransparencyNonCompliantSubresourceInMainFrame
            : WebFeature::
                  kCertificateTransparencyNonCompliantResourceInSubframe);
  }

  if (response_type == ResourceResponseType::kFromMemoryCache) {
    GetLocalFrameClient()->DispatchDidLoadResourceFromMemoryCache(
        resource->GetResourceRequest(), response);

    // Note: probe::WillSendRequest needs to precede before this probe method.
    probe::MarkResourceAsCached(GetFrame(), MasterDocumentLoader(), identifier);
    if (response.IsNull())
      return;
  }

  MixedContentChecker::CheckMixedPrivatePublic(GetFrame(),
                                               response.RemoteIPAddress());

  PreloadHelper::CanLoadResources resource_loading_policy =
      response_type == ResourceResponseType::kFromMemoryCache
          ? PreloadHelper::kDoNotLoadResources
          : PreloadHelper::kLoadResourcesAndPreconnect;
  PreloadHelper::LoadLinksFromHeader(
      response.HttpHeaderField(http_names::kLink), response.CurrentRequestUrl(),
      *GetFrame(), frame_or_imported_document_->GetDocument(),
      NetworkHintsInterfaceImpl(), resource_loading_policy,
      PreloadHelper::kLoadAll, nullptr);

  if (response.HasMajorCertificateErrors() &&
      request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kTopLevel) {
    MixedContentChecker::HandleCertificateError(GetFrame(), response,
                                                request.GetRequestContext());
  }

  if (response.IsLegacyTLSVersion()) {
    UseCounter::Count(frame_or_imported_document_->GetDocument(),
                      WebFeature::kLegacyTLSVersionInSubresource);
    GetLocalFrameClient()->ReportLegacyTLSVersion(response.CurrentRequestUrl());
  }

  GetFrame()->Loader().Progress().IncrementProgress(identifier, response);
  GetLocalFrameClient()->DispatchDidReceiveResponse(response);
  DocumentLoader* document_loader = MasterDocumentLoader();
  probe::DidReceiveResourceResponse(Probe(), identifier, document_loader,
                                    response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  GetFrame()->Console().ReportResourceResponseReceived(document_loader,
                                                       identifier, response);
}

void FrameFetchContext::DispatchDidReceiveData(unsigned long identifier,
                                               const char* data,
                                               uint64_t data_length) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  GetFrame()->Loader().Progress().IncrementProgress(identifier, data_length);
  probe::DidReceiveData(Probe(), identifier, MasterDocumentLoader(), data,
                        data_length);
}

void FrameFetchContext::DispatchDidReceiveEncodedData(
    unsigned long identifier,
    size_t encoded_data_length) {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  probe::DidReceiveEncodedDataLength(Probe(), MasterDocumentLoader(),
                                     identifier, encoded_data_length);
}

void FrameFetchContext::DispatchDidDownloadToBlob(unsigned long identifier,
                                                  BlobDataHandle* blob) {
  if (GetResourceFetcherProperties().IsDetached() || !blob)
    return;

  probe::DidReceiveBlob(Probe(), identifier, MasterDocumentLoader(), blob);
}

void FrameFetchContext::DispatchDidFinishLoading(
    unsigned long identifier,
    TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length,
    bool should_report_corb_blocking,
    ResourceResponseType response_type) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::DidFinishLoading(Probe(), identifier, MasterDocumentLoader(),
                          finish_time, encoded_data_length, decoded_body_length,
                          should_report_corb_blocking);

  Document* document = frame_or_imported_document_->GetDocument();
  if (!document) {
    return;
  }

  if (auto* interactive_detector = InteractiveDetector::From(*document)) {
    interactive_detector->OnResourceLoadEnd(finish_time);
  }

  if (LocalFrame* frame = document->GetFrame()) {
    if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
      idleness_detector->OnDidLoadResource();
    }
  }

  if (response_type == ResourceResponseType::kNotFromMemoryCache) {
    document->CheckCompleted();
  }
}

void FrameFetchContext::DispatchDidFail(const KURL& url,
                                        unsigned long identifier,
                                        const ResourceError& error,
                                        int64_t encoded_data_length,
                                        bool is_internal_request) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  if (DocumentLoader* loader = MasterDocumentLoader()) {
    if (network_utils::IsCertificateTransparencyRequiredError(
            error.ErrorCode())) {
      loader->GetUseCounter().Count(
          WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad,
          GetFrame());
    }
  }

  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::DidFailLoading(Probe(), identifier, MasterDocumentLoader(), error);

  // Notification to FrameConsole should come AFTER InspectorInstrumentation
  // call, DevTools front-end relies on this.
  if (!is_internal_request) {
    GetFrame()->Console().DidFailLoading(MasterDocumentLoader(), identifier,
                                         error);
  }
  Document* document = frame_or_imported_document_->GetDocument();
  if (!document) {
    return;
  }

  if (auto* interactive_detector = InteractiveDetector::From(*document)) {
    // We have not yet recorded load_finish_time. Pass nullopt here; we will
    // call CurrentTimeTicksInSeconds lazily when we need it.
    interactive_detector->OnResourceLoadEnd(base::nullopt);
  }
  if (LocalFrame* frame = document->GetFrame()) {
    if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
      idleness_detector->OnDidLoadResource();
    }
  }
  document->CheckCompleted();
}

void FrameFetchContext::RecordLoadingActivity(
    const ResourceRequest& request,
    ResourceType type,
    const AtomicString& fetch_initiator_name) {
  if (GetResourceFetcherProperties().IsDetached() || !GetDocumentLoader() ||
      GetDocumentLoader()->Fetcher()->Archive() || !request.Url().IsValid())
    return;
  V8DOMActivityLogger* activity_logger = nullptr;
  if (fetch_initiator_name == fetch_initiator_type_names::kXmlhttprequest) {
    activity_logger = V8DOMActivityLogger::CurrentActivityLogger();
  } else {
    activity_logger =
        V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
  }

  if (activity_logger) {
    Vector<String> argv;
    argv.push_back(Resource::ResourceTypeToString(type, fetch_initiator_name));
    argv.push_back(request.Url());
    activity_logger->LogEvent("blinkRequestResource", argv.size(), argv.data());
  }
}

void FrameFetchContext::DidObserveLoadingBehavior(
    WebLoadingBehaviorFlag behavior) {
  if (GetDocumentLoader())
    GetDocumentLoader()->DidObserveLoadingBehavior(behavior);
}

void FrameFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // Normally, |document_| is cleared on Document shutdown. However, Documents
  // for HTML imports will also not have a LocalFrame set: in that case, also
  // early return, as there is nothing to report the resource timing to.
  if (GetResourceFetcherProperties().IsDetached() ||
      !frame_or_imported_document_->GetDocument())
    return;
  LocalFrame* frame = frame_or_imported_document_->GetDocument()->GetFrame();
  if (!frame)
    return;

  // Timing for main resource is handled in DocumentLoader.
  // All other resources are reported to the corresponding Document.
  DOMWindowPerformance::performance(
      *frame_or_imported_document_->GetDocument()->domWindow())
      ->GenerateAndAddResourceTiming(info);
}

bool FrameFetchContext::AllowImage(bool images_enabled, const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return true;
  if (auto* settings_client = GetContentSettingsClient())
    images_enabled = settings_client->AllowImage(images_enabled, url);
  return images_enabled;
}

void FrameFetchContext::ModifyRequestForCSP(ResourceRequest& resource_request) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  // Record the latest requiredCSP value that will be used when sending this
  // request.
  GetFrame()->Loader().RecordLatestRequiredCSP();
  GetFrame()->Loader().ModifyRequestForCSP(
      resource_request, frame_or_imported_document_->GetDocument());
}

void FrameFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  WebEnabledClientHints enabled_hints;

  // If the feature is enabled, then client hints are allowed only on secure
  // URLs.
  if (!ClientHintsPreferences::IsClientHintsAllowed(request.Url()))
    return;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the render side.
  if (!AllowScriptFromSourceWithoutNotifying(request.Url()))
    return;

  // Sec-CH-UA is special: we always send the header to all origins that are
  // eligible for client hints (e.g. secure transport, JavaScript enabled). We
  // alter the header's value based on whether or not the site has opted into
  // additional detail.
  //
  // https://github.com/WICG/ua-client-hints
  blink::UserAgentMetadata ua = GetUserAgentMetadata();
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled()) {
    StringBuilder result;
    result.Append(ua.brand.data());
    const std::string& version =
        ShouldSendClientHint(mojom::WebClientHintsType::kUA, hints_preferences,
                             enabled_hints)
            ? ua.full_version
            : ua.major_version;
    if (!version.empty()) {
      result.Append(' ');
      result.Append(version.data());
    }
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUA)],
        result.ToAtomicString());
  }

  bool is_1p_origin = IsFirstPartyOrigin(request.Url());

  if (!base::FeatureList::IsEnabled(kAllowClientHintsToThirdParty) &&
      !is_1p_origin) {
    // No client hints for 3p origins.
    return;
  }
  // Persisted client hints preferences should be read for only the first
  // party origins.
  if (is_1p_origin && GetContentSettingsClient()) {
    GetContentSettingsClient()->GetAllowedClientHintsFromSource(request.Url(),
                                                                &enabled_hints);
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kDeviceMemory,
                           hints_preferences, enabled_hints)) {
    request.AddHTTPHeaderField(
        "Device-Memory",
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  float dpr = GetDevicePixelRatio();
  if (ShouldSendClientHint(mojom::WebClientHintsType::kDpr, hints_preferences,
                           enabled_hints)) {
    request.AddHTTPHeaderField("DPR", AtomicString(String::Number(dpr)));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kResourceWidth,
                           hints_preferences, enabled_hints)) {
    if (resource_width.is_set) {
      float physical_width = resource_width.width * dpr;
      request.AddHTTPHeaderField(
          "Width", AtomicString(String::Number(ceil(physical_width))));
    }
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kViewportWidth,
                           hints_preferences, enabled_hints) &&
      !GetResourceFetcherProperties().IsDetached() && GetFrame()->View()) {
    request.AddHTTPHeaderField(
        "Viewport-Width",
        AtomicString(String::Number(GetFrame()->View()->ViewportWidth())));
  }

  if (!is_1p_origin) {
    // No network quality client hints for 3p origins. Only DPR, resource width
    // and viewport width client hints are allowed for 1p origins.
    return;
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kRtt, hints_preferences,
                           enabled_hints)) {
    base::Optional<TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    unsigned long rtt =
        GetNetworkStateNotifier().RoundRtt(request.Url().Host(), http_rtt);
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kRtt)],
        AtomicString(String::Number(rtt)));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kDownlink,
                           hints_preferences, enabled_hints)) {
    base::Optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(request.Url().Host(),
                                                      throughput_mbps);
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kDownlink)],
        AtomicString(String::Number(mbps)));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kEct, hints_preferences,
                           enabled_hints)) {
    base::Optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect)
      holdback_ect = GetNetworkStateNotifier().EffectiveType();

    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kEct)],
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kLang, hints_preferences,
                           enabled_hints)) {
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kLang)],
        GetFrame()
            ->DomWindow()
            ->navigator()
            ->SerializeLanguagesForClientHintHeader());
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kUAArch,
                           hints_preferences, enabled_hints)) {
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAArch)],
        AtomicString(ua.architecture.data()));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kUAPlatform,
                           hints_preferences, enabled_hints)) {
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAPlatform)],
        AtomicString(ua.platform.data()));
  }

  if (ShouldSendClientHint(mojom::WebClientHintsType::kUAModel,
                           hints_preferences, enabled_hints)) {
    request.AddHTTPHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAModel)],
        AtomicString(ua.model.data()));
  }
}

void FrameFetchContext::PopulateResourceRequest(
    ResourceType type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(hints_preferences, resource_width, request);

  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
  if (csp && csp->ShouldSendCSPHeader(type))
    request.AddHTTPHeaderField("CSP", "active");
}

void FrameFetchContext::SetFirstPartyCookie(ResourceRequest& request) {
  // Set the first party for cookies url if it has not been set yet (new
  // requests). This value will be updated during redirects, consistent with
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1.1?
  if (request.SiteForCookies().IsNull()) {
    if (request.GetFrameType() ==
        network::mojom::RequestContextFrameType::kTopLevel) {
      request.SetSiteForCookies(request.Url());
    } else {
      request.SetSiteForCookies(GetSiteForCookies());
    }
  }
}

bool FrameFetchContext::AllowScriptFromSource(const KURL& url) const {
  if (AllowScriptFromSourceWithoutNotifying(url))
    return true;
  WebContentSettingsClient* settings_client = GetContentSettingsClient();
  if (settings_client)
    settings_client->DidNotAllowScript();
  return false;
}

bool FrameFetchContext::AllowScriptFromSourceWithoutNotifying(
    const KURL& url) const {
  Settings* settings = GetSettings();
  bool allow_script = !settings || settings->GetScriptEnabled();
  if (auto* settings_client = GetContentSettingsClient())
    allow_script = settings_client->AllowScriptFromSource(allow_script, url);
  return allow_script;
}

bool FrameFetchContext::IsFirstPartyOrigin(const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  return GetFrame()
      ->Tree()
      .Top()
      .GetSecurityContext()
      ->GetSecurityOrigin()
      ->IsSameSchemeHostPort(SecurityOrigin::Create(url).get());
}

bool FrameFetchContext::ShouldBlockRequestByInspector(const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;
  bool should_block_request = false;
  probe::ShouldBlockRequest(Probe(), url, &should_block_request);
  return should_block_request;
}

void FrameFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason,
    ResourceType resource_type) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  probe::DidBlockRequest(Probe(), resource_request, MasterDocumentLoader(),
                         Url(), fetch_initiator_info, blocked_reason,
                         resource_type);
}

bool FrameFetchContext::ShouldBypassMainWorldCSP() const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  return ContentSecurityPolicy::ShouldBypassMainWorld(
      GetFrame()->GetDocument());
}

bool FrameFetchContext::IsSVGImageChromeClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_svg_image_chrome_client;

  return GetFrame()->GetChromeClient().IsSVGImageChromeClient();
}

void FrameFetchContext::CountUsage(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  if (DocumentLoader* loader = MasterDocumentLoader())
    loader->GetUseCounter().Count(feature, GetFrame());
}

void FrameFetchContext::CountDeprecation(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  Deprecation::CountDeprecation(GetFrame(), feature);
}

bool FrameFetchContext::ShouldBlockWebSocketByMixedContentCheck(
    const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  return !MixedContentChecker::IsWebSocketAllowed(*this, GetFrame(), url);
}

std::unique_ptr<WebSocketHandshakeThrottle>
FrameFetchContext::CreateWebSocketHandshakeThrottle() {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return nullptr;
  }
  if (!GetFrame())
    return nullptr;
  return WebFrame::FromFrame(GetFrame())
      ->ToWebLocalFrame()
      ->Client()
      ->CreateWebSocketHandshakeThrottle();
}

bool FrameFetchContext::ShouldBlockFetchByMixedContentCheck(
    mojom::RequestContextType request_context,
    network::mojom::RequestContextFrameType frame_type,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  return MixedContentChecker::ShouldBlockFetch(GetFrame(), request_context,
                                               frame_type, redirect_status, url,
                                               reporting_policy);
}

bool FrameFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  // BlockCredentialedSubresources for main resource has already been checked
  // on the browser-side. It should not be checked a second time here because
  // the renderer-side implementation suffers from https://crbug.com/756846.
  if (resource_request.GetFrameType() !=
      network::mojom::RequestContextFrameType::kNone) {
    return false;
  }

  // URLs with no embedded credentials should load correctly.
  if (url.User().IsEmpty() && url.Pass().IsEmpty())
    return false;

  if (resource_request.GetRequestContext() ==
      mojom::RequestContextType::XML_HTTP_REQUEST) {
    return false;
  }

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  // TODO(mkwst): This doesn't work when the subresource is an iframe.
  // See https://crbug.com/756846.
  if (Url().User() == url.User() && Url().Pass() == url.Pass() &&
      SecurityOrigin::Create(url)->IsSameSchemeHostPort(
          GetResourceFetcherProperties()
              .GetFetchClientSettingsObject()
              .GetSecurityOrigin())) {
    return false;
  }

  CountDeprecation(WebFeature::kRequestedSubresourceWithEmbeddedCredentials);

  // TODO(mkwst): Remove the runtime check one way or the other once we're
  // sure it's going to stick (or that it's not).
  return RuntimeEnabledFeatures::BlockCredentialedSubresourcesEnabled();
}

const KURL& FrameFetchContext::Url() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->url;
  if (!frame_or_imported_document_->GetDocument())
    return NullURL();
  return frame_or_imported_document_->GetDocument()->Url();
}

const SecurityOrigin* FrameFetchContext::GetParentSecurityOrigin() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->parent_security_origin.get();
  Frame* parent = GetFrame()->Tree().Parent();
  if (!parent)
    return nullptr;
  return parent->GetSecurityContext()->GetSecurityOrigin();
}

const ContentSecurityPolicy* FrameFetchContext::GetContentSecurityPolicy()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->content_security_policy;
  Document* document = frame_or_imported_document_->GetDocument();
  return document ? document->GetContentSecurityPolicy() : nullptr;
}

void FrameFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  Document* document = frame_or_imported_document_->GetDocument();
  // Route the console message through Document if it's attached, so
  // that script line numbers can be included. Otherwise, route directly to the
  // FrameConsole, to ensure we never drop a message.
  if (document && document->GetFrame())
    document->AddConsoleMessage(message);
  else
    GetFrame()->Console().AddMessage(message);
}

WebContentSettingsClient* FrameFetchContext::GetContentSettingsClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  return GetFrame()->GetContentSettingsClient();
}

Settings* FrameFetchContext::GetSettings() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DCHECK(GetFrame());
  return GetFrame()->GetSettings();
}

String FrameFetchContext::GetUserAgent() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent;
  return GetFrame()->Loader().UserAgent();
}

UserAgentMetadata FrameFetchContext::GetUserAgentMetadata() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent_metadata;
  return GetLocalFrameClient()->UserAgentMetadata();
}

const ClientHintsPreferences FrameFetchContext::GetClientHintsPreferences()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->client_hints_preferences;

  Document* document = frame_or_imported_document_->GetDocument();
  if (!document || !document->GetFrame())
    return ClientHintsPreferences();

  return document->GetFrame()->GetClientHintsPreferences();
}

float FrameFetchContext::GetDevicePixelRatio() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->device_pixel_ratio;

  Document* document = frame_or_imported_document_->GetDocument();
  if (!document) {
    // Note that this value is not used because the preferences object returned
    // by GetClientHintsPreferences() doesn't allow to use it.
    return 1.0;
  }

  return document->DevicePixelRatio();
}

bool FrameFetchContext::ShouldSendClientHint(
    mojom::WebClientHintsType type,
    const ClientHintsPreferences& hints_preferences,
    const WebEnabledClientHints& enabled_hints) const {
  return GetClientHintsPreferences().ShouldSend(type) ||
         hints_preferences.ShouldSend(type) || enabled_hints.IsEnabled(type);
}

FetchContext* FrameFetchContext::Detach() {
  if (GetResourceFetcherProperties().IsDetached())
    return this;

  if (frame_or_imported_document_->GetDocument()) {
    frozen_state_ = MakeGarbageCollected<FrozenState>(
        Url(), GetParentSecurityOrigin(), GetContentSecurityPolicy(),
        GetSiteForCookies(), GetTopFrameOrigin(), GetClientHintsPreferences(),
        GetDevicePixelRatio(), GetUserAgent(), GetUserAgentMetadata(),
        IsSVGImageChromeClient());
  } else {
    // Some getters are unavailable in this case.
    frozen_state_ = MakeGarbageCollected<FrozenState>(
        NullURL(), GetParentSecurityOrigin(), GetContentSecurityPolicy(),
        GetSiteForCookies(), GetTopFrameOrigin(), GetClientHintsPreferences(),
        GetDevicePixelRatio(), GetUserAgent(), GetUserAgentMetadata(),
        IsSVGImageChromeClient());
  }

  frame_or_imported_document_ = nullptr;
  return this;
}

void FrameFetchContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(frozen_state_);
  BaseFetchContext::Trace(visitor);
}

ResourceLoadPriority FrameFetchContext::ModifyPriorityForExperiments(
    ResourceLoadPriority priority) const {
  if (!GetSettings())
    return priority;

  WebEffectiveConnectionType max_effective_connection_type_threshold =
      GetSettings()->GetLowPriorityIframesThreshold();

  if (max_effective_connection_type_threshold <=
      WebEffectiveConnectionType::kTypeOffline) {
    return priority;
  }

  WebEffectiveConnectionType effective_connection_type =
      GetNetworkStateNotifier().EffectiveType();

  if (effective_connection_type <= WebEffectiveConnectionType::kTypeOffline) {
    return priority;
  }

  if (effective_connection_type > max_effective_connection_type_threshold) {
    // Network is not slow enough.
    return priority;
  }

  if (GetFrame()->IsMainFrame()) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, main_frame_priority_histogram,
                        ("LowPriorityIframes.MainFrameRequestPriority",
                         static_cast<int>(ResourceLoadPriority::kHighest) + 1));
    main_frame_priority_histogram.Count(static_cast<int>(priority));
    return priority;
  }

  DEFINE_STATIC_LOCAL(EnumerationHistogram, iframe_priority_histogram,
                      ("LowPriorityIframes.IframeRequestPriority",
                       static_cast<int>(ResourceLoadPriority::kHighest) + 1));
  iframe_priority_histogram.Count(static_cast<int>(priority));
  // When enabled, the priority of all resources in subframe is dropped.
  // Non-delayable resources are assigned a priority of kLow, and the rest of
  // them are assigned a priority of kLowest. This ensures that if the webpage
  // fetches most of its primary content using iframes, then high priority
  // requests within the iframe go on the network first.
  if (priority >= ResourceLoadPriority::kHigh)
    return ResourceLoadPriority::kLow;
  return ResourceLoadPriority::kLowest;
}

bool FrameFetchContext::CalculateIfAdSubresource(
    const ResourceRequest& resource_request,
    ResourceType type) {
  // Mark the resource as an Ad if the SubresourceFilter thinks it's an ad.
  bool known_ad =
      BaseFetchContext::CalculateIfAdSubresource(resource_request, type);
  if (GetResourceFetcherProperties().IsDetached() ||
      !GetFrame()->GetAdTracker()) {
    return known_ad;
  }

  // The AdTracker needs to know about the request as well, and may also mark it
  // as an ad.
  return GetFrame()->GetAdTracker()->CalculateIfAdSubresource(
      frame_or_imported_document_->GetDocument(), resource_request, type,
      known_ad);
}

void FrameFetchContext::DispatchNetworkQuiet() {
  if (WebServiceWorkerNetworkProvider* service_worker_network_provider =
          MasterDocumentLoader()->GetServiceWorkerNetworkProvider()) {
    service_worker_network_provider->DispatchNetworkQuiet();
  }
}

base::Optional<ResourceRequestBlockedReason> FrameFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  Document* document = GetResourceFetcherProperties().IsDetached()
                           ? nullptr
                           : frame_or_imported_document_->GetDocument();
  if (document && document->IsFreezingInProgress() &&
      !resource_request.GetKeepalive()) {
    AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, mojom::ConsoleMessageLevel::kError,
        "Only fetch keepalive is allowed during onfreeze: " + url.GetString()));
    return ResourceRequestBlockedReason::kOther;
  }
  return BaseFetchContext::CanRequest(type, resource_request, url, options,
                                      reporting_policy, redirect_status);
}

CoreProbeSink* FrameFetchContext::Probe() const {
  return probe::ToCoreProbeSink(GetFrame()->GetDocument());
}

}  // namespace blink
