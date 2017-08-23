/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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

#include "core/loader/DocumentThreadableLoader.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/BaseFetchContext.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCORS.h"
#include "public/platform/WebCORSPreflightResultCache.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/interfaces/fetch_api.mojom-blink.h"

namespace blink {

namespace {

class EmptyDataHandle final : public WebDataConsumerHandle {
 private:
  class EmptyDataReader final : public WebDataConsumerHandle::Reader {
   public:
    explicit EmptyDataReader(WebDataConsumerHandle::Client* client)
        : factory_(this) {
      Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
          BLINK_FROM_HERE,
          WTF::Bind(&EmptyDataReader::Notify, factory_.CreateWeakPtr(),
                    WTF::Unretained(client)));
    }

   private:
    Result BeginRead(const void** buffer,
                     WebDataConsumerHandle::Flags,
                     size_t* available) override {
      *available = 0;
      *buffer = nullptr;
      return kDone;
    }
    Result EndRead(size_t) override {
      return WebDataConsumerHandle::kUnexpectedError;
    }
    void Notify(WebDataConsumerHandle::Client* client) {
      client->DidGetReadable();
    }
    WeakPtrFactory<EmptyDataReader> factory_;
  };

  std::unique_ptr<Reader> ObtainReader(Client* client) override {
    return WTF::MakeUnique<EmptyDataReader>(client);
  }
  const char* DebugName() const override { return "EmptyDataHandle"; }
};

// No-CORS requests are allowed for all these contexts, and plugin contexts with
// private permission when we set ServiceWorkerMode to None in
// PepperURLLoaderHost.
bool IsNoCORSAllowedContext(
    WebURLRequest::RequestContext context,
    WebURLRequest::ServiceWorkerMode service_worker_mode) {
  switch (context) {
    case WebURLRequest::kRequestContextAudio:
    case WebURLRequest::kRequestContextVideo:
    case WebURLRequest::kRequestContextObject:
    case WebURLRequest::kRequestContextFavicon:
    case WebURLRequest::kRequestContextImage:
    case WebURLRequest::kRequestContextScript:
    case WebURLRequest::kRequestContextWorker:
    case WebURLRequest::kRequestContextSharedWorker:
    case WebURLRequest::kRequestContextFetch:
      return true;
    case WebURLRequest::kRequestContextPlugin:
      return service_worker_mode == WebURLRequest::ServiceWorkerMode::kNone;
    default:
      return false;
  }
}

bool IsCORSEnabledRequestMode(WebURLRequest::FetchRequestMode mode) {
  return mode == WebURLRequest::kFetchRequestModeCORS ||
         mode == WebURLRequest::kFetchRequestModeCORSWithForcedPreflight;
}

}  // namespace

// Max number of CORS redirects handled in DocumentThreadableLoader. Same number
// as net/url_request/url_request.cc, and same number as
// https://fetch.spec.whatwg.org/#concept-http-fetch, Step 4.
// FIXME: currently the number of redirects is counted and limited here and in
// net/url_request/url_request.cc separately.
static const int kMaxCORSRedirects = 20;

void DocumentThreadableLoader::LoadResourceSynchronously(
    Document& document,
    const ResourceRequest& request,
    ThreadableLoaderClient& client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options) {
  (new DocumentThreadableLoader(*ThreadableLoadingContext::Create(document),
                                &client, kLoadSynchronously, options,
                                resource_loader_options))
      ->Start(request);
}

DocumentThreadableLoader* DocumentThreadableLoader::Create(
    ThreadableLoadingContext& loading_context,
    ThreadableLoaderClient* client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options) {
  return new DocumentThreadableLoader(loading_context, client,
                                      kLoadAsynchronously, options,
                                      resource_loader_options);
}

DocumentThreadableLoader::DocumentThreadableLoader(
    ThreadableLoadingContext& loading_context,
    ThreadableLoaderClient* client,
    BlockingBehavior blocking_behavior,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resource_loader_options)
    : client_(client),
      loading_context_(&loading_context),
      options_(options),
      resource_loader_options_(resource_loader_options),
      out_of_blink_cors_(RuntimeEnabledFeatures::OutOfBlinkCORSEnabled()),
      cors_flag_(false),
      suborigin_force_credentials_(false),
      security_origin_(resource_loader_options_.security_origin),
      is_using_data_consumer_handle_(false),
      async_(blocking_behavior == kLoadAsynchronously),
      request_context_(WebURLRequest::kRequestContextUnspecified),
      fetch_request_mode_(WebURLRequest::kFetchRequestModeSameOrigin),
      fetch_credentials_mode_(WebURLRequest::kFetchCredentialsModeOmit),
      timeout_timer_(
          TaskRunnerHelper::Get(TaskType::kNetworking, GetExecutionContext()),
          this,
          &DocumentThreadableLoader::DidTimeout),
      request_started_seconds_(0.0),
      cors_redirect_limit_(0),
      redirect_mode_(WebURLRequest::kFetchRedirectModeFollow),
      override_referrer_(false) {
  DCHECK(client);
}

void DocumentThreadableLoader::Start(const ResourceRequest& request) {
  if (out_of_blink_cors_)
    StartOutOfBlinkCORS(request);
  else
    StartBlinkCORS(request);
}

void DocumentThreadableLoader::StartOutOfBlinkCORS(
    const ResourceRequest& request) {
  DCHECK(out_of_blink_cors_);

  // TODO(hintzed) replace this delegation with an implementation that does not
  // perform CORS checks but relies on CORSURLLoader for CORS
  // (https://crbug.com/736308).
  StartBlinkCORS(request);
}

void DocumentThreadableLoader::DispatchInitialRequestOutOfBlinkCORS(
    ResourceRequest& request) {
  DCHECK(out_of_blink_cors_);

  // TODO(hintzed) replace this delegation with an implementation that does not
  // perform CORS checks but relies on CORSURLLoader for CORS
  // (https://crbug.com/736308).
  DispatchInitialRequestBlinkCORS(request);
}

