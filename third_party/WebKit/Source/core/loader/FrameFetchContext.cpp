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
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
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
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
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

FrameFetchContext::FrameFetchContext(DocumentLoader* loader, Document* document)
    : BaseFetchContext(document), document_loader_(loader) {
  DCHECK(GetFrame());
}

void FrameFetchContext::ProvideDocumentToContext(FetchContext& context,
                                                 Document* document) {
  DCHECK(document);
  CHECK(context.IsFrameFetchContext());
  static_cast<FrameFetchContext&>(context).execution_context_ = document;
}

FrameFetchContext::~FrameFetchContext() {
  document_loader_ = nullptr;
}

LocalFrame* FrameFetchContext::FrameOfImportsController() const {
  DCHECK(GetDocument());
  HTMLImportsController* imports_controller =
      GetDocument()->ImportsController();
  DCHECK(imports_controller);
  LocalFrame* frame = imports_controller->Master()->GetFrame();
  DCHECK(frame);
  return frame;
}

Document* FrameFetchContext::GetDocument() const {
  return ToDocument(execution_context_.Get());
}

LocalFrame* FrameFetchContext::GetFrame() const {
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

  // Reload should reflect the current data saver setting.
  if (IsReloadLoadType(MasterDocumentLoader()->LoadType()))
    request.ClearHTTPHeaderField("Save-Data");

  if (GetFrame()->GetSettings() &&
      GetFrame()->GetSettings()->GetDataSaverEnabled())
    request.SetHTTPHeaderField("Save-Data", "on");
}

// TODO(toyoshim, arthursonzogni): PlzNavigate doesn't use this function to set
// the ResourceRequest's cache policy. The cache policy determination needs to
// be factored out from FrameFetchContext and moved to the FrameLoader for
// instance.
WebCachePolicy FrameFetchContext::ResourceRequestCachePolicy(
    const ResourceRequest& request,
    Resource::Type type,
    FetchParameters::DeferOption defer) const {
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

// The |m_documentLoader| is null in the FrameFetchContext of an imported
// document.
// FIXME(http://crbug.com/274173): This means Inspector, which uses
// DocumentLoader as a grouping entity, cannot see imported documents.
inline DocumentLoader* FrameFetchContext::MasterDocumentLoader() const {
  if (document_loader_)
    return document_loader_.Get();

  return FrameOfImportsController()->Loader().GetDocumentLoader();
}

void FrameFetchContext::DispatchDidChangeResourcePriority(
    unsigned long identifier,
    ResourceLoadPriority load_priority,
    int intra_priority_value) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceChangePriority", "data",
      InspectorChangeResourcePriorityEvent::Data(identifier, load_priority));
  probe::didChangeResourcePriority(GetFrame(), identifier, load_priority);
}

void FrameFetchContext::PrepareRequest(ResourceRequest& request,
                                       RedirectType redirect_type) {
  GetFrame()->Loader().ApplyUserAgent(request);
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
  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    GetFrame()->Loader().Progress().WillStartLoading(identifier,
                                                     request.Priority());
  }
  probe::willSendRequest(GetFrame(), identifier, MasterDocumentLoader(),
                         request, redirect_response, initiator_info);
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
      GetDocument(), NetworkHintsInterfaceImpl(), resource_loading_policy,
      LinkLoader::kLoadAll, nullptr);

  if (response.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(GetFrame(), response,
                                                frame_type, request_context);
  }

  GetFrame()->Loader().Progress().IncrementProgress(identifier, response);
  GetLocalFrameClient()->DispatchDidReceiveResponse(response);
  DocumentLoader* document_loader = MasterDocumentLoader();
  probe::didReceiveResourceResponse(GetFrame(), identifier, document_loader,
                                    response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  GetFrame()->Console().ReportResourceResponseReceived(document_loader,
                                                       identifier, response);

  // MainResource responses were already added, skip them here.
  if (RuntimeEnabledFeatures::serverTimingEnabled() &&
      resource->GetType() != Resource::kMainResource &&
      GetFrame()->GetDocument() && GetFrame()->GetDocument()->domWindow()) {
    LocalDOMWindow* localDOMWindow = GetFrame()->GetDocument()->domWindow();
    DOMWindowPerformance::performance(*localDOMWindow)
        ->AddServerTiming(response,
                          localDOMWindow->HasLoadEventFired()
                              ? PerformanceBase::ShouldAddToBuffer::Never
                              : PerformanceBase::ShouldAddToBuffer::Always);
  }
}

