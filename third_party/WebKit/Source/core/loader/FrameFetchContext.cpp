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

#include "core/loader/FrameFetchContext.h"

#include <algorithm>
#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/Document.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/NetworkQuietDetector.h"
#include "core/loader/PingLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/SubresourceFilter.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/private/FrameClientHintsPreferencesContext.h"
#include "core/page/Page.h"
#include "core/paint/FirstMeaningfulPaintDetector.h"
#include "core/probe/CoreProbes.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceBase.h"
#include "platform/WebFrameScheduler.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"

namespace blink {

namespace {

enum class RequestMethod { kIsPost, kIsNotPost };
enum class RequestType { kIsConditional, kIsNotConditional };
enum class ResourceType { kIsMainResource, kIsNotMainResource };

// Determines WebCachePolicy for a main resource, or WebCachePolicy that is
// corresponding to FrameLoadType.
// TODO(toyoshim): Probably, we should split FrameLoadType to WebCachePolicy
// conversion logic into a separate function.
WebCachePolicy DetermineWebCachePolicy(RequestMethod method,
                                       RequestType request_type,
                                       ResourceType resource_type,
                                       FrameLoadType load_type) {
  switch (load_type) {
    case kFrameLoadTypeStandard:
    case kFrameLoadTypeReplaceCurrentItem:
    case kFrameLoadTypeInitialInChildFrame:
      return (request_type == RequestType::kIsConditional ||
              method == RequestMethod::kIsPost)
                 ? WebCachePolicy::kValidatingCacheData
                 : WebCachePolicy::kUseProtocolCachePolicy;
    case kFrameLoadTypeBackForward:
    case kFrameLoadTypeInitialHistoryLoad:
      // Mutates the policy for POST requests to avoid form resubmission.
      return method == RequestMethod::kIsPost
                 ? WebCachePolicy::kReturnCacheDataDontLoad
                 : WebCachePolicy::kReturnCacheDataElseLoad;
    case kFrameLoadTypeReload:
      return resource_type == ResourceType::kIsMainResource
                 ? WebCachePolicy::kValidatingCacheData
                 : WebCachePolicy::kUseProtocolCachePolicy;
    case kFrameLoadTypeReloadBypassingCache:
      return WebCachePolicy::kBypassingCache;
  }
  NOTREACHED();
  return WebCachePolicy::kUseProtocolCachePolicy;
}

// Determines WebCachePolicy for |frame|. This WebCachePolicy should be a base
// policy to consider one of each resource belonging to the frame, and should
// not count resource specific conditions in.
// TODO(toyoshim): Remove |resourceType| to realize the design described above.
// See also comments in resourceRequestCachePolicy().
WebCachePolicy DetermineFrameWebCachePolicy(Frame* frame,
                                            ResourceType resource_type) {
  if (!frame)
    return WebCachePolicy::kUseProtocolCachePolicy;
  if (!frame->IsLocalFrame())
    return DetermineFrameWebCachePolicy(frame->Tree().Parent(), resource_type);

  // Does not propagate cache policy for subresources after the load event.
  // TODO(toyoshim): We should be able to remove following parents' policy check
  // if each frame has a relevant FrameLoadType for reload and history
  // navigations.
  if (resource_type == ResourceType::kIsNotMainResource &&
      ToLocalFrame(frame)->GetDocument()->LoadEventFinished()) {
    return WebCachePolicy::kUseProtocolCachePolicy;
  }

  // Respects BypassingCache rather than parent's policy.
  FrameLoadType load_type =
      ToLocalFrame(frame)->Loader().GetDocumentLoader()->LoadType();
  if (load_type == kFrameLoadTypeReloadBypassingCache)
    return WebCachePolicy::kBypassingCache;

  // Respects parent's policy if it has a special one.
  WebCachePolicy parent_policy =
      DetermineFrameWebCachePolicy(frame->Tree().Parent(), resource_type);
  if (parent_policy != WebCachePolicy::kUseProtocolCachePolicy)
    return parent_policy;

  // Otherwise, follows FrameLoadType. Use kIsNotPost, kIsNotConditional, and
  // kIsNotMainResource to obtain a representative policy for the frame.
  return DetermineWebCachePolicy(RequestMethod::kIsNotPost,
                                 RequestType::kIsNotConditional,
                                 ResourceType::kIsNotMainResource, load_type);
}

}  // namespace

struct FrameFetchContext::FrozenState final
    : GarbageCollectedFinalized<FrozenState> {
  FrozenState(ReferrerPolicy referrer_policy,
              const String& outgoing_referrer,
              const KURL& url,
              RefPtr<SecurityOrigin> security_origin,
              RefPtr<const SecurityOrigin> parent_security_origin,
              const Optional<WebAddressSpace>& address_space,
              const ContentSecurityPolicy* content_security_policy,
              KURL first_party_for_cookies,
              RefPtr<SecurityOrigin> requestor_origin,
              RefPtr<SecurityOrigin> requestor_origin_for_frame_loading,
              const ClientHintsPreferences& client_hints_preferences,
              float device_pixel_ratio,
              const String& user_agent,
              bool is_main_frame,
              bool is_svg_image_chrome_client)
      : referrer_policy(referrer_policy),
        outgoing_referrer(outgoing_referrer),
        url(url),
        security_origin(std::move(security_origin)),
        parent_security_origin(std::move(parent_security_origin)),
        address_space(address_space),
        content_security_policy(content_security_policy),
        first_party_for_cookies(first_party_for_cookies),
        requestor_origin(requestor_origin),
        requestor_origin_for_frame_loading(requestor_origin_for_frame_loading),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        is_main_frame(is_main_frame),
        is_svg_image_chrome_client(is_svg_image_chrome_client) {}

  const ReferrerPolicy referrer_policy;
  const String outgoing_referrer;
  const KURL url;
  const RefPtr<SecurityOrigin> security_origin;
  const RefPtr<const SecurityOrigin> parent_security_origin;
  const Optional<WebAddressSpace> address_space;
  const Member<const ContentSecurityPolicy> content_security_policy;
  const KURL first_party_for_cookies;
  const RefPtr<SecurityOrigin> requestor_origin;
  const RefPtr<SecurityOrigin> requestor_origin_for_frame_loading;
  const ClientHintsPreferences client_hints_preferences;
  const float device_pixel_ratio;
  const String user_agent;
  const bool is_main_frame;
  const bool is_svg_image_chrome_client;

  DEFINE_INLINE_TRACE() { visitor->Trace(content_security_policy); }
};

FrameFetchContext::FrameFetchContext(DocumentLoader* loader, Document* document)
    : document_loader_(loader), document_(document) {
  DCHECK(GetFrame());
}

void FrameFetchContext::ProvideDocumentToContext(FetchContext& context,
                                                 Document* document) {
  DCHECK(document);
  CHECK(context.IsFrameFetchContext());
  static_cast<FrameFetchContext&>(context).document_ = document;
}

FrameFetchContext::~FrameFetchContext() {
  document_loader_ = nullptr;
}

LocalFrame* FrameFetchContext::FrameOfImportsController() const {
  DCHECK(document_);
  DCHECK(!IsDetached());

  // It's guaranteed that imports_controller is not nullptr since:
  // - only ClearImportsController() clears it
  // - ClearImportsController() also calls ClearContext() on this
  //   FrameFetchContext() making IsDetached() return false
  HTMLImportsController* imports_controller = document_->ImportsController();
  DCHECK(imports_controller);

  // It's guaranteed that Master() is not yet Shutdown()-ed since when Master()
  // is Shutdown()-ed:
  // - Master()'s HTMLImportsController is disposed.
  // - All the HTMLImportLoader instances of the HTMLImportsController are
  //   disposed.
  // - ClearImportsController() is called on the Document of the
  //   HTMLImportLoader to detach this context which makes IsDetached() return
  //   true.
  // HTMLImportsController is created only when the master Document's
  // GetFrame() doesn't return nullptr, this is guaranteed to be not nullptr
  // here.
  LocalFrame* frame = imports_controller->Master()->GetFrame();
  DCHECK(frame);
  return frame;
}

RefPtr<WebTaskRunner> FrameFetchContext::GetTaskRunner() const {
  return GetFrame()->FrameScheduler()->LoadingTaskRunner();
}

WebFrameScheduler* FrameFetchContext::GetFrameScheduler() {
  if (IsDetached())
    return nullptr;
  return GetFrame()->FrameScheduler();
}

KURL FrameFetchContext::GetFirstPartyForCookies() const {
  if (IsDetached())
    return frozen_state_->first_party_for_cookies;

  // Use document_ for subresource or nested frame cases,
  // GetFrame()->GetDocument() otherwise.
  Document* document = document_ ? document_.Get() : GetFrame()->GetDocument();
  return document->FirstPartyForCookies();
}

LocalFrame* FrameFetchContext::GetFrame() const {
  DCHECK(!IsDetached());

  if (!document_loader_)
    return FrameOfImportsController();

  LocalFrame* frame = document_loader_->GetFrame();
  DCHECK(frame);
  return frame;
}

LocalFrameClient* FrameFetchContext::GetLocalFrameClient() const {
  return GetFrame()->Client();
}

void FrameFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                    FetchResourceType type) {
  BaseFetchContext::AddAdditionalRequestHeaders(request, type);

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (IsDetached())
    return;

  // Reload should reflect the current data saver setting.
  if (IsReloadLoadType(MasterDocumentLoader()->LoadType()))
    request.ClearHTTPHeaderField("Save-Data");

  if (GetSettings() && GetSettings()->GetDataSaverEnabled())
    request.SetHTTPHeaderField("Save-Data", "on");

  if (GetLocalFrameClient()->IsClientLoFiActiveForFrame()) {
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
WebCachePolicy FrameFetchContext::ResourceRequestCachePolicy(
    const ResourceRequest& request,
    Resource::Type type,
    FetchParameters::DeferOption defer) const {
  if (IsDetached())
    return WebCachePolicy::kUseProtocolCachePolicy;

  DCHECK(GetFrame());
  if (type == Resource::kMainResource) {
    const WebCachePolicy cache_policy = DetermineWebCachePolicy(
        request.HttpMethod() == "POST" ? RequestMethod::kIsPost
                                       : RequestMethod::kIsNotPost,
        request.IsConditional() ? RequestType::kIsConditional
                                : RequestType::kIsNotConditional,
        ResourceType::kIsMainResource, MasterDocumentLoader()->LoadType());
    // Follows the parent frame's policy.
    // TODO(toyoshim): Probably, FrameLoadType for each frame should have a
    // right type for reload or history navigations, and should not need to
    // check parent's frame policy here. Once it has a right FrameLoadType,
    // we can remove Resource::Type argument from determineFrameWebCachePolicy.
    // See also crbug.com/332602.
    if (cache_policy != WebCachePolicy::kUseProtocolCachePolicy)
      return cache_policy;
    return DetermineFrameWebCachePolicy(GetFrame()->Tree().Parent(),
                                        ResourceType::kIsMainResource);
  }

  const WebCachePolicy cache_policy = DetermineFrameWebCachePolicy(
      GetFrame(), ResourceType::kIsNotMainResource);

  // TODO(toyoshim): Revisit to consider if this clause can be merged to
  // determineWebCachePolicy or determineFrameWebCachePolicy.
  if (cache_policy == WebCachePolicy::kUseProtocolCachePolicy &&
      request.IsConditional()) {
    return WebCachePolicy::kValidatingCacheData;
  }
  return cache_policy;
}

inline DocumentLoader* FrameFetchContext::MasterDocumentLoader() const {
  DCHECK(!IsDetached());

  if (document_loader_)
    return document_loader_.Get();

  // GetDocumentLoader() here always returns a non-nullptr value that is the
  // DocumentLoader for |document_| because:
  // - A Document is created with a LocalFrame only after the
  //   DocumentLoader is committed
  // - When another DocumentLoader is committed, the FrameLoader
  //   Shutdown()-s |document_| making IsDetached() return false
  return FrameOfImportsController()->Loader().GetDocumentLoader();
}

void FrameFetchContext::DispatchDidChangeResourcePriority(
    unsigned long identifier,
    ResourceLoadPriority load_priority,
    int intra_priority_value) {
  if (IsDetached())
    return;
  TRACE_EVENT1(
      "devtools.timeline", "ResourceChangePriority", "data",
      InspectorChangeResourcePriorityEvent::Data(identifier, load_priority));
  probe::didChangeResourcePriority(GetFrame(), identifier, load_priority);
}

void FrameFetchContext::PrepareRequest(ResourceRequest& request,
                                       RedirectType redirect_type) {
  SetFirstPartyCookieAndRequestorOrigin(request);

  String user_agent = GetUserAgent();
  request.SetHTTPUserAgent(AtomicString(user_agent));

  if (IsDetached())
    return;
  GetLocalFrameClient()->DispatchWillSendRequest(request);

  // ServiceWorker hook ups.
  if (MasterDocumentLoader()->GetServiceWorkerNetworkProvider()) {
    WrappedResourceRequest webreq(request);
    MasterDocumentLoader()->GetServiceWorkerNetworkProvider()->WillSendRequest(
        webreq);
  }

  // If it's not for redirect, hook up ApplicationCache here too.
  if (redirect_type == FetchContext::RedirectType::kNotForRedirect &&
      document_loader_ && !document_loader_->Fetcher()->Archive() &&
      request.Url().IsValid()) {
    document_loader_->GetApplicationCacheHost()->WillStartLoading(request);
  }
}

void FrameFetchContext::DispatchWillSendRequest(
    unsigned long identifier,
    ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info) {
  if (IsDetached())
    return;

  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    GetFrame()->Loader().Progress().WillStartLoading(identifier,
                                                     request.Priority());
  }
  probe::willSendRequest(GetFrame()->GetDocument(), identifier,
                         MasterDocumentLoader(), request, redirect_response,
                         initiator_info);
  if (GetFrame()->FrameScheduler())
    GetFrame()->FrameScheduler()->DidStartLoading(identifier);
}

void FrameFetchContext::DispatchDidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    Resource* resource,
    ResourceResponseType response_type) {
  if (IsDetached())
    return;

  ParseAndPersistClientHints(response);

  if (response_type == ResourceResponseType::kFromMemoryCache) {
    // Note: probe::willSendRequest needs to precede before this probe method.
    probe::markResourceAsCached(GetFrame(), identifier);
    if (response.IsNull())
      return;
  }

  MixedContentChecker::CheckMixedPrivatePublic(GetFrame(),
                                               response.RemoteIPAddress());
  LinkLoader::CanLoadResources resource_loading_policy =
      response_type == ResourceResponseType::kFromMemoryCache
          ? LinkLoader::kDoNotLoadResources
          : LinkLoader::kLoadResourcesAndPreconnect;
  if (document_loader_ &&
      document_loader_ ==
          document_loader_->GetFrame()->Loader().ProvisionalDocumentLoader()) {
    FrameClientHintsPreferencesContext hints_context(GetFrame());
    document_loader_->GetClientHintsPreferences()
        .UpdateFromAcceptClientHintsHeader(
            response.HttpHeaderField(HTTPNames::Accept_CH), &hints_context);
    // When response is received with a provisional docloader, the resource
    // haven't committed yet, and we cannot load resources, only preconnect.
    resource_loading_policy = LinkLoader::kDoNotLoadResources;
  }
  LinkLoader::LoadLinksFromHeader(
      response.HttpHeaderField(HTTPNames::Link), response.Url(), *GetFrame(),
      document_, NetworkHintsInterfaceImpl(), resource_loading_policy,
      LinkLoader::kLoadAll, nullptr);

  if (response.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(GetFrame(), response,
                                                frame_type, request_context);
  }

  GetFrame()->Loader().Progress().IncrementProgress(identifier, response);
  GetLocalFrameClient()->DispatchDidReceiveResponse(response);
  DocumentLoader* document_loader = MasterDocumentLoader();
  probe::didReceiveResourceResponse(GetFrame()->GetDocument(), identifier,
                                    document_loader, response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  GetFrame()->Console().ReportResourceResponseReceived(document_loader,
                                                       identifier, response);
}

void FrameFetchContext::DispatchDidReceiveData(unsigned long identifier,
                                               const char* data,
                                               int data_length) {
  if (IsDetached())
    return;

  GetFrame()->Loader().Progress().IncrementProgress(identifier, data_length);
  probe::didReceiveData(GetFrame()->GetDocument(), identifier,
                        MasterDocumentLoader(), data, data_length);
}

void FrameFetchContext::DispatchDidReceiveEncodedData(unsigned long identifier,
                                                      int encoded_data_length) {
  if (IsDetached())
    return;
  probe::didReceiveEncodedDataLength(GetFrame()->GetDocument(), identifier,
                                     encoded_data_length);
}

void FrameFetchContext::DispatchDidDownloadData(unsigned long identifier,
                                                int data_length,
                                                int encoded_data_length) {
  if (IsDetached())
    return;

  GetFrame()->Loader().Progress().IncrementProgress(identifier, data_length);
  probe::didReceiveData(GetFrame()->GetDocument(), identifier,
                        MasterDocumentLoader(), 0, data_length);
  probe::didReceiveEncodedDataLength(GetFrame()->GetDocument(), identifier,
                                     encoded_data_length);
}

void FrameFetchContext::DispatchDidFinishLoading(unsigned long identifier,
                                                 double finish_time,
                                                 int64_t encoded_data_length,
                                                 int64_t decoded_body_length) {
  if (IsDetached())
    return;

  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::didFinishLoading(GetFrame()->GetDocument(), identifier,
                          MasterDocumentLoader(), finish_time,
                          encoded_data_length, decoded_body_length);
  if (GetFrame()->FrameScheduler())
    GetFrame()->FrameScheduler()->DidStopLoading(identifier);
}

void FrameFetchContext::DispatchDidFail(unsigned long identifier,
                                        const ResourceError& error,
                                        int64_t encoded_data_length,
                                        bool is_internal_request) {
  if (IsDetached())
    return;

  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::didFailLoading(GetFrame()->GetDocument(), identifier, error);
  // Notification to FrameConsole should come AFTER InspectorInstrumentation
  // call, DevTools front-end relies on this.
  if (!is_internal_request)
    GetFrame()->Console().DidFailLoading(identifier, error);
  if (GetFrame()->FrameScheduler())
    GetFrame()->FrameScheduler()->DidStopLoading(identifier);
}

void FrameFetchContext::DispatchDidLoadResourceFromMemoryCache(
    unsigned long identifier,
    const ResourceRequest& resource_request,
    const ResourceResponse& resource_response) {
  if (IsDetached())
    return;

  GetLocalFrameClient()->DispatchDidLoadResourceFromMemoryCache(
      resource_request, resource_response);
}

bool FrameFetchContext::ShouldLoadNewResource(Resource::Type type) const {
  if (!document_loader_)
    return true;

  if (IsDetached())
    return false;

  FrameLoader& loader = document_loader_->GetFrame()->Loader();
  if (type == Resource::kMainResource)
    return document_loader_ == loader.ProvisionalDocumentLoader();
  return document_loader_ == loader.GetDocumentLoader();
}

static std::unique_ptr<TracedValue>
LoadResourceTraceData(unsigned long identifier, const KURL& url, int priority) {
  String request_id = IdentifiersFactory::RequestId(identifier);

  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetString("requestId", request_id);
  value->SetString("url", url.GetString());
  value->SetInteger("priority", priority);
  return value;
}

void FrameFetchContext::RecordLoadingActivity(
    unsigned long identifier,
    const ResourceRequest& request,
    Resource::Type type,
    const AtomicString& fetch_initiator_name) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "blink.net", "Resource", identifier, "data",
      LoadResourceTraceData(identifier, request.Url(), request.Priority()));
  if (!document_loader_ || document_loader_->Fetcher()->Archive() ||
      !request.Url().IsValid())
    return;
  V8DOMActivityLogger* activity_logger = nullptr;
  if (fetch_initiator_name == FetchInitiatorTypeNames::xmlhttprequest) {
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

void FrameFetchContext::DidLoadResource(Resource* resource) {
  if (!document_)
    return;
  FirstMeaningfulPaintDetector::From(*document_).CheckNetworkStable();
  NetworkQuietDetector::From(*document_).CheckNetworkStable();

  if (resource->IsLoadEventBlockingResourceType())
    document_->CheckCompleted();
}

void FrameFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  Document* initiator_document = document_ && info.IsMainResource()
                                     ? document_->ParentDocument()
                                     : document_.Get();
  if (!initiator_document || !initiator_document->domWindow())
    return;
  DOMWindowPerformance::performance(*initiator_document->domWindow())
      ->AddResourceTiming(info);
}