void DocumentThreadableLoader::HandleResponseOutOfBlinkCORS(
    unsigned long identifier,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  // TODO(hintzed) replace this delegation with an implementation that does not
  // perform CORS checks but relies on CORSURLLoader for CORS
  // (https://crbug.com/736308).
  HandleResponseBlinkCORS(identifier, request_mode, credentials_mode, response,
                          std::move(handle));
}

bool DocumentThreadableLoader::RedirectReceivedOutOfBlinkCORS(
    Resource* resource,
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  DCHECK(out_of_blink_cors_);

  // TODO(hintzed) replace this delegation with an implementation that does not
  // perform CORS checks but relies on CORSURLLoader for CORS
  // (https://crbug.com/736308).
  return RedirectReceivedBlinkCORS(resource, new_request, redirect_response);
}

void DocumentThreadableLoader::MakeCrossOriginAccessRequestOutOfBlinkCORS(
    const ResourceRequest& request) {
  DCHECK(out_of_blink_cors_);

  // TODO(hintzed) replace this delegation with an implementation that does not
  // perform CORS checks but relies on CORSURLLoader for CORS
  // (https://crbug.com/736308).
  MakeCrossOriginAccessRequestBlinkCORS(request);
}

void DocumentThreadableLoader::StartBlinkCORS(const ResourceRequest& request) {
  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(async_ || request.HttpReferrer().IsEmpty());

  bool cors_enabled = IsCORSEnabledRequestMode(request.GetFetchRequestMode());

  // kPreventPreflight can be used only when the CORS is enabled.
  DCHECK(options_.preflight_policy == kConsiderPreflight || cors_enabled);

  if (cors_enabled) {
    cors_redirect_limit_ = kMaxCORSRedirects;
  }

  request_context_ = request.GetRequestContext();
  fetch_request_mode_ = request.GetFetchRequestMode();
  fetch_credentials_mode_ = request.GetFetchCredentialsMode();
  redirect_mode_ = request.GetFetchRedirectMode();

  if (request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeNoCORS) {
    SECURITY_CHECK(IsNoCORSAllowedContext(request_context_,
                                          request.GetServiceWorkerMode()));
  } else {
    cors_flag_ = !GetSecurityOrigin()->CanRequestNoSuborigin(request.Url());
  }

  // Per https://w3c.github.io/webappsec-suborigins/#security-model-opt-outs,
  // credentials are forced when credentials mode is "same-origin", the
  // 'unsafe-credentials' option is set, and the request's physical origin is
  // the same as the URL's.

  suborigin_force_credentials_ =
      GetSecurityOrigin()->HasSuboriginAndShouldAllowCredentialsFor(
          request.Url());

  // The CORS flag variable is not yet used at the step in the spec that
  // corresponds to this line, but divert |cors_flag_| here for convenience.
  if (cors_flag_ && request.GetFetchRequestMode() ==
                        WebURLRequest::kFetchRequestModeSameOrigin) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(
        GetExecutionContext(), client_);
    ThreadableLoaderClient* client = client_;
    Clear();
    ResourceError error = ResourceError::CancelledDueToAccessCheckError(
        request.Url(), ResourceRequestBlockedReason::kOther,
        "Cross origin requests are not supported.");
    const String message = "Failed to load " + error.FailingURL() + ": " +
                           error.LocalizedDescription();
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
    client->DidFail(error);
    return;
  }

  request_started_seconds_ = MonotonicallyIncreasingTime();

  // Save any headers on the request here. If this request redirects
  // cross-origin, we cancel the old request create a new one, and copy these
  // headers.
  request_headers_ = request.HttpHeaderFields();

  // DocumentThreadableLoader is used by all javascript initiated fetch, so we
  // use this chance to record non-GET fetch script requests. However, this is
  // based on the following assumptions, so please be careful when adding
  // similar logic:
  // - ThreadableLoader is used as backend for all javascript initiated network
  //   fetches.
  // - Note that ThreadableLoader is also used for non-network fetch such as
  //   FileReaderLoader. However it emulates GET method so signal is not
  //   recorded here.
  // - ThreadableLoader w/ non-GET request is only created from javascript
  //   initiated fetch.
  // - Some non-script initiated fetches such as WorkerScriptLoader also use
  //   ThreadableLoader, but they are guaranteed to use GET method.
  if (request.HttpMethod() != HTTPNames::GET && GetDocument()) {
    if (Page* page = GetDocument()->GetPage())
      page->GetChromeClient().DidObserveNonGetFetchFromScript();
  }

  ResourceRequest new_request(request);

  // Process the CORS protocol inside the DocumentThreadableLoader for the
  // following cases:
  //
  // - When the request is sync or the protocol is unsupported since we can
  //   assume that any SW is skipped for such requests by content/ code.
  // - When the ServiceWorkerMode is not kAll, the local SW will be skipped.
  //   The request can still be intercepted by a foreign SW, but we cannot know
  //   whether such a foreign fetch interception happens or not at this point.
  // - If we're not yet controlled by a local SW, then we're sure that this
  //   request won't be intercepted by a local SW. In case we end up with
  //   sending a CORS preflight request, the actual request to be sent later
  //   may be intercepted. This is taken care of in LoadPreflightRequest() by
  //   setting the ServiceWorkerMode to kNone.
  //
  // From the above analysis, you can see that the request can never be
  // intercepted by a local SW inside this if-block. It's because:
  // - the ServiceWorkerMode needs to be kAll, and
  // - we're controlled by a SW at this point
  // to allow a local SW to intercept the request. Even when the request gets
  // issued asnychronously after performing the CORS preflight, it doesn'g get
  // intercepted since LoadPreflightRequest() sets the flag to kNone in
  // advance.
  if (!async_ ||
      request.GetServiceWorkerMode() !=
          WebURLRequest::ServiceWorkerMode::kAll ||
      !SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          request.Url().Protocol()) ||
      !loading_context_->GetResourceFetcher()->IsControlledByServiceWorker()) {
    DispatchInitialRequestBlinkCORS(new_request);
    return;
  }

  if (IsCORSEnabledRequestMode(request.GetFetchRequestMode())) {
    // Save the request to fallback_request_for_service_worker to use when the
    // local SW doesn't handle (call respondWith()) a CORS enabled request.
    fallback_request_for_service_worker_ = ResourceRequest(request);

    // We still want to give a foreign SW if any a chance to handle the
    // request. So, only skip the controlling local SW for the fallback
    // request. This is currently safe because of http://crbug.com/604084. The
    // WasFallbackRequiredByServiceWorker() flag is never set when a foreign SW
    // handled a request.
    fallback_request_for_service_worker_.SetServiceWorkerMode(
        WebURLRequest::ServiceWorkerMode::kForeign);
  }

  LoadRequest(new_request, resource_loader_options_);
}