void FrameFetchContext::DispatchDidReceiveData(unsigned long identifier,
                                               const char* data,
                                               int data_length) {
  GetFrame()->Loader().Progress().IncrementProgress(identifier, data_length);
  probe::didReceiveData(GetFrame(), identifier, data, data_length);
}

void FrameFetchContext::DispatchDidReceiveEncodedData(unsigned long identifier,
                                                      int encoded_data_length) {
  probe::didReceiveEncodedDataLength(GetFrame(), identifier,
                                     encoded_data_length);
}

void FrameFetchContext::DispatchDidDownloadData(unsigned long identifier,
                                                int data_length,
                                                int encoded_data_length) {
  GetFrame()->Loader().Progress().IncrementProgress(identifier, data_length);
  probe::didReceiveData(GetFrame(), identifier, 0, data_length);
  probe::didReceiveEncodedDataLength(GetFrame(), identifier,
                                     encoded_data_length);
}

void FrameFetchContext::DispatchDidFinishLoading(unsigned long identifier,
                                                 double finish_time,
                                                 int64_t encoded_data_length,
                                                 int64_t decoded_body_length) {
  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::didFinishLoading(GetFrame(), identifier, finish_time,
                          encoded_data_length, decoded_body_length);
  if (GetFrame()->FrameScheduler())
    GetFrame()->FrameScheduler()->DidStopLoading(identifier);
}

void FrameFetchContext::DispatchDidFail(unsigned long identifier,
                                        const ResourceError& error,
                                        int64_t encoded_data_length,
                                        bool is_internal_request) {
  GetFrame()->Loader().Progress().CompleteProgress(identifier);
  probe::didFailLoading(GetFrame(), identifier, error);
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
  GetLocalFrameClient()->DispatchDidLoadResourceFromMemoryCache(
      resource_request, resource_response);
}

bool FrameFetchContext::ShouldLoadNewResource(Resource::Type type) const {
  if (!document_loader_)
    return true;

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
  if (!GetDocument())
    return;
  FirstMeaningfulPaintDetector::From(*GetDocument()).CheckNetworkStable();
  if (resource->IsLoadEventBlockingResourceType())
    GetDocument()->CheckCompleted();
}

void FrameFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  Document* initiator_document = GetDocument() && info.IsMainResource()
                                     ? GetDocument()->ParentDocument()
                                     : GetDocument();
  if (!initiator_document || !initiator_document->domWindow())
    return;
  DOMWindowPerformance::performance(*initiator_document->domWindow())
      ->AddResourceTiming(info);
}

bool FrameFetchContext::AllowImage(bool images_enabled, const KURL& url) const {
  return GetContentSettingsClient()->AllowImage(images_enabled, url);
}

bool FrameFetchContext::IsControlledByServiceWorker() const {
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
  DCHECK(MasterDocumentLoader());
  auto* service_worker_network_provider =
      MasterDocumentLoader()->GetServiceWorkerNetworkProvider();
  return service_worker_network_provider
             ? service_worker_network_provider->ServiceWorkerID()
             : -1;
}

bool FrameFetchContext::IsMainFrame() const {
  return GetFrame()->IsMainFrame();
}

bool FrameFetchContext::DefersLoading() const {
  return GetFrame()->GetPage()->Suspended();
}

