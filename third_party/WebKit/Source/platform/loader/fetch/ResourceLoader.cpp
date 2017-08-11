/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/ResourceLoader.h"

#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/NetworkInstrumentation.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCORS.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "services/network/public/interfaces/fetch_api.mojom-blink.h"

namespace blink {

ResourceLoader* ResourceLoader::Create(ResourceFetcher* fetcher,
                                       ResourceLoadScheduler* scheduler,
                                       Resource* resource) {
  return new ResourceLoader(fetcher, scheduler, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher,
                               ResourceLoadScheduler* scheduler,
                               Resource* resource)
    : scheduler_client_id_(ResourceLoadScheduler::kInvalidClientId),
      fetcher_(fetcher),
      scheduler_(scheduler),
      resource_(resource),
      is_cache_aware_loading_activated_(false) {
  DCHECK(resource_);
  DCHECK(fetcher_);

  resource_->SetLoader(this);
}

ResourceLoader::~ResourceLoader() {}

DEFINE_TRACE(ResourceLoader) {
  visitor->Trace(fetcher_);
  visitor->Trace(scheduler_);
  visitor->Trace(resource_);
  ResourceLoadSchedulerClient::Trace(visitor);
}

void ResourceLoader::Start() {
  const ResourceRequest& request = resource_->GetResourceRequest();
  ActivateCacheAwareLoadingIfNeeded(request);
  loader_ = Context().CreateURLLoader(request);

  // Synchronous requests should not work with a throttling. Also, tentatively
  // disables throttling for fetch requests that could keep on holding an active
  // connection until data is read by JavaScript.
  ResourceLoadScheduler::ThrottleOption option =
      (resource_->Options().synchronous_policy == kRequestSynchronously ||
       request.GetRequestContext() == WebURLRequest::kRequestContextFetch)
          ? ResourceLoadScheduler::ThrottleOption::kCanNotBeThrottled
          : ResourceLoadScheduler::ThrottleOption::kCanBeThrottled;
  DCHECK_EQ(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  scheduler_->Request(this, option, &scheduler_client_id_);
}

void ResourceLoader::Run() {
  StartWith(resource_->GetResourceRequest());
}

void ResourceLoader::StartWith(const ResourceRequest& request) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  DCHECK(loader_);

  if (resource_->Options().synchronous_policy == kRequestSynchronously &&
      Context().DefersLoading()) {
    Cancel();
    return;
  }

  loader_->SetDefersLoading(Context().DefersLoading());

  if (request.GetKeepalive())
    keepalive_ = this;

  if (is_cache_aware_loading_activated_) {
    // Override cache policy for cache-aware loading. If this request fails, a
    // reload with original request will be triggered in DidFail().
    ResourceRequest cache_aware_request(request);
    cache_aware_request.SetCachePolicy(WebCachePolicy::kReturnCacheDataIfValid);
    loader_->LoadAsynchronously(WrappedResourceRequest(cache_aware_request),
                                this);
    return;
  }

  if (resource_->Options().synchronous_policy == kRequestSynchronously)
    RequestSynchronously(request);
  else
    loader_->LoadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::Release(ResourceLoadScheduler::ReleaseOption option) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  bool released = scheduler_->Release(scheduler_client_id_, option);
  DCHECK(released);
  scheduler_client_id_ = ResourceLoadScheduler::kInvalidClientId;
}

void ResourceLoader::Restart(const ResourceRequest& request) {
  CHECK_EQ(resource_->Options().synchronous_policy, kRequestAsynchronously);

  keepalive_.Clear();
  loader_ = Context().CreateURLLoader(request);
  StartWith(request);
}

void ResourceLoader::SetDefersLoading(bool defers) {
  DCHECK(loader_);
  loader_->SetDefersLoading(defers);
}

void ResourceLoader::DidChangePriority(ResourceLoadPriority load_priority,
                                       int intra_priority_value) {
  if (loader_) {
    loader_->DidChangePriority(
        static_cast<WebURLRequest::Priority>(load_priority),
        intra_priority_value);
  }
}

void ResourceLoader::Cancel() {
  HandleError(
      ResourceError::CancelledError(resource_->LastResourceRequest().Url()));
}

void ResourceLoader::CancelForRedirectAccessCheckError(
    const KURL& new_url,
    ResourceRequestBlockedReason blocked_reason) {
  resource_->WillNotFollowRedirect();

  if (loader_) {
    HandleError(
        ResourceError::CancelledDueToAccessCheckError(new_url, blocked_reason));
  }
}

static bool IsManualRedirectFetchRequest(const ResourceRequest& request) {
  return request.GetFetchRedirectMode() ==
             WebURLRequest::kFetchRedirectModeManual &&
         request.GetRequestContext() == WebURLRequest::kRequestContextFetch;
}

bool ResourceLoader::WillFollowRedirect(
    const WebURL& new_url,
    const WebURL& new_site_for_cookies,
    const WebString& new_referrer,
    WebReferrerPolicy new_referrer_policy,
    const WebString& new_method,
    const WebURLResponse& passed_redirect_response,
    bool& report_raw_headers) {
  DCHECK(!passed_redirect_response.IsNull());

  if (is_cache_aware_loading_activated_) {
    // Fail as cache miss if cached response is a redirect.
    HandleError(
        ResourceError::CacheMissError(resource_->LastResourceRequest().Url()));
    return false;
  }

  const ResourceRequest& last_request = resource_->LastResourceRequest();
  ResourceRequest new_request(new_url);
  new_request.SetSiteForCookies(new_site_for_cookies);
  new_request.SetDownloadToFile(last_request.DownloadToFile());
  new_request.SetUseStreamOnResponse(last_request.UseStreamOnResponse());
  new_request.SetRequestContext(last_request.GetRequestContext());
  new_request.SetFrameType(last_request.GetFrameType());
  new_request.SetServiceWorkerMode(
      passed_redirect_response.WasFetchedViaServiceWorker()
          ? WebURLRequest::ServiceWorkerMode::kAll
          : WebURLRequest::ServiceWorkerMode::kNone);
  new_request.SetShouldResetAppCache(last_request.ShouldResetAppCache());
  new_request.SetFetchRequestMode(last_request.GetFetchRequestMode());
  new_request.SetFetchCredentialsMode(last_request.GetFetchCredentialsMode());
  new_request.SetKeepalive(last_request.GetKeepalive());
  String referrer =
      new_referrer.IsEmpty() ? Referrer::NoReferrer() : String(new_referrer);
  new_request.SetHTTPReferrer(
      Referrer(referrer, static_cast<ReferrerPolicy>(new_referrer_policy)));
  new_request.SetPriority(last_request.Priority());
  new_request.SetHTTPMethod(new_method);
  if (new_request.HttpMethod() == last_request.HttpMethod())
    new_request.SetHTTPBody(last_request.HttpBody());
  new_request.SetCheckForBrowserSideNavigation(
      last_request.CheckForBrowserSideNavigation());

  Resource::Type resource_type = resource_->GetType();

  const ResourceRequest& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  WebURLRequest::RequestContext request_context =
      initial_request.GetRequestContext();
  WebURLRequest::FrameType frame_type = initial_request.GetFrameType();
  WebURLRequest::FetchRequestMode fetch_request_mode =
      initial_request.GetFetchRequestMode();
  WebURLRequest::FetchCredentialsMode fetch_credentials_mode =
      initial_request.GetFetchCredentialsMode();

  const ResourceLoaderOptions& options = resource_->Options();

  const ResourceResponse& redirect_response(
      passed_redirect_response.ToResourceResponse());

  new_request.SetRedirectStatus(
      ResourceRequest::RedirectStatus::kFollowedRedirect);

  if (!IsManualRedirectFetchRequest(initial_request)) {
    bool unused_preload = resource_->IsUnusedPreload();

    // Don't send security violation reports for unused preloads.
    SecurityViolationReportingPolicy reporting_policy =
        unused_preload ? SecurityViolationReportingPolicy::kSuppressReporting
                       : SecurityViolationReportingPolicy::kReport;

    // CanRequest() checks only enforced CSP, so check report-only here to
    // ensure that violations are sent.
    Context().CheckCSPForRequest(
        request_context, new_url, options, reporting_policy,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
        resource_type, new_request, new_url, options, reporting_policy,
        FetchParameters::kUseDefaultOriginRestrictionForType,
        ResourceRequest::RedirectStatus::kFollowedRedirect);
    if (blocked_reason != ResourceRequestBlockedReason::kNone) {
      CancelForRedirectAccessCheckError(new_url, blocked_reason);
      return false;
    }

    if (options.cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        fetch_request_mode == WebURLRequest::kFetchRequestModeCORS) {
      RefPtr<SecurityOrigin> source_origin = options.security_origin;
      if (!source_origin.Get())
        source_origin = Context().GetSecurityOrigin();
      WebSecurityOrigin source_web_origin(source_origin.Get());
      WrappedResourceRequest new_request_wrapper(new_request);
      WebString cors_error_msg;
      if (!WebCORS::HandleRedirect(
              source_web_origin, new_request_wrapper, redirect_response.Url(),
              redirect_response.HttpStatusCode(),
              redirect_response.HttpHeaderFields(), fetch_credentials_mode,
              resource_->MutableOptions(), cors_error_msg)) {
        resource_->SetCORSStatus(CORSStatus::kFailed);

        if (!unused_preload) {
          Context().AddErrorConsoleMessage(cors_error_msg,
                                           FetchContext::kJSSource);
        }

        CancelForRedirectAccessCheckError(new_url,
                                          ResourceRequestBlockedReason::kOther);
        return false;
      }

      source_origin = source_web_origin;
    }
    if (resource_type == Resource::kImage &&
        fetcher_->ShouldDeferImageLoad(new_url)) {
      CancelForRedirectAccessCheckError(new_url,
                                        ResourceRequestBlockedReason::kOther);
      return false;
    }
  }

  bool cross_origin =
      !SecurityOrigin::AreSameSchemeHostPort(redirect_response.Url(), new_url);
  fetcher_->RecordResourceTimingOnRedirect(resource_.Get(), redirect_response,
                                           cross_origin);

  if (options.cors_handling_by_resource_fetcher ==
          kEnableCORSHandlingByResourceFetcher &&
      fetch_request_mode == WebURLRequest::kFetchRequestModeCORS) {
    bool allow_stored_credentials = false;
    switch (fetch_credentials_mode) {
      case WebURLRequest::kFetchCredentialsModeOmit:
        break;
      case WebURLRequest::kFetchCredentialsModeSameOrigin:
        allow_stored_credentials = !options.cors_flag;
        break;
      case WebURLRequest::kFetchCredentialsModeInclude:
      case WebURLRequest::kFetchCredentialsModePassword:
        allow_stored_credentials = true;
        break;
    }
    new_request.SetAllowStoredCredentials(allow_stored_credentials);
  }

  // The following two calls may rewrite the new_request.Url() to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::WillSendRequest() and
  // RenderFrameImpl::WillSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, compare new_request.Url() and
  // new_url after calling them, and return false to make the redirect fail on
  // mismatch.

  Context().PrepareRequest(new_request,
                           FetchContext::RedirectType::kForRedirect);
  Context().DispatchWillSendRequest(resource_->Identifier(), new_request,
                                    redirect_response, options.initiator_info);

  // First-party cookie logic moved from DocumentLoader in Blink to
  // net::URLRequest in the browser. Assert that Blink didn't try to change it
  // to something else.
  DCHECK(KURL(new_site_for_cookies) == new_request.SiteForCookies());

  // The following parameters never change during the lifetime of a request.
  DCHECK_EQ(new_request.GetRequestContext(), request_context);
  DCHECK_EQ(new_request.GetFrameType(), frame_type);
  DCHECK_EQ(new_request.GetFetchRequestMode(), fetch_request_mode);
  DCHECK_EQ(new_request.GetFetchCredentialsMode(), fetch_credentials_mode);

  if (new_request.Url() != KURL(new_url)) {
    CancelForRedirectAccessCheckError(new_request.Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  if (!resource_->WillFollowRedirect(new_request, redirect_response)) {
    CancelForRedirectAccessCheckError(new_request.Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  report_raw_headers = new_request.ReportRawHeaders();

  return true;
}

void ResourceLoader::DidReceiveCachedMetadata(const char* data, int length) {
  resource_->SetSerializedCachedMetadata(data, length);
}

void ResourceLoader::DidSendData(unsigned long long bytes_sent,
                                 unsigned long long total_bytes_to_be_sent) {
  resource_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

FetchContext& ResourceLoader::Context() const {
  return fetcher_->Context();
}

CORSStatus ResourceLoader::DetermineCORSStatus(const ResourceResponse& response,
                                               StringBuilder& error_msg) const {
  // Service workers handle CORS separately.
  if (response.WasFetchedViaServiceWorker()) {
    switch (response.ResponseTypeViaServiceWorker()) {
      case network::mojom::FetchResponseType::kBasic:
      case network::mojom::FetchResponseType::kCORS:
      case network::mojom::FetchResponseType::kDefault:
      case network::mojom::FetchResponseType::kError:
        return CORSStatus::kServiceWorkerSuccessful;
      case network::mojom::FetchResponseType::kOpaque:
      case network::mojom::FetchResponseType::kOpaqueRedirect:
        return CORSStatus::kServiceWorkerOpaque;
    }
    NOTREACHED();
  }

  if (resource_->GetType() == Resource::Type::kMainResource)
    return CORSStatus::kNotApplicable;

  SecurityOrigin* source_origin = resource_->Options().security_origin.Get();

  if (!source_origin)
    source_origin = Context().GetSecurityOrigin();

  DCHECK(source_origin);

  if (source_origin->CanRequestNoSuborigin(response.Url()))
    return CORSStatus::kSameOrigin;

  // RequestContext, FetchRequestMode and FetchCredentialsMode never change
  // during the lifetime of a request.
  const ResourceRequest& initial_request = resource_->GetResourceRequest();

  if (resource_->Options().cors_handling_by_resource_fetcher !=
          kEnableCORSHandlingByResourceFetcher ||
      initial_request.GetFetchRequestMode() !=
          WebURLRequest::kFetchRequestModeCORS)
    return CORSStatus::kNotApplicable;

  // Use the original response instead of the 304 response for a successful
  // revalidation.
  const ResourceResponse& response_for_access_control =
      (resource_->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource_->GetResponse()
          : response;

  WebCORS::AccessStatus cors_status =
      WebCORS::CheckAccess(response_for_access_control.Url(),
                           response_for_access_control.HttpStatusCode(),
                           response_for_access_control.HttpHeaderFields(),
                           initial_request.GetFetchCredentialsMode(),
                           WebSecurityOrigin(source_origin));

  if (cors_status == WebCORS::AccessStatus::kAccessAllowed)
    return CORSStatus::kSuccessful;

  String resource_type = Resource::ResourceTypeToString(
      resource_->GetType(), resource_->Options().initiator_info.name);
  error_msg.Append("Access to ");
  error_msg.Append(resource_type);
  error_msg.Append(" at '");
  error_msg.Append(response.Url().GetString());
  error_msg.Append("' from origin '");
  error_msg.Append(source_origin->ToString());
  error_msg.Append("' has been blocked by CORS policy: ");
  error_msg.Append(WebCORS::AccessControlErrorString(
      cors_status, response_for_access_control.HttpStatusCode(),
      response_for_access_control.HttpHeaderFields(),
      WebSecurityOrigin(source_origin), initial_request.GetRequestContext()));

  return CORSStatus::kFailed;
}

void ResourceLoader::DidReceiveResponse(
    const WebURLResponse& web_url_response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!web_url_response.IsNull());

  Resource::Type resource_type = resource_->GetType();

  const ResourceRequest& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  WebURLRequest::RequestContext request_context =
      initial_request.GetRequestContext();
  WebURLRequest::FetchRequestMode fetch_request_mode =
      initial_request.GetFetchRequestMode();

  const ResourceLoaderOptions& options = resource_->Options();

  const ResourceResponse& response = web_url_response.ToResourceResponse();

  // Later, CORS results should already be in the response we get from the
  // browser at this point.
  StringBuilder cors_error_msg;
  resource_->SetCORSStatus(DetermineCORSStatus(response, cors_error_msg));

  if (response.WasFetchedViaServiceWorker()) {
    if (options.cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        fetch_request_mode == WebURLRequest::kFetchRequestModeCORS &&
        response.WasFallbackRequiredByServiceWorker()) {
      ResourceRequest last_request = resource_->LastResourceRequest();
      DCHECK_EQ(last_request.GetServiceWorkerMode(),
                WebURLRequest::ServiceWorkerMode::kAll);
      // This code handles the case when a regular controlling service worker
      // doesn't handle a cross origin request. When this happens we still want
      // to give foreign fetch a chance to handle the request, so only skip the
      // controlling service worker for the fallback request. This is currently
      // safe because of http://crbug.com/604084 the
      // wasFallbackRequiredByServiceWorker flag is never set when foreign fetch
      // handled a request.
      if (!Context().ShouldLoadNewResource(resource_type)) {
        // Cancel the request if we should not trigger a reload now.
        HandleError(ResourceError::CancelledError(response.Url()));
        return;
      }
      last_request.SetServiceWorkerMode(
          WebURLRequest::ServiceWorkerMode::kForeign);
      Restart(last_request);
      return;
    }

    // If the response is fetched via ServiceWorker, the original URL of the
    // response could be different from the URL of the request. We check the URL
    // not to load the resources which are forbidden by the page CSP.
    // https://w3c.github.io/webappsec-csp/#should-block-response
    const KURL& original_url = response.OriginalURLViaServiceWorker();
    if (!original_url.IsEmpty()) {
      // CanRequest() below only checks enforced policies: check report-only
      // here to ensure violations are sent.
      Context().CheckCSPForRequest(
          request_context, original_url, options,
          SecurityViolationReportingPolicy::kReport,
          ResourceRequest::RedirectStatus::kFollowedRedirect);

      ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
          resource_type, initial_request, original_url, options,
          SecurityViolationReportingPolicy::kReport,
          FetchParameters::kUseDefaultOriginRestrictionForType,
          ResourceRequest::RedirectStatus::kFollowedRedirect);
      if (blocked_reason != ResourceRequestBlockedReason::kNone) {
        HandleError(ResourceError::CancelledDueToAccessCheckError(
            original_url, blocked_reason));
        return;
      }
    }
  } else if (options.cors_handling_by_resource_fetcher ==
                 kEnableCORSHandlingByResourceFetcher &&
             fetch_request_mode == WebURLRequest::kFetchRequestModeCORS) {
    if (!resource_->IsSameOriginOrCORSSuccessful()) {
      if (!resource_->IsUnusedPreload())
        Context().AddErrorConsoleMessage(cors_error_msg.ToString(),
                                         FetchContext::kJSSource);

      // Redirects can change the response URL different from one of request.
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response.Url(), ResourceRequestBlockedReason::kOther));
      return;
    }
  }

  // FrameType never changes during the lifetime of a request.
  Context().DispatchDidReceiveResponse(
      resource_->Identifier(), response, initial_request.GetFrameType(),
      request_context, resource_,
      FetchContext::ResourceResponseType::kNotFromMemoryCache);

  resource_->ResponseReceived(response, std::move(handle));
  if (!resource_->Loader())
    return;

  if (response.HttpStatusCode() >= 400 &&
      !resource_->ShouldIgnoreHTTPStatusCodeErrors())
    HandleError(ResourceError::CancelledError(response.Url()));
}

void ResourceLoader::DidReceiveResponse(const WebURLResponse& response) {
  DidReceiveResponse(response, nullptr);
}

void ResourceLoader::DidDownloadData(int length, int encoded_data_length) {
  Context().DispatchDidDownloadData(resource_->Identifier(), length,
                                    encoded_data_length);
  resource_->DidDownloadData(length);
}

void ResourceLoader::DidReceiveData(const char* data, int length) {
  CHECK_GE(length, 0);

  Context().DispatchDidReceiveData(resource_->Identifier(), data, length);
  resource_->AppendData(data, length);
}

void ResourceLoader::DidReceiveTransferSizeUpdate(int transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  Context().DispatchDidReceiveEncodedData(resource_->Identifier(),
                                          transfer_size_diff);
}

void ResourceLoader::DidFinishLoadingFirstPartInMultipart() {
  network_instrumentation::EndResourceLoad(
      resource_->Identifier(),
      network_instrumentation::RequestOutcome::kSuccess);

  fetcher_->HandleLoaderFinish(resource_.Get(), 0,
                               ResourceFetcher::kDidFinishFirstPartInMultipart);
}

void ResourceLoader::DidFinishLoading(double finish_time,
                                      int64_t encoded_data_length,
                                      int64_t encoded_body_length,
                                      int64_t decoded_body_length) {
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);

  keepalive_.Clear();
  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule);
  loader_.reset();

  network_instrumentation::EndResourceLoad(
      resource_->Identifier(),
      network_instrumentation::RequestOutcome::kSuccess);

  fetcher_->HandleLoaderFinish(resource_.Get(), finish_time,
                               ResourceFetcher::kDidFinishLoading);
}

void ResourceLoader::DidFail(const WebURLError& error,
                             int64_t encoded_data_length,
                             int64_t encoded_body_length,
                             int64_t decoded_body_length) {
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);
  HandleError(error);
}

void ResourceLoader::HandleError(const ResourceError& error) {
  if (is_cache_aware_loading_activated_ && error.IsCacheMiss() &&
      Context().ShouldLoadNewResource(resource_->GetType())) {
    resource_->WillReloadAfterDiskCacheMiss();
    is_cache_aware_loading_activated_ = false;
    Restart(resource_->GetResourceRequest());
    return;
  }

  keepalive_.Clear();
  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule);
  loader_.reset();