void DocumentThreadableLoader::DispatchInitialRequest(
    ResourceRequest& request) {
  if (out_of_blink_cors_)
    DispatchInitialRequestOutOfBlinkCORS(request);
  else
    DispatchInitialRequestBlinkCORS(request);
}

void DocumentThreadableLoader::DispatchInitialRequestBlinkCORS(
    ResourceRequest& request) {
  if (!request.IsExternalRequest() && !cors_flag_) {
    LoadRequest(request, resource_loader_options_);
    return;
  }

  DCHECK(IsCORSEnabledRequestMode(request.GetFetchRequestMode()) ||
         request.IsExternalRequest());

  MakeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::PrepareCrossOriginRequest(
    ResourceRequest& request) const {
  if (GetSecurityOrigin())
    request.SetHTTPOrigin(GetSecurityOrigin());
  if (override_referrer_)
    request.SetHTTPReferrer(referrer_after_redirect_);
}

void DocumentThreadableLoader::LoadPreflightRequest(
    const ResourceRequest& actual_request,
    const ResourceLoaderOptions& actual_options) {
  WebURLRequest web_url_request = WebCORS::CreateAccessControlPreflightRequest(
      WrappedResourceRequest(actual_request));

  ResourceRequest& preflight_request =
      web_url_request.ToMutableResourceRequest();

  // TODO(tyoshino): Call prepareCrossOriginRequest(preflightRequest) to
  // also set the referrer header.
  if (GetSecurityOrigin())
    preflight_request.SetHTTPOrigin(GetSecurityOrigin());

  actual_request_ = actual_request;
  actual_options_ = actual_options;

  // Explicitly set the ServiceWorkerMode to None here. Although the page is
  // not controlled by a SW at this point, a new SW may be controlling the
  // page when this actual request gets sent later. We should not send the
  // actual request to the SW. See https://crbug.com/604583.
  actual_request_.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kNone);

  // Create a ResourceLoaderOptions for preflight.
  ResourceLoaderOptions preflight_options = actual_options;

  LoadRequest(preflight_request, preflight_options);
}

void DocumentThreadableLoader::MakeCrossOriginAccessRequest(
    const ResourceRequest& request) {
  if (out_of_blink_cors_)
    MakeCrossOriginAccessRequestOutOfBlinkCORS(request);
  else
    MakeCrossOriginAccessRequestBlinkCORS(request);
}

void DocumentThreadableLoader::MakeCrossOriginAccessRequestBlinkCORS(
    const ResourceRequest& request) {
  DCHECK(IsCORSEnabledRequestMode(request.GetFetchRequestMode()) ||
         request.IsExternalRequest());
  DCHECK(client_);
  DCHECK(!GetResource());

  // Cross-origin requests are only allowed certain registered schemes. We would
  // catch this when checking response headers later, but there is no reason to
  // send a request, preflighted or not, that's guaranteed to be denied.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request.Url().Protocol())) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(
        GetExecutionContext(), client_);
    DispatchDidFailAccessControlCheck(
        ResourceError::CancelledDueToAccessCheckError(
            request.Url(), ResourceRequestBlockedReason::kOther,
            String::Format(
                "Cross origin requests are only supported for "
                "protocol schemes: %s.",
                WebCORS::ListOfCORSEnabledURLSchemes().Ascii().c_str())));
    return;
  }

  // Non-secure origins may not make "external requests":
  // https://wicg.github.io/cors-rfc1918/#integration-fetch
  String error_message;
  if (!GetExecutionContext()->IsSecureContext(error_message) &&
      request.IsExternalRequest()) {
    DispatchDidFailAccessControlCheck(
        ResourceError::CancelledDueToAccessCheckError(
            request.Url(), ResourceRequestBlockedReason::kOrigin,
            "Requests to internal network resources are not allowed "
            "from non-secure contexts (see https://goo.gl/Y0ZkNV). "
            "This is an experimental restriction which is part of "
            "'https://mikewest.github.io/cors-rfc1918/'."));
    return;
  }

  ResourceRequest cross_origin_request(request);
  ResourceLoaderOptions cross_origin_options(resource_loader_options_);

  cross_origin_request.RemoveUserAndPassFromURL();

  // Enforce the CORS preflight for checking the Access-Control-Allow-External
  // header. The CORS preflight cache doesn't help for this purpose.
  if (request.IsExternalRequest()) {
    LoadPreflightRequest(cross_origin_request, cross_origin_options);
    return;
  }

  if (request.GetFetchRequestMode() !=
      WebURLRequest::kFetchRequestModeCORSWithForcedPreflight) {
    if (options_.preflight_policy == kPreventPreflight) {
      PrepareCrossOriginRequest(cross_origin_request);
      LoadRequest(cross_origin_request, cross_origin_options);
      return;
    }

    DCHECK_EQ(options_.preflight_policy, kConsiderPreflight);

    // We use ContainsOnlyCORSSafelistedOrForbiddenHeaders() here since
    // |request| may have been modified in the process of loading (not from
    // the user's input). For example, referrer. We need to accept them. For
    // security, we must reject forbidden headers/methods at the point we
    // accept user's input. Not here.
    if (WebCORS::IsCORSSafelistedMethod(request.HttpMethod()) &&
        WebCORS::ContainsOnlyCORSSafelistedOrForbiddenHeaders(
            request.HttpHeaderFields())) {
      PrepareCrossOriginRequest(cross_origin_request);
      LoadRequest(cross_origin_request, cross_origin_options);
      return;
    }
  }

  // Now, we need to check that the request passes the CORS preflight either by
  // issuing a CORS preflight or based on an entry in the CORS preflight cache.

  bool should_ignore_preflight_cache = false;
  if (!IsMainThread()) {
    // TODO(horo): Currently we don't support the CORS preflight cache on worker
    // thread when off-main-thread-fetch is enabled. See
    // https://crbug.com/443374.
    should_ignore_preflight_cache = true;
  } else {
    // Prevent use of the CORS preflight cache when instructed by the DevTools
    // not to use caches.
    probe::shouldForceCORSPreflight(GetExecutionContext(),
                                    &should_ignore_preflight_cache);
  }

  if (should_ignore_preflight_cache ||
      !WebCORSPreflightResultCache::Shared().CanSkipPreflight(
          GetSecurityOrigin()->ToString(), cross_origin_request.Url(),
          cross_origin_request.GetFetchCredentialsMode(),
          cross_origin_request.HttpMethod(),
          cross_origin_request.HttpHeaderFields())) {
    LoadPreflightRequest(cross_origin_request, cross_origin_options);
    return;
  }

  // We don't want any requests that could involve a CORS preflight to get
  // intercepted by a foreign SW, even if we have the result of the preflight
  // cached already. See https://crbug.com/674370.
  cross_origin_request.SetServiceWorkerMode(
      WebURLRequest::ServiceWorkerMode::kNone);

  PrepareCrossOriginRequest(cross_origin_request);
  LoadRequest(cross_origin_request, cross_origin_options);
}

