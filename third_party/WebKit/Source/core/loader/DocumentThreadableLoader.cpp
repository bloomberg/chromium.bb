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
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/private/CrossOriginPreflightResultCache.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"

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
      return true;
    case WebURLRequest::kRequestContextPlugin:
      return service_worker_mode == WebURLRequest::ServiceWorkerMode::kNone;
    default:
      return false;
  }
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
      force_do_not_allow_stored_credentials_(false),
      security_origin_(resource_loader_options_.security_origin),
      same_origin_request_(false),
      is_using_data_consumer_handle_(false),
      async_(blocking_behavior == kLoadAsynchronously),
      request_context_(WebURLRequest::kRequestContextUnspecified),
      timeout_timer_(loading_context_->GetTaskRunner(TaskType::kNetworking),
                     this,
                     &DocumentThreadableLoader::DidTimeout),
      request_started_seconds_(0.0),
      cors_redirect_limit_(
          (options_.fetch_request_mode ==
               WebURLRequest::kFetchRequestModeCORS ||
           options_.fetch_request_mode ==
               WebURLRequest::kFetchRequestModeCORSWithForcedPreflight)
              ? kMaxCORSRedirects
              : 0),
      redirect_mode_(WebURLRequest::kFetchRedirectModeFollow),
      override_referrer_(false) {
  DCHECK(client);

  // kPreventPreflight can be used only when the CORS is enabled.
  DCHECK(options_.preflight_policy == kConsiderPreflight ||
         options_.fetch_request_mode == WebURLRequest::kFetchRequestModeCORS ||
         options_.fetch_request_mode ==
             WebURLRequest::kFetchRequestModeCORSWithForcedPreflight);
}

void DocumentThreadableLoader::Start(const ResourceRequest& request) {
  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(async_ || request.HttpReferrer().IsEmpty());

  same_origin_request_ =
      GetSecurityOrigin()->CanRequestNoSuborigin(request.Url());
  request_context_ = request.GetRequestContext();
  redirect_mode_ = request.GetFetchRedirectMode();

  if (!same_origin_request_ && options_.fetch_request_mode ==
                                   WebURLRequest::kFetchRequestModeSameOrigin) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(GetDocument(),
                                                                 client_);
    ThreadableLoaderClient* client = client_;
    Clear();
    client->DidFail(ResourceError(kErrorDomainBlinkInternal, 0,
                                  request.Url().GetString(),
                                  "Cross origin requests are not supported."));
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
  if (request_context_ != WebURLRequest::kRequestContextFetch) {
    // When the request context is not "fetch", |fetch_request_mode|
    // represents the fetch request mode, and |allow_credentials| represents
    // the fetch credentials mode. So we set those flags here so that we can see
    // the correct request mode and credentials mode in the service worker's
    // fetch event handler.
    switch (options_.fetch_request_mode) {
      case WebURLRequest::kFetchRequestModeSameOrigin:
      case WebURLRequest::kFetchRequestModeCORS:
      case WebURLRequest::kFetchRequestModeCORSWithForcedPreflight:
        break;
      case WebURLRequest::kFetchRequestModeNoCORS:
        SECURITY_CHECK(IsNoCORSAllowedContext(request_context_,
                                              request.GetServiceWorkerMode()));
        break;
      case WebURLRequest::kFetchRequestModeNavigate:
        NOTREACHED();
        break;
    }
    new_request.SetFetchRequestMode(options_.fetch_request_mode);
    if (resource_loader_options_.allow_credentials == kAllowStoredCredentials) {
      new_request.SetFetchCredentialsMode(
          WebURLRequest::kFetchCredentialsModeInclude);
    } else {
      new_request.SetFetchCredentialsMode(
          WebURLRequest::kFetchCredentialsModeSameOrigin);
    }
  }

  // We assume that ServiceWorker is skipped for sync requests and unsupported
  // protocol requests by content/ code.
  if (async_ &&
      request.GetServiceWorkerMode() ==
          WebURLRequest::ServiceWorkerMode::kAll &&
      SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          request.Url().Protocol()) &&
      loading_context_->GetResourceFetcher()->IsControlledByServiceWorker()) {
    if (new_request.GetFetchRequestMode() ==
            WebURLRequest::kFetchRequestModeCORS ||
        new_request.GetFetchRequestMode() ==
            WebURLRequest::kFetchRequestModeCORSWithForcedPreflight) {
      fallback_request_for_service_worker_ = ResourceRequest(request);
      // m_fallbackRequestForServiceWorker is used when a regular controlling
      // service worker doesn't handle a cross origin request. When this happens
      // we still want to give foreign fetch a chance to handle the request, so
      // only skip the controlling service worker for the fallback request. This
      // is currently safe because of http://crbug.com/604084 the
      // wasFallbackRequiredByServiceWorker flag is never set when foreign fetch
      // handled a request.
      fallback_request_for_service_worker_.SetServiceWorkerMode(
          WebURLRequest::ServiceWorkerMode::kForeign);
    }
    LoadRequest(new_request, resource_loader_options_);
    return;
  }

  DispatchInitialRequest(new_request);
}