bool FrameFetchContext::AllowImage(bool images_enabled, const KURL& url) const {
  if (IsDetached())
    return true;

  return GetContentSettingsClient()->AllowImage(images_enabled, url);
}

bool FrameFetchContext::IsControlledByServiceWorker() const {
  if (IsDetached())
    return false;

  DCHECK(MasterDocumentLoader());

  // Service workers are bypassed by suborigins (see
  // https://w3c.github.io/webappsec-suborigins/). Since service worker
  // controllers are assigned based on physical origin, without knowledge of
  // whether the context is in a suborigin, it is necessary to explicitly bypass
  // service workers on a per-request basis. Additionally, it is necessary to
  // explicitly return |false| here so that it is clear that the SW will be
  // bypassed. In particular, this is important for
  // ResourceFetcher::getCacheIdentifier(), which will return the SW's cache if
  // the context's isControlledByServiceWorker() returns |true|, and thus will
  // returned cached resources from the service worker. That would have the
  // effect of not bypassing the SW.
  if (GetSecurityOrigin() && GetSecurityOrigin()->HasSuborigin())
    return false;

  auto* service_worker_network_provider =
      MasterDocumentLoader()->GetServiceWorkerNetworkProvider();
  return service_worker_network_provider &&
         service_worker_network_provider->IsControlledByServiceWorker();
}