DocumentThreadableLoader::~DocumentThreadableLoader() {
  CHECK(!client_);
  DCHECK(!resource_);
}

void DocumentThreadableLoader::OverrideTimeout(
    unsigned long timeout_milliseconds) {
  DCHECK(async_);

  // |m_requestStartedSeconds| == 0.0 indicates loading is already finished and
  // |m_timeoutTimer| is already stopped, and thus we do nothing for such cases.
  // See https://crbug.com/551663 for details.
  if (request_started_seconds_ <= 0.0)
    return;

  timeout_timer_.Stop();
  // At the time of this method's implementation, it is only ever called by
  // XMLHttpRequest, when the timeout attribute is set after sending the
  // request.
  //
  // The XHR request says to resolve the time relative to when the request
  // was initially sent, however other uses of this method may need to
  // behave differently, in which case this should be re-arranged somehow.
  if (timeout_milliseconds) {
    double elapsed_time =
        MonotonicallyIncreasingTime() - request_started_seconds_;
    double next_fire = timeout_milliseconds / 1000.0;
    double resolved_time = std::max(next_fire - elapsed_time, 0.0);
    timeout_timer_.StartOneShot(resolved_time, BLINK_FROM_HERE);
  }
}

void DocumentThreadableLoader::Cancel() {
  // Cancel can re-enter, and therefore |resource()| might be null here as a
  // result.
  if (!client_ || !GetResource()) {
    Clear();
    return;
  }

  DispatchDidFail(ResourceError::CancelledError(GetResource()->Url()));
}

void DocumentThreadableLoader::SetDefersLoading(bool value) {
  if (GetResource())
    GetResource()->SetDefersLoading(value);
}

void DocumentThreadableLoader::Clear() {
  client_ = nullptr;
  timeout_timer_.Stop();
  request_started_seconds_ = 0.0;
  ClearResource();
}

bool DocumentThreadableLoader::RedirectReceived(
    Resource* resource,
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  if (out_of_blink_cors_) {
    return RedirectReceivedOutOfBlinkCORS(resource, new_request,
                                          redirect_response);
  } else {
    return RedirectReceivedBlinkCORS(resource, new_request, redirect_response);
  }
}