bool FrameFetchContext::IsLoadComplete() const {
  return GetDocument() && GetDocument()->LoadEventFinished();
}

bool FrameFetchContext::PageDismissalEventBeingDispatched() const {
  return GetDocument() && GetDocument()->PageDismissalEventBeingDispatched() !=
                              Document::kNoDismissal;
}

bool FrameFetchContext::UpdateTimingInfoForIFrameNavigation(
    ResourceTimingInfo* info) {
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
  PingLoader::LoadImage(GetFrame(), url);
}

void FrameFetchContext::AddConsoleMessage(const String& message,
                                          LogMessageType message_type) const {
  MessageLevel level = message_type == kLogWarningMessage ? kWarningMessageLevel
                                                          : kErrorMessageLevel;
  ConsoleMessage* console_message =
      ConsoleMessage::Create(kJSMessageSource, level, message);
  // Route the console message through Document if it's attached, so
  // that script line numbers can be included. Otherwise, route directly to the
  // FrameConsole, to ensure we never drop a message.
  if (GetDocument() && GetDocument()->GetFrame())
    GetDocument()->AddConsoleMessage(console_message);
  else
    GetFrame()->Console().AddMessage(console_message);
}

void FrameFetchContext::ModifyRequestForCSP(ResourceRequest& resource_request) {
  // Record the latest requiredCSP value that will be used when sending this
  // request.
  GetFrame()->Loader().RecordLatestRequiredCSP();
  GetFrame()->Loader().ModifyRequestForCSP(resource_request, GetDocument());
}

void FrameFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  if (!RuntimeEnabledFeatures::clientHintsEnabled() || !GetDocument())
    return;

  bool should_send_dpr =
      GetDocument()->GetClientHintsPreferences().ShouldSendDPR() ||
      hints_preferences.ShouldSendDPR();
  bool should_send_resource_width =
      GetDocument()->GetClientHintsPreferences().ShouldSendResourceWidth() ||
      hints_preferences.ShouldSendResourceWidth();
  bool should_send_viewport_width =
      GetDocument()->GetClientHintsPreferences().ShouldSendViewportWidth() ||
      hints_preferences.ShouldSendViewportWidth();

  if (should_send_dpr) {
    request.AddHTTPHeaderField(
        "DPR", AtomicString(String::Number(GetDocument()->DevicePixelRatio())));
  }

  if (should_send_resource_width) {
    if (resource_width.is_set) {
      float physical_width =
          resource_width.width * GetDocument()->DevicePixelRatio();
      request.AddHTTPHeaderField(
          "Width", AtomicString(String::Number(ceil(physical_width))));
    }
  }

  if (should_send_viewport_width && GetFrame()->View()) {
    request.AddHTTPHeaderField(
        "Viewport-Width",
        AtomicString(String::Number(GetFrame()->View()->ViewportWidth())));
  }
}

void FrameFetchContext::PopulateResourceRequest(
    const KURL& url,
    Resource::Type type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest& request) {
  SetFirstPartyCookieAndRequestorOrigin(request);

  // Before modifying the request for CSP, evaluate report-only headers. This
  // allows site owners to learn about requests that are being modified
  // (e.g. mixed content that is being upgraded by upgrade-insecure-requests).
  CheckCSPForRequest(request, url, options, reporting_policy,
                     request.GetRedirectStatus(),
                     ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);

  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(hints_preferences, resource_width, request);
  AddCSPHeaderIfNecessary(type, request);
}