int64_t FrameFetchContext::ServiceWorkerID() const {
  DCHECK(IsControlledByServiceWorker());
  DCHECK(MasterDocumentLoader());
  auto* service_worker_network_provider =
      MasterDocumentLoader()->GetServiceWorkerNetworkProvider();
  return service_worker_network_provider
             ? service_worker_network_provider->ServiceWorkerID()
             : -1;
}

int FrameFetchContext::ApplicationCacheHostID() const {
  if (!document_loader_)
    return WebApplicationCacheHost::kAppCacheNoHostId;
  return document_loader_->GetApplicationCacheHost()->GetHostID();
}

bool FrameFetchContext::IsMainFrame() const {
  if (IsDetached())
    return frozen_state_->is_main_frame;
  return GetFrame()->IsMainFrame();
}

bool FrameFetchContext::DefersLoading() const {
  return IsDetached() ? false : GetFrame()->GetPage()->Suspended();
}

bool FrameFetchContext::IsLoadComplete() const {
  if (IsDetached())
    return true;

  return document_ && document_->LoadEventFinished();
}

bool FrameFetchContext::PageDismissalEventBeingDispatched() const {
  return document_ && document_->PageDismissalEventBeingDispatched() !=
                          Document::kNoDismissal;
}

bool FrameFetchContext::UpdateTimingInfoForIFrameNavigation(
    ResourceTimingInfo* info) {
  if (IsDetached())
    return false;

  // <iframe>s should report the initial navigation requested by the parent
  // document, but not subsequent navigations.
  // FIXME: Resource timing is broken when the parent is a remote frame.
  if (!GetFrame()->DeprecatedLocalOwner() ||
      GetFrame()->DeprecatedLocalOwner()->LoadedNonEmptyDocument())
    return false;
  GetFrame()->DeprecatedLocalOwner()->DidLoadNonEmptyDocument();
  // Do not report iframe navigation that restored from history, since its
  // location may have been changed after initial navigation.
  if (MasterDocumentLoader()->LoadType() == kFrameLoadTypeInitialHistoryLoad)
    return false;
  info->SetInitiatorType(GetFrame()->DeprecatedLocalOwner()->localName());
  return true;
}