  network_instrumentation::EndResourceLoad(
      resource_->Identifier(), network_instrumentation::RequestOutcome::kFail);

  fetcher_->HandleLoaderError(resource_.Get(), error);
}

void ResourceLoader::RequestSynchronously(const ResourceRequest& request) {
  // downloadToFile is not supported for synchronous requests.
  DCHECK(!request.DownloadToFile());
  DCHECK(loader_);
  DCHECK_EQ(request.Priority(), kResourceLoadPriorityHighest);

  WrappedResourceRequest request_in(request);
  WebURLResponse response_out;
  WebURLError error_out;
  WebData data_out;
  int64_t encoded_data_length = WebURLLoaderClient::kUnknownEncodedDataLength;
  int64_t encoded_body_length = 0;
  loader_->LoadSynchronously(request_in, response_out, error_out, data_out,
                             encoded_data_length, encoded_body_length);

  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!loader_)
    return;
  int64_t decoded_body_length = data_out.size();
  if (error_out.reason) {
    DidFail(error_out, encoded_data_length, encoded_body_length,
            decoded_body_length);
    return;
  }
  DidReceiveResponse(response_out);
  if (!loader_)
    return;
  DCHECK_GE(response_out.ToResourceResponse().EncodedBodyLength(), 0);

  // Follow the async case convention of not calling DidReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (data_out.size()) {
    data_out.ForEachSegment([this](const char* segment, size_t segment_size,
                                   size_t segment_offset) {
      Context().DispatchDidReceiveData(resource_->Identifier(), segment,
                                       segment_size);
      return true;
    });
    resource_->SetResourceBuffer(data_out);
  }
  DidFinishLoading(MonotonicallyIncreasingTime(), encoded_data_length,
                   encoded_body_length, decoded_body_length);
}