void DocumentThreadableLoader::DispatchInitialRequest(
    const ResourceRequest& request) {
  if (!request.IsExternalRequest() &&
      (same_origin_request_ ||
       options_.fetch_request_mode == WebURLRequest::kFetchRequestModeNoCORS)) {
    LoadRequest(request, resource_loader_options_);
    return;
  }

  DCHECK(options_.fetch_request_mode == WebURLRequest::kFetchRequestModeCORS ||
         options_.fetch_request_mode ==
             WebURLRequest::kFetchRequestModeCORSWithForcedPreflight ||
         request.IsExternalRequest());

  MakeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::PrepareCrossOriginRequest(
    ResourceRequest& request) {
  if (GetSecurityOrigin())
    request.SetHTTPOrigin(GetSecurityOrigin());
  if (override_referrer_)
    request.SetHTTPReferrer(referrer_after_redirect_);
}

void DocumentThreadableLoader::MakeCrossOriginAccessRequest(
    const ResourceRequest& request) {
  DCHECK(options_.fetch_request_mode == WebURLRequest::kFetchRequestModeCORS ||
         options_.fetch_request_mode ==
             WebURLRequest::kFetchRequestModeCORSWithForcedPreflight ||
         request.IsExternalRequest());
  DCHECK(client_);
  DCHECK(!GetResource());

  // Cross-origin requests are only allowed certain registered schemes. We would
  // catch this when checking response headers later, but there is no reason to
  // send a request, preflighted or not, that's guaranteed to be denied.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request.Url().Protocol())) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(GetDocument(),
                                                                 client_);
    DispatchDidFailAccessControlCheck(ResourceError(
        kErrorDomainBlinkInternal, 0, request.Url().GetString(),
        "Cross origin requests are only supported for protocol schemes: " +
            SchemeRegistry::ListOfCORSEnabledURLSchemes() + "."));
    return;
  }

  // Non-secure origins may not make "external requests":
  // https://mikewest.github.io/cors-rfc1918/#integration-fetch
  if (!loading_context_->IsSecureContext() && request.IsExternalRequest()) {
    DispatchDidFailAccessControlCheck(
        ResourceError(kErrorDomainBlinkInternal, 0, request.Url().GetString(),
                      "Requests to internal network resources are not allowed "
                      "from non-secure contexts (see https://goo.gl/Y0ZkNV). "
                      "This is an experimental restriction which is part of "
                      "'https://mikewest.github.io/cors-rfc1918/'."));
    return;
  }

  ResourceRequest cross_origin_request(request);
  ResourceLoaderOptions cross_origin_options(resource_loader_options_);

  cross_origin_request.RemoveUserAndPassFromURL();

  cross_origin_request.SetAllowStoredCredentials(EffectiveAllowCredentials() ==
                                                 kAllowStoredCredentials);

  // We update the credentials mode according to effectiveAllowCredentials()
  // here for backward compatibility. But this is not correct.
  // FIXME: We should set it in the caller of DocumentThreadableLoader.
  cross_origin_request.SetFetchCredentialsMode(
      EffectiveAllowCredentials() == kAllowStoredCredentials
          ? WebURLRequest::kFetchCredentialsModeInclude
          : WebURLRequest::kFetchCredentialsModeOmit);

  // We use isSimpleOrForbiddenRequest() here since |request| may have been
  // modified in the process of loading (not from the user's input). For
  // example, referrer. We need to accept them. For security, we must reject
  // forbidden headers/methods at the point we accept user's input. Not here.
  if (!request.IsExternalRequest() &&
      options_.fetch_request_mode !=
          WebURLRequest::kFetchRequestModeCORSWithForcedPreflight &&
      ((options_.preflight_policy == kConsiderPreflight &&
        FetchUtils::IsSimpleOrForbiddenRequest(request.HttpMethod(),
                                               request.HttpHeaderFields())) ||
       options_.preflight_policy == kPreventPreflight)) {
    PrepareCrossOriginRequest(cross_origin_request);
    LoadRequest(cross_origin_request, cross_origin_options);
    return;
  }

  // Explicitly set the ServiceWorkerMode to None here. Although the page is
  // not controlled by a SW at this point, a new SW may be controlling the
  // page when this request gets sent later. We should not send the actual
  // request to the SW. https://crbug.com/604583
  // Similarly we don't want any requests that could involve a CORS preflight
  // to get intercepted by a foreign fetch service worker, even if we have the
  // result of the preflight cached already. https://crbug.com/674370
  cross_origin_request.SetServiceWorkerMode(
      WebURLRequest::ServiceWorkerMode::kNone);

  bool should_force_preflight = request.IsExternalRequest();
  if (!should_force_preflight)
    probe::shouldForceCORSPreflight(GetDocument(), &should_force_preflight);
  // TODO(horo): Currently we don't support the CORS preflight cache on worker
  // thread when off-main-thread-fetch is enabled. https://crbug.com/443374
  bool can_skip_preflight =
      IsMainThread() &&
      CrossOriginPreflightResultCache::Shared().CanSkipPreflight(
          GetSecurityOrigin()->ToString(), cross_origin_request.Url(),
          EffectiveAllowCredentials(), cross_origin_request.HttpMethod(),
          cross_origin_request.HttpHeaderFields());
  if (can_skip_preflight && !should_force_preflight) {
    PrepareCrossOriginRequest(cross_origin_request);
    LoadRequest(cross_origin_request, cross_origin_options);
    return;
  }

  ResourceRequest preflight_request =
      CreateAccessControlPreflightRequest(cross_origin_request);
  // TODO(tyoshino): Call prepareCrossOriginRequest(preflightRequest) to
  // also set the referrer header.
  if (GetSecurityOrigin())
    preflight_request.SetHTTPOrigin(GetSecurityOrigin());

  // Create a ResourceLoaderOptions for preflight.
  ResourceLoaderOptions preflight_options = cross_origin_options;
  preflight_options.allow_credentials = kDoNotAllowStoredCredentials;

  actual_request_ = cross_origin_request;
  actual_options_ = cross_origin_options;

  LoadRequest(preflight_request, preflight_options);
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

  // FIXME: This error is sent to the client in didFail(), so it should not be
  // an internal one. Use LocalFrameClient::cancelledError() instead.
  ResourceError error(kErrorDomainBlinkInternal, 0, GetResource()->Url(),
                      "Load cancelled");
  error.SetIsCancellation(true);

  DispatchDidFail(error);
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