void FrameFetchContext::SetFirstPartyCookieAndRequestorOrigin(
    ResourceRequest& request) {
  if (!GetDocument())
    return;

  if (request.FirstPartyForCookies().IsNull()) {
    request.SetFirstPartyForCookies(
        GetDocument() ? GetDocument()->FirstPartyForCookies()
                      : SecurityOrigin::UrlWithUniqueSecurityOrigin());
  }

  // Subresource requests inherit their requestor origin from |m_document|
  // directly. Top-level and nested frame types are taken care of in
  // 'FrameLoadRequest()'. Auxiliary frame types in 'createWindow()' and
  // 'FrameLoader::load'.
  // TODO(mkwst): It would be cleaner to adjust blink::ResourceRequest to
  // initialize itself with a `nullptr` initiator so that this can be a simple
  // `isNull()` check. https://crbug.com/625969
  if (request.GetFrameType() == WebURLRequest::kFrameTypeNone &&
      request.RequestorOrigin()->IsUnique()) {
    request.SetRequestorOrigin(
        GetDocument()->IsSandboxed(kSandboxOrigin)
            ? SecurityOrigin::Create(execution_context_->Url())
            : execution_context_->GetSecurityOrigin());
  }
}

MHTMLArchive* FrameFetchContext::Archive() const {
  DCHECK(!IsMainFrame());
  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in multiple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  if (!GetFrame()->Tree().Parent()->IsLocalFrame())
    return nullptr;
  return ToLocalFrame(GetFrame()->Tree().Parent())
      ->Loader()
      .GetDocumentLoader()
      ->Fetcher()
      ->Archive();
}

RefPtr<WebTaskRunner> FrameFetchContext::LoadingTaskRunner() const {
  return GetFrame()->FrameScheduler()->LoadingTaskRunner();
}

ContentSettingsClient* FrameFetchContext::GetContentSettingsClient() const {
  return GetFrame()->GetContentSettingsClient();
}

Settings* FrameFetchContext::GetSettings() const {
  DCHECK(GetFrame());
  return GetFrame()->GetSettings();
}

SubresourceFilter* FrameFetchContext::GetSubresourceFilter() const {
  DocumentLoader* document_loader = MasterDocumentLoader();
  return document_loader ? document_loader->GetSubresourceFilter() : nullptr;
}

SecurityContext* FrameFetchContext::GetParentSecurityContext() const {
  if (Frame* parent = GetFrame()->Tree().Parent())
    return parent->GetSecurityContext();
  return nullptr;
}

bool FrameFetchContext::ShouldBlockRequestByInspector(
    const ResourceRequest& resource_request) const {
  bool should_block_request = false;
  probe::shouldBlockRequest(GetFrame(), resource_request,
                            &should_block_request);
  return should_block_request;
}

void FrameFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason) const {
  probe::didBlockRequest(GetFrame(), resource_request, MasterDocumentLoader(),
                         fetch_initiator_info, blocked_reason);
}

void FrameFetchContext::ReportLocalLoadFailed(const KURL& url) const {
  FrameLoader::ReportLocalLoadFailed(GetFrame(), url.ElidedString());
}

bool FrameFetchContext::ShouldBypassMainWorldCSP() const {
  return GetFrame()->GetScriptController().ShouldBypassMainWorldCSP();
}

bool FrameFetchContext::IsSVGImageChromeClient() const {
  return GetFrame()->GetChromeClient().IsSVGImageChromeClient();
}

void FrameFetchContext::CountUsage(UseCounter::Feature feature) const {
  UseCounter::Count(GetFrame(), feature);
}

void FrameFetchContext::CountDeprecation(UseCounter::Feature feature) const {
  Deprecation::CountDeprecation(GetFrame(), feature);
}

bool FrameFetchContext::ShouldBlockFetchByMixedContentCheck(
    const ResourceRequest& resource_request,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  return MixedContentChecker::ShouldBlockFetch(GetFrame(), resource_request,
                                               url, reporting_policy);
}

std::unique_ptr<WebURLLoader> FrameFetchContext::CreateURLLoader() {
  return Platform::Current()->CreateURLLoader();
}

DEFINE_TRACE(FrameFetchContext) {
  visitor->Trace(document_loader_);
  BaseFetchContext::Trace(visitor);
}

}  // namespace blink