void FrameFetchContext::SendImagePing(const KURL& url) {
  if (IsDetached())
    return;
  PingLoader::LoadImage(GetFrame(), url);
}

void FrameFetchContext::AddConsoleMessage(const String& message,
                                          LogMessageType message_type) const {
  if (IsDetached())
    return;

  MessageLevel level = message_type == kLogWarningMessage ? kWarningMessageLevel
                                                          : kErrorMessageLevel;
  ConsoleMessage* console_message =
      ConsoleMessage::Create(kJSMessageSource, level, message);
  // Route the console message through Document if it's attached, so
  // that script line numbers can be included. Otherwise, route directly to the
  // FrameConsole, to ensure we never drop a message.
  if (document_ && document_->GetFrame())
    document_->AddConsoleMessage(console_message);
  else
    GetFrame()->Console().AddMessage(console_message);
}

SecurityOrigin* FrameFetchContext::GetSecurityOrigin() const {
  if (IsDetached())
    return frozen_state_->security_origin.Get();
  return document_ ? document_->GetSecurityOrigin() : nullptr;
}

void FrameFetchContext::ModifyRequestForCSP(ResourceRequest& resource_request) {
  if (IsDetached())
    return;

  // Record the latest requiredCSP value that will be used when sending this
  // request.
  GetFrame()->Loader().RecordLatestRequiredCSP();
  GetFrame()->Loader().ModifyRequestForCSP(resource_request, document_);
}