// In this method, we can clear |request| to tell content::WebURLLoaderImpl of
// Chromium not to follow the redirect. This works only when this method is
// called by RawResource::willSendRequest(). If called by
// RawResource::didAddClient(), clearing |request| won't be propagated to
// content::WebURLLoaderImpl. So, this loader must also get detached from the
// resource by calling clearResource().
bool DocumentThreadableLoader::RedirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response) {
  DCHECK(client_);
  DCHECK_EQ(resource, this->GetResource());
  DCHECK(async_);

  checker_.RedirectReceived();

  if (!actual_request_.IsNull()) {
    ReportResponseReceived(resource->Identifier(), redirect_response);

    HandlePreflightFailure(redirect_response.Url().GetString(),
                           "Response for preflight is invalid (redirect)");

    return false;
  }

  if (redirect_mode_ == WebURLRequest::kFetchRedirectModeManual) {
    // We use |m_redirectMode| to check the original redirect mode. |request| is
    // a new request for redirect. So we don't set the redirect mode of it in
    // WebURLLoaderImpl::Context::OnReceivedRedirect().
    DCHECK(request.UseStreamOnResponse());
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
  if (IsAllowedRedirect(request.Url())) {
    client_->DidReceiveRedirectTo(request.Url());
    if (client_->IsDocumentThreadableLoaderClient()) {
      return static_cast<DocumentThreadableLoaderClient*>(client_)
          ->WillFollowRedirect(request, redirect_response);
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

  if (GetDocument() && GetDocument()->GetFrame()) {
    probe::didReceiveCORSRedirectResponse(
        GetDocument()->GetFrame(), resource->Identifier(),
        GetDocument()->GetFrame()->Loader().GetDocumentLoader(),
        redirect_response, resource);
  }

  String access_control_error_description;

  CrossOriginAccessControl::RedirectStatus redirect_status =
      CrossOriginAccessControl::CheckRedirectLocation(request.Url());
  bool allow_redirect =
      redirect_status == CrossOriginAccessControl::kRedirectSuccess;
  if (!allow_redirect) {
    StringBuilder builder;
    builder.Append("Redirect from '");
    builder.Append(redirect_response.Url().GetString());
    builder.Append("' has been blocked by CORS policy: ");
    CrossOriginAccessControl::RedirectErrorString(builder, redirect_status,
                                                  request.Url());
    access_control_error_description = builder.ToString();
  } else if (!same_origin_request_) {
    // The redirect response must pass the access control check if the original
    // request was not same-origin.
    CrossOriginAccessControl::AccessStatus cors_status =
        CrossOriginAccessControl::CheckAccess(redirect_response,
                                              EffectiveAllowCredentials(),
                                              GetSecurityOrigin());
    allow_redirect = cors_status == CrossOriginAccessControl::kAccessAllowed;
    if (!allow_redirect) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(redirect_response.Url().GetString());
      builder.Append("' to '");
      builder.Append(request.Url().GetString());
      builder.Append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::AccessControlErrorString(
          builder, cors_status, redirect_response, GetSecurityOrigin(),
          request_context_);
      access_control_error_description = builder.ToString();
    }
  }

  if (!allow_redirect) {
    DispatchDidFailAccessControlCheck(ResourceError(
        kErrorDomainBlinkInternal, 0, redirect_response.Url().GetString(),
        access_control_error_description));
    return false;
  }

  client_->DidReceiveRedirectTo(request.Url());

  // FIXME: consider combining this with CORS redirect handling performed by
  // CrossOriginAccessControl::handleRedirect().
  ClearResource();

  // If the original request wasn't same-origin, then if the request URL origin
  // is not same origin with the original URL origin, set the source origin to a
  // globally unique identifier. (If the original request was same-origin, the
  // origin of the new request should be the original URL origin.)
  if (!same_origin_request_) {
    RefPtr<SecurityOrigin> original_origin =
        SecurityOrigin::Create(redirect_response.Url());
    RefPtr<SecurityOrigin> request_origin =
        SecurityOrigin::Create(request.Url());
    if (!original_origin->IsSameSchemeHostPort(request_origin.Get()))
      security_origin_ = SecurityOrigin::CreateUnique();
  }
  // Force any subsequent requests to use these checks.
  same_origin_request_ = false;

  // Since the request is no longer same-origin, if the user didn't request
  // credentials in the first place, update our state so we neither request them
  // nor expect they must be allowed.
  if (resource_loader_options_.credentials_requested ==
      kClientDidNotRequestCredentials)
    force_do_not_allow_stored_credentials_ = true;

  // Save the referrer to use when following the redirect.
  override_referrer_ = true;
  referrer_after_redirect_ =
      Referrer(request.HttpReferrer(), request.GetReferrerPolicy());

  ResourceRequest cross_origin_request(request);

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

  HandleResponse(resource->Identifier(), response, std::move(handle));
}

void DocumentThreadableLoader::HandlePreflightResponse(
    const ResourceResponse& response) {
  String access_control_error_description;

  CrossOriginAccessControl::AccessStatus cors_status =
      CrossOriginAccessControl::CheckAccess(
          response, EffectiveAllowCredentials(), GetSecurityOrigin());
  if (cors_status != CrossOriginAccessControl::kAccessAllowed) {
    StringBuilder builder;
    builder.Append(
        "Response to preflight request doesn't pass access "
        "control check: ");
    CrossOriginAccessControl::AccessControlErrorString(
        builder, cors_status, response, GetSecurityOrigin(), request_context_);
    HandlePreflightFailure(response.Url().GetString(), builder.ToString());
    return;
  }

  CrossOriginAccessControl::PreflightStatus preflight_status =
      CrossOriginAccessControl::CheckPreflight(response);
  if (preflight_status != CrossOriginAccessControl::kPreflightSuccess) {
    StringBuilder builder;
    CrossOriginAccessControl::PreflightErrorString(builder, preflight_status,
                                                   response);
    HandlePreflightFailure(response.Url().GetString(), builder.ToString());
    return;
  }

  if (actual_request_.IsExternalRequest()) {
    CrossOriginAccessControl::PreflightStatus external_preflight_status =
        CrossOriginAccessControl::CheckExternalPreflight(response);
    if (external_preflight_status !=
        CrossOriginAccessControl::kPreflightSuccess) {
      StringBuilder builder;
      CrossOriginAccessControl::PreflightErrorString(
          builder, external_preflight_status, response);
      HandlePreflightFailure(response.Url().GetString(), builder.ToString());
      return;
    }
  }

  std::unique_ptr<CrossOriginPreflightResultCacheItem> preflight_result =
      WTF::WrapUnique(
          new CrossOriginPreflightResultCacheItem(EffectiveAllowCredentials()));
  if (!preflight_result->Parse(response, access_control_error_description) ||
      !preflight_result->AllowsCrossOriginMethod(
          actual_request_.HttpMethod(), access_control_error_description) ||
      !preflight_result->AllowsCrossOriginHeaders(
          actual_request_.HttpHeaderFields(),
          access_control_error_description)) {
    HandlePreflightFailure(response.Url().GetString(),
                           access_control_error_description);
    return;
  }

  if (IsMainThread()) {
    // TODO(horo): Currently we don't support the CORS preflight cache on worker
    // thread when off-main-thread-fetch is enabled. https://crbug.com/443374
    CrossOriginPreflightResultCache::Shared().AppendEntry(
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
  probe::didReceiveResourceResponse(frame, identifier, loader, response,
                                    GetResource());
  frame->Console().ReportResourceResponseReceived(loader, identifier, response);
}

void DocumentThreadableLoader::HandleResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(client_);

  if (!actual_request_.IsNull()) {
    ReportResponseReceived(identifier, response);
    HandlePreflightResponse(response);
    return;
  }

  if (response.WasFetchedViaServiceWorker()) {
    if (response.WasFetchedViaForeignFetch())
      loading_context_->RecordUseCount(UseCounter::kForeignFetchInterception);
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

  if (!same_origin_request_ &&
      (options_.fetch_request_mode == WebURLRequest::kFetchRequestModeCORS ||
       options_.fetch_request_mode ==
           WebURLRequest::kFetchRequestModeCORSWithForcedPreflight)) {
    CrossOriginAccessControl::AccessStatus cors_status =
        CrossOriginAccessControl::CheckAccess(
            response, EffectiveAllowCredentials(), GetSecurityOrigin());
    if (cors_status != CrossOriginAccessControl::kAccessAllowed) {
      ReportResponseReceived(identifier, response);
      StringBuilder builder;
      CrossOriginAccessControl::AccessControlErrorString(
          builder, cors_status, response, GetSecurityOrigin(),
          request_context_);
      DispatchDidFailAccessControlCheck(
          ResourceError(kErrorDomainBlinkInternal, 0,
                        response.Url().GetString(), builder.ToString()));
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
    DCHECK(!same_origin_request_);
    DCHECK(options_.fetch_request_mode ==
               WebURLRequest::kFetchRequestModeCORS ||
           options_.fetch_request_mode ==
               WebURLRequest::kFetchRequestModeCORSWithForcedPreflight);
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

  // Using values from net/base/net_error_list.h ERR_TIMED_OUT, Same as existing
  // FIXME above - this error should be coming from LocalFrameClient to be
  // identifiable.
  static const int kTimeoutError = -7;
  ResourceError error("net", kTimeoutError, GetResource()->Url(), String());
  error.SetIsTimeout(true);

  DispatchDidFail(error);
}

void DocumentThreadableLoader::LoadFallbackRequestForServiceWorker() {
  ClearResource();
  ResourceRequest fallback_request(fallback_request_for_service_worker_);
  fallback_request_for_service_worker_ = ResourceRequest();
  DispatchInitialRequest(fallback_request);
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
    const String& url,
    const String& error_description) {
  ResourceError error(kErrorDomainBlinkInternal, 0, url, error_description);

  // Prevent handleSuccessfulFinish() from bypassing access check.
  actual_request_ = ResourceRequest();

  DispatchDidFailAccessControlCheck(error);
}

void DocumentThreadableLoader::DispatchDidFailAccessControlCheck(
    const ResourceError& error) {
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFailAccessControlCheck(error);
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

  FetchParameters new_params(request, options_.initiator,
                             resource_loader_options);
  if (options_.fetch_request_mode == WebURLRequest::kFetchRequestModeNoCORS)
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
    probe::documentThreadableLoaderFailedToStartLoadingForClient(GetDocument(),
                                                                 client_);
    ThreadableLoaderClient* client = client_;
    Clear();
    // setResource() might call notifyFinished() and thus clear()
    // synchronously, and in such cases ThreadableLoaderClient is already
    // notified and |client| is null.
    if (!client)
      return;
    client->DidFail(ResourceError(kErrorDomainBlinkInternal, 0,
                                  request.Url().GetString(),
                                  "Failed to start loading."));
    return;
  }

  if (GetResource()->IsLoading()) {
    unsigned long identifier = GetResource()->Identifier();
    probe::documentThreadableLoaderStartedLoadingForClient(GetDocument(),
                                                           identifier, client_);
  } else {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(GetDocument(),
                                                                 client_);
  }
}

void DocumentThreadableLoader::LoadRequestSync(
    const ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  FetchParameters fetch_params(request, options_.initiator,
                               resource_loader_options);
  if (options_.fetch_request_mode == WebURLRequest::kFetchRequestModeNoCORS)
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

  probe::documentThreadableLoaderStartedLoadingForClient(GetDocument(),
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
  if (request_url != response.Url() && !IsAllowedRedirect(response.Url())) {
    client_ = nullptr;
    client->DidFailRedirectCheck();
    return;
  }

  HandleResponse(identifier, response, nullptr);

  // handleResponse() may detect an error. In such a case (check |m_client| as
  // it gets reset by clear() call), skip the rest.
  //
  // |this| is alive here since loadResourceSynchronously() keeps it alive until
  // the end of the function.
  if (!client_)
    return;

  RefPtr<const SharedBuffer> data = resource->ResourceBuffer();
  if (data)
    HandleReceivedData(data->Data(), data->size());

  // The client may cancel this loader in handleReceivedData(). In such a case,
  // skip the rest.
  if (!client_)
    return;

  HandleSuccessfulFinish(identifier, 0.0);
}

void DocumentThreadableLoader::LoadRequest(
    const ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  // Any credential should have been removed from the cross-site requests.
  const KURL& request_url = request.Url();
  DCHECK(same_origin_request_ || request_url.User().IsEmpty());
  DCHECK(same_origin_request_ || request_url.Pass().IsEmpty());

  // Update resourceLoaderOptions with enforced values.
  if (force_do_not_allow_stored_credentials_)
    resource_loader_options.allow_credentials = kDoNotAllowStoredCredentials;
  resource_loader_options.security_origin = security_origin_;
  if (async_)
    LoadRequestAsync(request, resource_loader_options);
  else
    LoadRequestSync(request, resource_loader_options);
}

bool DocumentThreadableLoader::IsAllowedRedirect(const KURL& url) const {
  if (options_.fetch_request_mode == WebURLRequest::kFetchRequestModeNoCORS)
    return true;

  return same_origin_request_ && GetSecurityOrigin()->CanRequest(url);
}

StoredCredentials DocumentThreadableLoader::EffectiveAllowCredentials() const {
  if (force_do_not_allow_stored_credentials_)
    return kDoNotAllowStoredCredentials;
  return resource_loader_options_.allow_credentials;
}

const SecurityOrigin* DocumentThreadableLoader::GetSecurityOrigin() const {
  return security_origin_ ? security_origin_.Get()
                          : loading_context_->GetSecurityOrigin();
}

Document* DocumentThreadableLoader::GetDocument() const {
  DCHECK(loading_context_);
  return loading_context_->GetLoadingDocument();
}

DEFINE_TRACE(DocumentThreadableLoader) {
  visitor->Trace(resource_);
  visitor->Trace(loading_context_);
  ThreadableLoader::Trace(visitor);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