// In this method, we can clear |request| to tell content::WebURLLoaderImpl of
// Chromium not to follow the redirect. This works only when this method is
// called by RawResource::willSendRequest(). If called by
// RawResource::didAddClient(), clearing |request| won't be propagated to
// content::WebURLLoaderImpl. So, this loader must also get detached from the
// resource by calling clearResource().
bool DocumentThreadableLoader::RedirectReceivedBlinkCORS(
    Resource* resource,
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  suborigin_force_credentials_ = false;

  checker_.RedirectReceived();

  const KURL& new_url = new_request.Url();
  const KURL& original_url = redirect_response.Url();

  if (!actual_request_.IsNull()) {
    ReportResponseReceived(resource->Identifier(), redirect_response);

    HandlePreflightFailure(original_url,
                           "Response for preflight is invalid (redirect)");

    return false;
  }

  if (redirect_mode_ == WebURLRequest::kFetchRedirectModeManual) {
    // We use |redirect_mode_| to check the original redirect mode.
    // |new_request| is a new request for redirect. So we don't set the
    // redirect mode of it in WebURLLoaderImpl::Context::OnReceivedRedirect().
    DCHECK(new_request.UseStreamOnResponse());
    // There is no need to read the body of redirect response because there is
    // no way to read the body of opaque-redirect filtered response's internal
    // response.
    // TODO(horo): If we support any API which expose the internal body, we will
    // have to read the body. And also HTTPCache changes will be needed because
    // it doesn't store the body of redirect responses.
    ResponseReceived(resource, redirect_response,
                     WTF::MakeUnique<EmptyDataHandle>());

    if (client_) {
      DCHECK(actual_request_.IsNull());
      NotifyFinished(resource);
    }

    return false;
  }

  if (redirect_mode_ == WebURLRequest::kFetchRedirectModeError) {
    ThreadableLoaderClient* client = client_;
    Clear();
    client->DidFailRedirectCheck();

    return false;
  }

  // Allow same origin requests to continue after allowing clients to audit the
  // redirect.
  if (IsAllowedRedirect(new_request.GetFetchRequestMode(), new_url)) {
    client_->DidReceiveRedirectTo(new_url);
    if (client_->IsDocumentThreadableLoaderClient()) {
      return static_cast<DocumentThreadableLoaderClient*>(client_)
          ->WillFollowRedirect(new_url, redirect_response);
    }
    return true;
  }

  if (cors_redirect_limit_ <= 0) {
    ThreadableLoaderClient* client = client_;
    Clear();
    client->DidFailRedirectCheck();
    return false;
  }

  --cors_redirect_limit_;

  probe::didReceiveCORSRedirectResponse(
      GetExecutionContext(), resource->Identifier(),
      GetDocument() && GetDocument()->GetFrame()
          ? GetDocument()->GetFrame()->Loader().GetDocumentLoader()
          : nullptr,
      redirect_response, resource);

  WebCORS::RedirectStatus redirect_status =
      WebCORS::CheckRedirectLocation(new_url);
  if (redirect_status != WebCORS::RedirectStatus::kRedirectSuccess) {
    StringBuilder builder;
    builder.Append("Redirect from '");
    builder.Append(original_url.GetString());
    builder.Append("' has been blocked by CORS policy: ");
    builder.Append(WebCORS::RedirectErrorString(redirect_status, new_url));
    DispatchDidFailAccessControlCheck(
        ResourceError::CancelledDueToAccessCheckError(
            original_url, ResourceRequestBlockedReason::kOther,
            builder.ToString()));
    return false;
  }

  if (cors_flag_) {
    // The redirect response must pass the access control check if the CORS
    // flag is set.
    WebCORS::AccessStatus cors_status = WebCORS::CheckAccess(
        redirect_response.Url(), redirect_response.HttpStatusCode(),
        redirect_response.HttpHeaderFields(),
        new_request.GetFetchCredentialsMode(),
        WebSecurityOrigin(GetSecurityOrigin()));
    if (cors_status != WebCORS::AccessStatus::kAccessAllowed) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(original_url.GetString());
      builder.Append("' to '");
      builder.Append(new_url.GetString());
      builder.Append("' has been blocked by CORS policy: ");
      builder.Append(WebCORS::AccessControlErrorString(
          cors_status, redirect_response.HttpStatusCode(),
          redirect_response.HttpHeaderFields(),
          WebSecurityOrigin(GetSecurityOrigin()), request_context_));
      DispatchDidFailAccessControlCheck(
          ResourceError::CancelledDueToAccessCheckError(
              original_url, ResourceRequestBlockedReason::kOther,
              builder.ToString()));
      return false;
    }
  }

  client_->DidReceiveRedirectTo(new_url);

  // FIXME: consider combining this with CORS redirect handling performed by
  // CrossOriginAccessControl::handleRedirect().
  ClearResource();

  // If
  // - CORS flag is set, and
  // - the origin of the redirect target URL is not same origin with the origin
  //   of the current request's URL
  // set the source origin to a unique opaque origin.
  //
  // See https://fetch.spec.whatwg.org/#http-redirect-fetch.
  if (cors_flag_) {
    RefPtr<SecurityOrigin> original_origin =
        SecurityOrigin::Create(original_url);
    RefPtr<SecurityOrigin> new_origin = SecurityOrigin::Create(new_url);
    if (!original_origin->IsSameSchemeHostPort(new_origin.Get()))
      security_origin_ = SecurityOrigin::CreateUnique();
  }

  // Set |cors_flag_| so that further logic (corresponds to the main fetch in
  // the spec) will be performed with CORS flag set.
  // See https://fetch.spec.whatwg.org/#http-redirect-fetch.
  cors_flag_ = true;

  // Save the referrer to use when following the redirect.
  override_referrer_ = true;
  referrer_after_redirect_ =
      Referrer(new_request.HttpReferrer(), new_request.GetReferrerPolicy());

  ResourceRequest cross_origin_request(new_request);

  // Remove any headers that may have been added by the network layer that cause
  // access control to fail.
  cross_origin_request.ClearHTTPReferrer();
  cross_origin_request.ClearHTTPOrigin();
  cross_origin_request.ClearHTTPUserAgent();
  // Add any request headers which we previously saved from the
  // original request.
  for (const auto& header : request_headers_)
    cross_origin_request.SetHTTPHeaderField(header.key, header.value);
  MakeCrossOriginAccessRequest(cross_origin_request);

  return false;
}

void DocumentThreadableLoader::RedirectBlocked() {
  checker_.RedirectBlocked();

  // Tells the client that a redirect was received but not followed (for an
  // unknown reason).
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFailRedirectCheck();
}

void DocumentThreadableLoader::DataSent(
    Resource* resource,
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  checker_.DataSent();
  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void DocumentThreadableLoader::DataDownloaded(Resource* resource,
                                              int data_length) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(actual_request_.IsNull());
  DCHECK(async_);

  checker_.DataDownloaded();
  client_->DidDownloadData(data_length);
}

void DocumentThreadableLoader::DidReceiveResourceTiming(
    Resource* resource,
    const ResourceTimingInfo& info) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  client_->DidReceiveResourceTiming(info);
}

void DocumentThreadableLoader::ResponseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  checker_.ResponseReceived();

  if (handle)
    is_using_data_consumer_handle_ = true;

  HandleResponse(resource->Identifier(), fetch_request_mode_,
                 fetch_credentials_mode_, response, std::move(handle));
}