float FrameFetchContext::ClientHintsDeviceMemory(int64_t physical_memory_mb) {
  // The calculations in this method are described in the specifcations:
  // https://github.com/WICG/device-memory.
  DCHECK_GT(physical_memory_mb, 0);
  int lower_bound = physical_memory_mb;
  int power = 0;

  // Extract the most significant 2-bits and their location.
  while (lower_bound >= 4) {
    lower_bound >>= 1;
    power++;
  }
  // The lower_bound value is either 0b10 or 0b11.
  DCHECK(lower_bound & 2);

  int64_t upper_bound = lower_bound + 1;
  lower_bound = lower_bound << power;
  upper_bound = upper_bound << power;

  // Find the closest bound, and convert it to GB.
  if (physical_memory_mb - lower_bound <= upper_bound - physical_memory_mb)
    return static_cast<float>(lower_bound) / 1024.0;
  return static_cast<float>(upper_bound) / 1024.0;
}

void FrameFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  if (!RuntimeEnabledFeatures::ClientHintsEnabled())
    return;

  if (ShouldSendClientHint(kWebClientHintsTypeDeviceMemory,
                           hints_preferences)) {
    int64_t physical_memory = MemoryCoordinator::GetPhysicalMemoryMB();
    request.AddHTTPHeaderField(
        "Device-Memory",
        AtomicString(String::Number(ClientHintsDeviceMemory(physical_memory))));
  }

  float dpr = GetDevicePixelRatio();
  if (ShouldSendClientHint(kWebClientHintsTypeDpr, hints_preferences))
    request.AddHTTPHeaderField("DPR", AtomicString(String::Number(dpr)));

  if (ShouldSendClientHint(kWebClientHintsTypeResourceWidth,
                           hints_preferences)) {
    if (resource_width.is_set) {
      float physical_width = resource_width.width * dpr;
      request.AddHTTPHeaderField(
          "Width", AtomicString(String::Number(ceil(physical_width))));
    }
  }

  if (ShouldSendClientHint(kWebClientHintsTypeViewportWidth,
                           hints_preferences) &&
      !IsDetached() && GetFrame()->View()) {
    request.AddHTTPHeaderField(
        "Viewport-Width",
        AtomicString(String::Number(GetFrame()->View()->ViewportWidth())));
  }
}

