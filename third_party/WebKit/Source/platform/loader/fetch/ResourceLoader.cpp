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
#include "platform/loader/fetch/CrossOriginAccessControl.h"
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
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"

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
    WebURLRequest& passed_new_request,
    const WebURLResponse& passed_redirect_response) {
  DCHECK(!passed_new_request.IsNull());
  DCHECK(!passed_redirect_response.IsNull());

  if (is_cache_aware_loading_activated_) {
    // Fail as cache miss if cached response is a redirect.
    HandleError(
        ResourceError::CacheMissError(resource_->LastResourceRequest().Url()));
    return false;
  }

  const ResourceRequest& request = resource_->GetResourceRequest();
  Resource::Type resource_type = resource_->GetType();
  const ResourceLoaderOptions& options = resource_->Options();

  ResourceRequest& new_request(passed_new_request.ToMutableResourceRequest());
  const KURL& new_url = new_request.Url();

  const ResourceResponse& redirect_response(
      passed_redirect_response.ToResourceResponse());

  new_request.SetRedirectStatus(
      ResourceRequest::RedirectStatus::kFollowedRedirect);

  if (!IsManualRedirectFetchRequest(request)) {
    bool unused_preload = resource_->IsUnusedPreload();

    // Don't send security violation reports for unused preloads.
    SecurityViolationReportingPolicy reporting_policy =
        unused_preload ? SecurityViolationReportingPolicy::kSuppressReporting
                       : SecurityViolationReportingPolicy::kReport;

    // CanRequest() checks only enforced CSP, so check report-only here to
    // ensure that violations are sent.
    Context().CheckCSPForRequest(
        new_request, new_url, options, reporting_policy,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
        resource_type, new_request, new_url, options, reporting_policy,
        FetchParameters::kUseDefaultOriginRestrictionForType,
        new_request.GetRedirectStatus());
    if (blocked_reason != ResourceRequestBlockedReason::kNone) {
      CancelForRedirectAccessCheckError(new_url, blocked_reason);
      return false;
    }

    if (options.cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeCORS) {
      RefPtr<SecurityOrigin> source_origin = options.security_origin;
      if (!source_origin.Get())
        source_origin = Context().GetSecurityOrigin();

      String cors_error_msg;
      if (!CrossOriginAccessControl::HandleRedirect(
              source_origin, new_request, redirect_response,
              request.GetFetchCredentialsMode(), resource_->MutableOptions(),
              cors_error_msg)) {
        resource_->SetCORSStatus(CORSStatus::kFailed);

        if (!unused_preload)
          Context().AddConsoleMessage(cors_error_msg);

        CancelForRedirectAccessCheckError(new_url,
                                          ResourceRequestBlockedReason::kOther);
        return false;
      }
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
      request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeCORS) {
    bool allow_stored_credentials = false;
    switch (new_request.GetFetchCredentialsMode()) {
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
  // rewriting but currently we cannot. So, save the value here, compare it
  // with new_request.Url() after calling them, and return false to make the
  // redirect fail on mismatch.
  const KURL original_url = new_request.Url();

  Context().PrepareRequest(new_request,
                           FetchContext::RedirectType::kForRedirect);
  Context().DispatchWillSendRequest(resource_->Identifier(), new_request,
                                    redirect_response, options.initiator_info);

  if (new_request.Url() != original_url) {
    CancelForRedirectAccessCheckError(new_request.Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  if (!resource_->WillFollowRedirect(new_request, redirect_response)) {
    CancelForRedirectAccessCheckError(new_request.Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

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
    switch (response.ServiceWorkerResponseType()) {
      case mojom::FetchResponseType::kBasic:
      case mojom::FetchResponseType::kCORS:
      case mojom::FetchResponseType::kDefault:
      case mojom::FetchResponseType::kError:
        return CORSStatus::kServiceWorkerSuccessful;
      case mojom::FetchResponseType::kOpaque:
      case mojom::FetchResponseType::kOpaqueRedirect:
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

  if (resource_->Options().cors_handling_by_resource_fetcher !=
          kEnableCORSHandlingByResourceFetcher ||
      resource_->GetResourceRequest().GetFetchRequestMode() !=
          WebURLRequest::kFetchRequestModeCORS)
    return CORSStatus::kNotApplicable;

  // Use the original response instead of the 304 response for a successful
  // revalidation.
  const ResourceResponse& response_for_access_control =
      (resource_->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource_->GetResponse()
          : response;

  CrossOriginAccessControl::AccessStatus cors_status =
      CrossOriginAccessControl::CheckAccess(
          response_for_access_control,
          resource_->LastResourceRequest().GetFetchCredentialsMode(),
          source_origin);

  if (cors_status == CrossOriginAccessControl::AccessStatus::kAccessAllowed)
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

  CrossOriginAccessControl::AccessControlErrorString(
      error_msg, cors_status, response_for_access_control, source_origin,
      resource_->LastResourceRequest().GetRequestContext());

  return CORSStatus::kFailed;
}

void ResourceLoader::DidReceiveResponse(
    const WebURLResponse& web_url_response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!web_url_response.IsNull());

  const ResourceRequest& request = resource_->GetResourceRequest();
  const ResourceLoaderOptions& options = resource_->Options();
  Resource::Type resource_type = resource_->GetType();

  const ResourceResponse& response = web_url_response.ToResourceResponse();

  // Later, CORS results should already be in the response we get from the
  // browser at this point.
  StringBuilder cors_error_msg;
  resource_->SetCORSStatus(DetermineCORSStatus(response, cors_error_msg));

  if (response.WasFetchedViaServiceWorker()) {
    if (options.cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        request.GetFetchRequestMode() == WebURLRequest::kFetchRequestModeCORS &&
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
          request, original_url, options,
          SecurityViolationReportingPolicy::kReport,
          ResourceRequest::RedirectStatus::kFollowedRedirect);

      ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
          resource_type, request, original_url, options,
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
             request.GetFetchRequestMode() ==
                 WebURLRequest::kFetchRequestModeCORS) {
    bool unused_preload = resource_->IsUnusedPreload();

    // Redirects can change the response URL different from one of request.
    const KURL& response_url = response.Url();

    ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
        resource_type, request, response_url, options,
        /* Don't send security violation reports for unused preloads */
        (unused_preload ? SecurityViolationReportingPolicy::kSuppressReporting
                        : SecurityViolationReportingPolicy::kReport),
        FetchParameters::kUseDefaultOriginRestrictionForType,
        request.GetRedirectStatus());
    if (blocked_reason != ResourceRequestBlockedReason::kNone) {
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response_url, blocked_reason));
      return;
    }

    if (!resource_->IsSameOriginOrCORSSuccessful()) {
      if (!unused_preload)
        Context().AddConsoleMessage(cors_error_msg.ToString());

      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response_url, ResourceRequestBlockedReason::kOther));
      return;
    }
  }

  Context().DispatchDidReceiveResponse(
      resource_->Identifier(), response, request.GetFrameType(),
      request.GetRequestContext(), resource_,
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
    Context().DispatchDidReceiveData(resource_->Identifier(), data_out.Data(),
                                     data_out.size());
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