void DocumentThreadableLoader::HandlePreflightResponse(
    const ResourceResponse& response) {

  WebCORS::AccessStatus cors_status = WebCORS::CheckAccess(
      response.Url(), response.HttpStatusCode(), response.HttpHeaderFields(),
      actual_request_.GetFetchCredentialsMode(),
      WebSecurityOrigin(GetSecurityOrigin()));
  if (cors_status != WebCORS::AccessStatus::kAccessAllowed) {
    StringBuilder builder;
    builder.Append(
        "Response to preflight request doesn't pass access "
        "control check: ");
    builder.Append(WebCORS::AccessControlErrorString(
        cors_status, response.HttpStatusCode(), response.HttpHeaderFields(),
        WebSecurityOrigin(GetSecurityOrigin()), request_context_));
    HandlePreflightFailure(response.Url(), builder.ToString());
    return;
  }

  WebCORS::PreflightStatus preflight_status =
      WebCORS::CheckPreflight(response.HttpStatusCode());
  if (preflight_status != WebCORS::PreflightStatus::kPreflightSuccess) {
    HandlePreflightFailure(response.Url(),
                           WebCORS::PreflightErrorString(
                               preflight_status, response.HttpHeaderFields(),
                               response.HttpStatusCode()));
    return;
  }

  if (actual_request_.IsExternalRequest()) {
    WebCORS::PreflightStatus external_preflight_status =
        WebCORS::CheckExternalPreflight(response.HttpHeaderFields());
    if (external_preflight_status !=
        WebCORS::PreflightStatus::kPreflightSuccess) {
      HandlePreflightFailure(response.Url(), WebCORS::PreflightErrorString(
                                                 external_preflight_status,
                                                 response.HttpHeaderFields(),
                                                 response.HttpStatusCode()));
      return;
    }
  }

  WebString access_control_error_description;
  std::unique_ptr<WebCORSPreflightResultCacheItem> preflight_result =
      WebCORSPreflightResultCacheItem::Create(
          actual_request_.GetFetchCredentialsMode(),
          response.HttpHeaderFields(), access_control_error_description);

  if (!preflight_result ||
      !preflight_result->AllowsCrossOriginMethod(
          actual_request_.HttpMethod(), access_control_error_description) ||
      !preflight_result->AllowsCrossOriginHeaders(
          actual_request_.HttpHeaderFields(),
          access_control_error_description)) {
    HandlePreflightFailure(response.Url(), access_control_error_description);
    return;
  }

  if (IsMainThread()) {
    // TODO(horo): Currently we don't support the CORS preflight cache on worker
    // thread when off-main-thread-fetch is enabled. https://crbug.com/443374
    WebCORSPreflightResultCache::Shared().AppendEntry(
        GetSecurityOrigin()->ToString(), actual_request_.Url(),
        std::move(preflight_result));
  }
}

void DocumentThreadableLoader::ReportResponseReceived(
    unsigned long identifier,
    const ResourceResponse& response) {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  if (!frame)
    return;
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  probe::didReceiveResourceResponse(GetExecutionContext(), identifier, loader,
                                    response, GetResource());
  frame->Console().ReportResourceResponseReceived(loader, identifier, response);
}

void DocumentThreadableLoader::HandleResponse(
    unsigned long identifier,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  if (out_of_blink_cors_) {
    HandleResponseOutOfBlinkCORS(identifier, request_mode, credentials_mode,
                                 response, std::move(handle));
  } else {
    HandleResponseBlinkCORS(identifier, request_mode, credentials_mode,
                            response, std::move(handle));
  }
}

void DocumentThreadableLoader::HandleResponseBlinkCORS(
    unsigned long identifier,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(client_);

  if (!actual_request_.IsNull()) {
    ReportResponseReceived(identifier, response);
    HandlePreflightResponse(response);
    return;
  }

  if (response.WasFetchedViaServiceWorker()) {
    if (response.WasFetchedViaForeignFetch()) {
      loading_context_->GetFetchContext()->CountUsage(
          WebFeature::kForeignFetchInterception);
    }
    if (response.WasFallbackRequiredByServiceWorker()) {
      // At this point we must have m_fallbackRequestForServiceWorker. (For
      // SharedWorker the request won't be CORS or CORS-with-preflight,
      // therefore fallback-to-network is handled in the browser process when
      // the ServiceWorker does not call respondWith().)
      DCHECK(!fallback_request_for_service_worker_.IsNull());
      ReportResponseReceived(identifier, response);
      LoadFallbackRequestForServiceWorker();
      return;
    }

    // It's possible that we issue a fetch with request with non "no-cors"
    // mode but get an opaque filtered response if a service worker is involved.
    // We dispatch a CORS failure for the case.
    // TODO(yhirano): This is probably not spec conformant. Fix it after
    // https://github.com/w3c/preload/issues/100 is addressed.
    if (request_mode != WebURLRequest::kFetchRequestModeNoCORS &&
        response.ResponseTypeViaServiceWorker() ==
            network::mojom::FetchResponseType::kOpaque) {
      StringBuilder builder;
      builder.Append(WebCORS::AccessControlErrorString(
          WebCORS::AccessStatus::kInvalidResponse, response.HttpStatusCode(),
          response.HttpHeaderFields(), WebSecurityOrigin(GetSecurityOrigin()),
          request_context_));
      DispatchDidFailAccessControlCheck(
          ResourceError::CancelledDueToAccessCheckError(
              response.Url(), ResourceRequestBlockedReason::kOther,
              builder.ToString()));
      return;
    }

    fallback_request_for_service_worker_ = ResourceRequest();
    client_->DidReceiveResponse(identifier, response, std::move(handle));
    return;
  }

  // Even if the request met the conditions to get handled by a Service Worker
  // in the constructor of this class (and therefore
  // |m_fallbackRequestForServiceWorker| is set), the Service Worker may skip
  // processing the request. Only if the request is same origin, the skipped
  // response may come here (wasFetchedViaServiceWorker() returns false) since
  // such a request doesn't have to go through the CORS algorithm by calling
  // loadFallbackRequestForServiceWorker().
  // FIXME: We should use |m_sameOriginRequest| when we will support Suborigins
  // (crbug.com/336894) for Service Worker.
  DCHECK(fallback_request_for_service_worker_.IsNull() ||
         GetSecurityOrigin()->CanRequest(
             fallback_request_for_service_worker_.Url()));
  fallback_request_for_service_worker_ = ResourceRequest();

  if (IsCORSEnabledRequestMode(request_mode) && cors_flag_) {
    WebCORS::AccessStatus cors_status = WebCORS::CheckAccess(
        response.Url(), response.HttpStatusCode(), response.HttpHeaderFields(),
        credentials_mode, WebSecurityOrigin(GetSecurityOrigin()));
    if (cors_status != WebCORS::AccessStatus::kAccessAllowed) {
      ReportResponseReceived(identifier, response);
      DispatchDidFailAccessControlCheck(
          ResourceError::CancelledDueToAccessCheckError(
              response.Url(), ResourceRequestBlockedReason::kOther,
              WebCORS::AccessControlErrorString(
                  cors_status, response.HttpStatusCode(),
                  response.HttpHeaderFields(),
                  WebSecurityOrigin(GetSecurityOrigin()), request_context_)));
      return;
    }
  }

  client_->DidReceiveResponse(identifier, response, std::move(handle));
}