void FrameFetchContext::PopulateResourceRequest(
    Resource::Type type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(hints_preferences, resource_width, request);
  AddCSPHeaderIfNecessary(type, request);
}

void FrameFetchContext::SetFirstPartyCookieAndRequestorOrigin(
    ResourceRequest& request) {
  // Set the first party for cookies url if it has not been set yet (new
  // requests). This value will be updated during redirects, consistent with
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1.1?
  if (request.FirstPartyForCookies().IsNull()) {
    if (request.GetFrameType() == WebURLRequest::kFrameTypeTopLevel) {
      request.SetFirstPartyForCookies(request.Url());
    } else {
      request.SetFirstPartyForCookies(GetFirstPartyForCookies());
    }
  }

  // Subresource requests inherit their requestor origin from |document_|
  // directly.  Top-level frame types are taken care of in 'FrameLoadRequest()'.
  // Auxiliary frame types in 'CreateWindow()' and 'FrameLoader::Load'.
  if (!request.RequestorOrigin()) {
    if (request.GetFrameType() == WebURLRequest::kFrameTypeNone) {
      request.SetRequestorOrigin(GetRequestorOrigin());
    } else {
      // Set the requestor origin to the same origin as the frame's document
      // if it hasn't yet been set. (We may hit here for nested frames and
      // redirect cases)
      request.SetRequestorOrigin(GetRequestorOriginForFrameLoading());
    }
  }
}