void ResourceLoader::Dispose() {
  loader_ = nullptr;

  // Release() should be called to release |scheduler_client_id_| beforehand in
  // DidFinishLoading() or DidFail(), but when a timer to call Cancel() is
  // ignored due to GC, this case happens. We just release here because we can
  // not schedule another request safely. See crbug.com/675947.
  if (scheduler_client_id_ != ResourceLoadScheduler::kInvalidClientId)
    Release(ResourceLoadScheduler::ReleaseOption::kReleaseOnly);
}

void ResourceLoader::ActivateCacheAwareLoadingIfNeeded(
    const ResourceRequest& request) {
  DCHECK(!is_cache_aware_loading_activated_);

  if (resource_->Options().cache_aware_loading_enabled !=
      kIsCacheAwareLoadingEnabled)
    return;

  // Synchronous requests are not supported.
  if (resource_->Options().synchronous_policy == kRequestSynchronously)
    return;

  // Don't activate on Resource revalidation.
  if (resource_->IsCacheValidator())
    return;

  // Don't activate if cache policy is explicitly set.
  if (request.GetCachePolicy() != WebCachePolicy::kUseProtocolCachePolicy)
    return;

  is_cache_aware_loading_activated_ = true;
}

bool ResourceLoader::GetKeepalive() const {
  return resource_->GetResourceRequest().GetKeepalive();
}

}  // namespace blink