void DocumentThreadableLoader::SetSerializedCachedMetadata(Resource*,
                                                           const char* data,
                                                           size_t size) {
  checker_.SetSerializedCachedMetadata();

  if (!actual_request_.IsNull())
    return;
  client_->DidReceiveCachedMetadata(data, size);
}

void DocumentThreadableLoader::DataReceived(Resource* resource,
                                            const char* data,
                                            size_t data_length) {
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  checker_.DataReceived();

  if (is_using_data_consumer_handle_)
    return;

  // TODO(junov): Fix the ThreadableLoader ecosystem to use size_t. Until then,
  // we use safeCast to trap potential overflows.
  HandleReceivedData(data, SafeCast<unsigned>(data_length));
}

void DocumentThreadableLoader::HandleReceivedData(const char* data,
                                                  size_t data_length) {
  DCHECK(client_);

  // Preflight data should be invisible to clients.
  if (!actual_request_.IsNull())
    return;

  DCHECK(fallback_request_for_service_worker_.IsNull());

  client_->DidReceiveData(data, data_length);
}

void DocumentThreadableLoader::NotifyFinished(Resource* resource) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  checker_.NotifyFinished(resource);

  if (resource->ErrorOccurred()) {
    DispatchDidFail(resource->GetResourceError());
  } else {
    HandleSuccessfulFinish(resource->Identifier(), resource->LoadFinishTime());
  }
}

void DocumentThreadableLoader::HandleSuccessfulFinish(unsigned long identifier,
                                                      double finish_time) {
  DCHECK(fallback_request_for_service_worker_.IsNull());

  if (!actual_request_.IsNull()) {
    DCHECK(actual_request_.IsExternalRequest() || cors_flag_);
    LoadActualRequest();
    return;
  }

  ThreadableLoaderClient* client = client_;
  // Protect the resource in |didFinishLoading| in order not to release the
  // downloaded file.
  Persistent<Resource> protect = GetResource();
  Clear();
  client->DidFinishLoading(identifier, finish_time);
}

void DocumentThreadableLoader::DidTimeout(TimerBase* timer) {
  DCHECK(async_);
  DCHECK_EQ(timer, &timeout_timer_);
  // clearResource() may be called in clear() and some other places. clear()
  // calls stop() on |m_timeoutTimer|. In the other places, the resource is set
  // again. If the creation fails, clear() is called. So, here, resource() is
  // always non-nullptr.
  DCHECK(GetResource());
  // When |m_client| is set to nullptr only in clear() where |m_timeoutTimer|
  // is stopped. So, |m_client| is always non-nullptr here.
  DCHECK(client_);

  DispatchDidFail(ResourceError::TimeoutError(GetResource()->Url()));
}

void DocumentThreadableLoader::LoadFallbackRequestForServiceWorker() {
  ClearResource();
  ResourceRequest fallback_request(fallback_request_for_service_worker_);
  fallback_request_for_service_worker_ = ResourceRequest();
  if (out_of_blink_cors_)
    DispatchInitialRequestOutOfBlinkCORS(fallback_request);
  else
    DispatchInitialRequestBlinkCORS(fallback_request);
}

void DocumentThreadableLoader::LoadActualRequest() {
  ResourceRequest actual_request = actual_request_;
  ResourceLoaderOptions actual_options = actual_options_;
  actual_request_ = ResourceRequest();
  actual_options_ = ResourceLoaderOptions();

  ClearResource();

  PrepareCrossOriginRequest(actual_request);
  LoadRequest(actual_request, actual_options);
}

void DocumentThreadableLoader::HandlePreflightFailure(
    const KURL& url,
    const String& error_description) {
  // Prevent handleSuccessfulFinish() from bypassing access check.
  actual_request_ = ResourceRequest();

  DispatchDidFailAccessControlCheck(
      ResourceError::CancelledDueToAccessCheckError(
          url, ResourceRequestBlockedReason::kOther, error_description));
}

void DocumentThreadableLoader::DispatchDidFailAccessControlCheck(
    const ResourceError& error) {
  const String message = "Failed to load " + error.FailingURL() + ": " +
                         error.LocalizedDescription();
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));

  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFail(error);
}

void DocumentThreadableLoader::DispatchDidFail(const ResourceError& error) {
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFail(error);
}

void DocumentThreadableLoader::LoadRequestAsync(
    const ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  if (!actual_request_.IsNull())
    resource_loader_options.data_buffering_policy = kBufferData;

  // The timer can be active if this is the actual request of a
  // CORS-with-preflight request.
  if (options_.timeout_milliseconds > 0 && !timeout_timer_.IsActive()) {
    timeout_timer_.StartOneShot(options_.timeout_milliseconds / 1000.0,
                                BLINK_FROM_HERE);
  }

  FetchParameters new_params(request, resource_loader_options);
  if (request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeNoCORS)
    new_params.SetOriginRestriction(FetchParameters::kNoOriginRestriction);
  DCHECK(!GetResource());

  ResourceFetcher* fetcher = loading_context_->GetResourceFetcher();
  if (request.GetRequestContext() == WebURLRequest::kRequestContextVideo ||
      request.GetRequestContext() == WebURLRequest::kRequestContextAudio)
    SetResource(RawResource::FetchMedia(new_params, fetcher));
  else if (request.GetRequestContext() ==
           WebURLRequest::kRequestContextManifest)
    SetResource(RawResource::FetchManifest(new_params, fetcher));
  else
    SetResource(RawResource::Fetch(new_params, fetcher));

  if (!GetResource()) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(
        GetExecutionContext(), client_);
    ThreadableLoaderClient* client = client_;
    Clear();
    // setResource() might call notifyFinished() and thus clear()
    // synchronously, and in such cases ThreadableLoaderClient is already
    // notified and |client| is null.
    if (!client)
      return;
    String message =
        String("Failed to start loading ") + request.Url().GetString();
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
    client->DidFail(ResourceError::CancelledError(request.Url()));
    return;
  }

  if (GetResource()->IsLoading()) {
    unsigned long identifier = GetResource()->Identifier();
    probe::documentThreadableLoaderStartedLoadingForClient(
        GetExecutionContext(), identifier, client_);
  } else {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(
        GetExecutionContext(), client_);
  }
}