MHTMLArchive* FrameFetchContext::Archive() const {
  DCHECK(!IsMainFrame());
  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in multiple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  if (IsDetached() || !GetFrame()->Tree().Parent()->IsLocalFrame())
    return nullptr;
  return ToLocalFrame(GetFrame()->Tree().Parent())
      ->Loader()
      .GetDocumentLoader()
      ->Fetcher()
      ->Archive();
}

bool FrameFetchContext::AllowScriptFromSource(const KURL& url) const {
  ContentSettingsClient* settings_client = GetContentSettingsClient();
  Settings* settings = GetSettings();
  if (settings_client && !settings_client->AllowScriptFromSource(
                             !settings || settings->GetScriptEnabled(), url)) {
    settings_client->DidNotAllowScript();
    return false;
  }
  return true;
}

SubresourceFilter* FrameFetchContext::GetSubresourceFilter() const {
  if (IsDetached())
    return nullptr;
  DocumentLoader* document_loader = MasterDocumentLoader();
  return document_loader ? document_loader->GetSubresourceFilter() : nullptr;
}

bool FrameFetchContext::ShouldBlockRequestByInspector(
    const ResourceRequest& resource_request) const {
  if (IsDetached())
    return false;
  bool should_block_request = false;
  probe::shouldBlockRequest(GetFrame()->GetDocument(), resource_request,
                            &should_block_request);
  return should_block_request;
}

void FrameFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason) const {
  if (IsDetached())
    return;
  probe::didBlockRequest(GetFrame()->GetDocument(), resource_request,
                         MasterDocumentLoader(), fetch_initiator_info,
                         blocked_reason);
}

bool FrameFetchContext::ShouldBypassMainWorldCSP() const {
  if (IsDetached())
    return false;

  return GetFrame()->GetScriptController().ShouldBypassMainWorldCSP();
}

bool FrameFetchContext::IsSVGImageChromeClient() const {
  if (IsDetached())
    return frozen_state_->is_svg_image_chrome_client;

  return GetFrame()->GetChromeClient().IsSVGImageChromeClient();
}

void FrameFetchContext::CountUsage(WebFeature feature) const {
  if (IsDetached())
    return;
  UseCounter::Count(GetFrame(), feature);
}

void FrameFetchContext::CountDeprecation(WebFeature feature) const {
  if (IsDetached())
    return;
  Deprecation::CountDeprecation(GetFrame(), feature);
}

bool FrameFetchContext::ShouldBlockFetchByMixedContentCheck(
    const ResourceRequest& resource_request,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  return MixedContentChecker::ShouldBlockFetch(GetFrame(), resource_request,
                                               url, reporting_policy);
}