void DocumentThreadableLoader::LoadRequestSync(
    const ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  FetchParameters fetch_params(request, resource_loader_options);
  if (request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeNoCORS)
    fetch_params.SetOriginRestriction(FetchParameters::kNoOriginRestriction);
  Resource* resource = RawResource::FetchSynchronously(
      fetch_params, loading_context_->GetResourceFetcher());
  ResourceResponse response =
      resource ? resource->GetResponse() : ResourceResponse();
  unsigned long identifier = resource
                                 ? resource->Identifier()
                                 : std::numeric_limits<unsigned long>::max();
  ResourceError error =
      resource ? resource->GetResourceError() : ResourceError();

  probe::documentThreadableLoaderStartedLoadingForClient(GetExecutionContext(),
                                                         identifier, client_);
  ThreadableLoaderClient* client = client_;

  if (!resource) {
    client_ = nullptr;
    client->DidFail(error);
    return;
  }

  const KURL& request_url = request.Url();

  // No exception for file:/// resources, see <rdar://problem/4962298>. Also, if
  // we have an HTTP response, then it wasn't a network error in fact.
  if (!error.IsNull() && !request_url.IsLocalFile() &&
      response.HttpStatusCode() <= 0) {
    client_ = nullptr;
    client->DidFail(error);
    return;
  }

  // FIXME: A synchronous request does not tell us whether a redirect happened
  // or not, so we guess by comparing the request and response URLs. This isn't
  // a perfect test though, since a server can serve a redirect to the same URL
  // that was requested. Also comparing the request and response URLs as strings
  // will fail if the requestURL still has its credentials.
  if (request_url != response.Url() &&
      !IsAllowedRedirect(request.GetFetchRequestMode(), response.Url())) {
    client_ = nullptr;
    client->DidFailRedirectCheck();
    return;
  }

  HandleResponse(identifier, request.GetFetchRequestMode(),
                 request.GetFetchCredentialsMode(), response, nullptr);

  // HandleResponse() may detect an error. In such a case (check |m_client| as
  // it gets reset by clear() call), skip the rest.
  //
  // |this| is alive here since loadResourceSynchronously() keeps it alive until
  // the end of the function.
  if (!client_)
    return;

  if (RefPtr<const SharedBuffer> data = resource->ResourceBuffer()) {
    data->ForEachSegment([this](const char* segment, size_t segment_size,
                                size_t segment_offset) -> bool {
      HandleReceivedData(segment, segment_size);
      // The client may cancel this loader in handleReceivedData().
      return client_;
    });
  }

  // The client may cancel this loader in handleReceivedData(). In such a case,
  // skip the rest.
  if (!client_)
    return;

  HandleSuccessfulFinish(identifier, 0.0);
}

void DocumentThreadableLoader::LoadRequest(
    ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  resource_loader_options.cors_handling_by_resource_fetcher =
      kDisableCORSHandlingByResourceFetcher;

  bool allow_stored_credentials = false;
  switch (request.GetFetchCredentialsMode()) {
    case WebURLRequest::kFetchCredentialsModeOmit:
      break;
    case WebURLRequest::kFetchCredentialsModeSameOrigin:
      // TODO(tyoshino): It's wrong to use |cors_flag| here. Fix it to use the
      // response tainting.
      //
      // TODO(tyoshino): The credentials mode must work even when the "no-cors"
      // mode is in use. See the following issues:
      // - https://github.com/whatwg/fetch/issues/130
      // - https://github.com/whatwg/fetch/issues/169
      allow_stored_credentials = !cors_flag_ || suborigin_force_credentials_;
      break;
    case WebURLRequest::kFetchCredentialsModeInclude:
    case WebURLRequest::kFetchCredentialsModePassword:
      allow_stored_credentials = true;
      break;
  }
  request.SetAllowStoredCredentials(allow_stored_credentials);

  resource_loader_options.security_origin = security_origin_;
  if (async_)
    LoadRequestAsync(request, resource_loader_options);
  else
    LoadRequestSync(request, resource_loader_options);
}

bool DocumentThreadableLoader::IsAllowedRedirect(
    WebURLRequest::FetchRequestMode fetch_request_mode,
    const KURL& url) const {
  if (fetch_request_mode == WebURLRequest::kFetchRequestModeNoCORS)
    return true;

  return !cors_flag_ && GetSecurityOrigin()->CanRequest(url);
}

SecurityOrigin* DocumentThreadableLoader::GetSecurityOrigin() const {
  return security_origin_
             ? security_origin_.Get()
             : loading_context_->GetFetchContext()->GetSecurityOrigin();
}

Document* DocumentThreadableLoader::GetDocument() const {
  ExecutionContext* context = GetExecutionContext();
  if (context->IsDocument())
    return ToDocument(context);
  return nullptr;
}

ExecutionContext* DocumentThreadableLoader::GetExecutionContext() const {
  DCHECK(loading_context_);
  return loading_context_->GetExecutionContext();
}

DEFINE_TRACE(DocumentThreadableLoader) {
  visitor->Trace(resource_);
  visitor->Trace(loading_context_);
  ThreadableLoader::Trace(visitor);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