bool FrameFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  if ((!url.User().IsEmpty() || !url.Pass().IsEmpty()) &&
      resource_request.GetRequestContext() !=
          WebURLRequest::kRequestContextXMLHttpRequest) {
    if (Url().User() != url.User() || Url().Pass() != url.Pass() ||
        !SecurityOrigin::Create(url)->IsSameSchemeHostPort(
            GetSecurityOrigin())) {
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

ReferrerPolicy FrameFetchContext::GetReferrerPolicy() const {
  if (IsDetached())
    return frozen_state_->referrer_policy;
  return document_->GetReferrerPolicy();
}

String FrameFetchContext::GetOutgoingReferrer() const {
  if (IsDetached())
    return frozen_state_->outgoing_referrer;
  return document_->OutgoingReferrer();
}

const KURL& FrameFetchContext::Url() const {
  if (IsDetached())
    return frozen_state_->url;
  if (!document_)
    return NullURL();
  return document_->Url();
}

const SecurityOrigin* FrameFetchContext::GetParentSecurityOrigin() const {
  if (IsDetached())
    return frozen_state_->parent_security_origin.Get();
  Frame* parent = GetFrame()->Tree().Parent();
  if (!parent)
    return nullptr;
  return parent->GetSecurityContext()->GetSecurityOrigin();
}

Optional<WebAddressSpace> FrameFetchContext::GetAddressSpace() const {
  if (IsDetached())
    return frozen_state_->address_space;
  if (!document_)
    return WTF::nullopt;
  ExecutionContext* context = document_;
  return WTF::make_optional(context->GetSecurityContext().AddressSpace());
}

const ContentSecurityPolicy* FrameFetchContext::GetContentSecurityPolicy()
    const {
  if (IsDetached())
    return frozen_state_->content_security_policy;
  return document_ ? document_->GetContentSecurityPolicy() : nullptr;
}

void FrameFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  if (document_)
    document_->AddConsoleMessage(message);
}

ContentSettingsClient* FrameFetchContext::GetContentSettingsClient() const {
  if (IsDetached())
    return nullptr;
  return GetFrame()->GetContentSettingsClient();
}

Settings* FrameFetchContext::GetSettings() const {
  if (IsDetached())
    return nullptr;
  DCHECK(GetFrame());
  return GetFrame()->GetSettings();
}

String FrameFetchContext::GetUserAgent() const {
  if (IsDetached())
    return frozen_state_->user_agent;
  return GetFrame()->Loader().UserAgent();
}

RefPtr<SecurityOrigin> FrameFetchContext::GetRequestorOrigin() {
  if (IsDetached())
    return frozen_state_->requestor_origin;

  if (document_->IsSandboxed(kSandboxOrigin))
    return SecurityOrigin::Create(document_->Url());

  return GetSecurityOrigin();
}

RefPtr<SecurityOrigin> FrameFetchContext::GetRequestorOriginForFrameLoading() {
  if (IsDetached())
    return frozen_state_->requestor_origin;

  return GetFrame()->GetDocument()->GetSecurityOrigin();
}

ClientHintsPreferences FrameFetchContext::GetClientHintsPreferences() const {
  if (IsDetached())
    return frozen_state_->client_hints_preferences;

  if (!document_)
    return ClientHintsPreferences();

  return document_->GetClientHintsPreferences();
}

float FrameFetchContext::GetDevicePixelRatio() const {
  if (IsDetached())
    return frozen_state_->device_pixel_ratio;

  if (!document_) {
    // Note that this value is not used because the preferences object returned
    // by GetClientHintsPreferences() doesn't allow to use it.
    return 1.0;
  }

  return document_->DevicePixelRatio();
}

bool FrameFetchContext::ShouldSendClientHint(
    WebClientHintsType type,
    const ClientHintsPreferences& hints_preferences) const {
  return GetClientHintsPreferences().ShouldSend(type) ||
         hints_preferences.ShouldSend(type);
}

void FrameFetchContext::ParseAndPersistClientHints(
    const ResourceResponse& response) {
  ClientHintsPreferences hints_preferences;
  WebEnabledClientHints enabled_client_hints;
  TimeDelta persist_duration;
  FrameClientHintsPreferencesContext hints_context(GetFrame());
  hints_preferences.UpdatePersistentHintsFromHeaders(
      response, &hints_context, enabled_client_hints, &persist_duration);

  if (persist_duration.InSeconds() <= 0)
    return;

  GetContentSettingsClient()->PersistClientHints(
      enabled_client_hints, persist_duration, response.Url());
}

std::unique_ptr<WebURLLoader> FrameFetchContext::CreateURLLoader(
    const ResourceRequest& request) {
  DCHECK(!IsDetached());

  RefPtr<WebTaskRunner> task_runner;
  if (request.GetKeepalive()) {
    // The loader should be able to work after the frame destruction, so we
    // cannot use the task runner associated with the frame.
    task_runner =
        Platform::Current()->CurrentThread()->Scheduler()->LoadingTaskRunner();
  } else {
    task_runner = GetTaskRunner();
  }

  if (MasterDocumentLoader()->GetServiceWorkerNetworkProvider()) {
    WrappedResourceRequest webreq(request);
    auto loader =
        MasterDocumentLoader()
            ->GetServiceWorkerNetworkProvider()
            ->CreateURLLoader(webreq, task_runner->ToSingleThreadTaskRunner());
    if (loader)
      return loader;
  }

  return GetFrame()->CreateURLLoader(request, task_runner.Get());
}

FetchContext* FrameFetchContext::Detach() {
  if (IsDetached())
    return this;

  if (document_) {
    frozen_state_ = new FrozenState(
        GetReferrerPolicy(), GetOutgoingReferrer(), Url(), GetSecurityOrigin(),
        GetParentSecurityOrigin(), GetAddressSpace(),
        GetContentSecurityPolicy(), GetFirstPartyForCookies(),
        GetRequestorOrigin(), GetRequestorOriginForFrameLoading(),
        GetClientHintsPreferences(), GetDevicePixelRatio(), GetUserAgent(),
        IsMainFrame(), IsSVGImageChromeClient());
  } else {
    // Some getters are unavailable in this case.
    frozen_state_ = new FrozenState(
        kReferrerPolicyDefault, String(), NullURL(), GetSecurityOrigin(),
        GetParentSecurityOrigin(), GetAddressSpace(),
        GetContentSecurityPolicy(), GetFirstPartyForCookies(),
        SecurityOrigin::CreateUnique(), SecurityOrigin::CreateUnique(),
        GetClientHintsPreferences(), GetDevicePixelRatio(), GetUserAgent(),
        IsMainFrame(), IsSVGImageChromeClient());
  }

  // This is needed to break a reference cycle in which off-heap
  // ComputedStyle is involved. See https://crbug.com/383860 for details.
  document_ = nullptr;

  return this;
}

DEFINE_TRACE(FrameFetchContext) {
  visitor->Trace(document_loader_);
  visitor->Trace(document_);
  visitor->Trace(frozen_state_);
  BaseFetchContext::Trace(visitor);
}

}  // namespace blink
