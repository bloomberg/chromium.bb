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
                                       Resource* resource) {
  return new ResourceLoader(fetcher, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher, Resource* resource)
    : fetcher_(fetcher),
      resource_(resource),
      is_cache_aware_loading_activated_(false) {
  DCHECK(resource_);
  DCHECK(fetcher_);

  resource_->SetLoader(this);
}

ResourceLoader::~ResourceLoader() {}

DEFINE_TRACE(ResourceLoader) {
  visitor->Trace(fetcher_);
  visitor->Trace(resource_);
}

void ResourceLoader::Start(const ResourceRequest& request) {
  DCHECK(!loader_);

  if (resource_->Options().synchronous_policy == kRequestSynchronously &&
      Context().DefersLoading()) {
    Cancel();
    return;
  }

  loader_ = fetcher_->Context().CreateURLLoader(request);
  DCHECK(loader_);
  loader_->SetDefersLoading(Context().DefersLoading());

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

void ResourceLoader::Restart(const ResourceRequest& request) {
  CHECK_EQ(resource_->Options().synchronous_policy, kRequestAsynchronously);

  loader_.reset();
  Start(request);
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

  ResourceRequest& new_request(passed_new_request.ToMutableResourceRequest());
  const ResourceResponse& redirect_response(
      passed_redirect_response.ToResourceResponse());

  new_request.SetRedirectStatus(
      ResourceRequest::RedirectStatus::kFollowedRedirect);

  const KURL original_url = new_request.Url();

  if (!IsManualRedirectFetchRequest(resource_->GetResourceRequest())) {
    ResourceRequestBlockedReason blocked_reason = Context().CanFollowRedirect(
        resource_->GetType(), new_request, new_request.Url(),
        resource_->Options(),
        /* Don't send security violation reports for unused preloads */
        (resource_->IsUnusedPreload()
             ? SecurityViolationReportingPolicy::kSuppressReporting
             : SecurityViolationReportingPolicy::kReport),
        FetchParameters::kUseDefaultOriginRestrictionForType);
    if (blocked_reason != ResourceRequestBlockedReason::kNone) {
      CancelForRedirectAccessCheckError(new_request.Url(), blocked_reason);
      return false;
    }

    if (resource_->Options().cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        resource_->GetResourceRequest().GetFetchRequestMode() ==
            WebURLRequest::kFetchRequestModeCORS) {
      RefPtr<SecurityOrigin> source_origin =
          resource_->Options().security_origin;
      if (!source_origin.Get())
        source_origin = Context().GetSecurityOrigin();

      String error_message;
      if (!CrossOriginAccessControl::HandleRedirect(
              source_origin, new_request, redirect_response,
              resource_->GetResourceRequest().GetFetchCredentialsMode(),
              resource_->MutableOptions(), error_message)) {
        resource_->SetCORSFailed();
        Context().AddConsoleMessage(error_message);
        CancelForRedirectAccessCheckError(new_request.Url(),
                                          ResourceRequestBlockedReason::kOther);
        return false;
      }
    }
    if (resource_->GetType() == Resource::kImage &&
        fetcher_->ShouldDeferImageLoad(new_request.Url())) {
      CancelForRedirectAccessCheckError(new_request.Url(),
                                        ResourceRequestBlockedReason::kOther);
      return false;
    }
  }

  bool cross_origin = !SecurityOrigin::AreSameSchemeHostPort(
      redirect_response.Url(), new_request.Url());
  fetcher_->RecordResourceTimingOnRedirect(resource_.Get(), redirect_response,
                                           cross_origin);

  if (resource_->Options().cors_handling_by_resource_fetcher ==
          kEnableCORSHandlingByResourceFetcher &&
      resource_->GetResourceRequest().GetFetchRequestMode() ==
          WebURLRequest::kFetchRequestModeCORS) {
    bool allow_stored_credentials = false;
    switch (new_request.GetFetchCredentialsMode()) {
      case WebURLRequest::kFetchCredentialsModeOmit:
        break;
      case WebURLRequest::kFetchCredentialsModeSameOrigin:
        allow_stored_credentials = !resource_->Options().cors_flag;
        break;
      case WebURLRequest::kFetchCredentialsModeInclude:
      case WebURLRequest::kFetchCredentialsModePassword:
        allow_stored_credentials = true;
        break;
    }
    new_request.SetAllowStoredCredentials(allow_stored_credentials);
  }

  Context().PrepareRequest(new_request,
                           FetchContext::RedirectType::kForRedirect);
  Context().DispatchWillSendRequest(resource_->Identifier(), new_request,
                                    redirect_response,
                                    resource_->Options().initiator_info);

  // ResourceFetcher::WillFollowRedirect() may rewrite the URL to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::WillSendRequest() and
  // RenderFrameImpl::WillSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, return false to make the
  // redirect fail.
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

ResourceRequestBlockedReason ResourceLoader::CanAccessResponse(
    Resource* resource,
    const ResourceResponse& response) const {
  // Redirects can change the response URL different from one of request.
  bool unused_preload = resource->IsUnusedPreload();
  ResourceRequestBlockedReason blocked_reason = Context().CanRequest(
      resource->GetType(), resource->GetResourceRequest(), response.Url(),
      resource->Options(),
      /* Don't send security violation reports for unused preloads */
      (unused_preload ? SecurityViolationReportingPolicy::kSuppressReporting
                      : SecurityViolationReportingPolicy::kReport),
      FetchParameters::kUseDefaultOriginRestrictionForType);
  if (blocked_reason != ResourceRequestBlockedReason::kNone)
    return blocked_reason;

  SecurityOrigin* source_origin = resource->Options().security_origin.Get();
  if (!source_origin)
    source_origin = Context().GetSecurityOrigin();

  if (source_origin->CanRequestNoSuborigin(response.Url()))
    return ResourceRequestBlockedReason::kNone;

  // Use the original response instead of the 304 response for a successful
  // revaldiation.
  const ResourceResponse& response_for_access_control =
      (resource->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource->GetResponse()
          : response;

  CrossOriginAccessControl::AccessStatus cors_status =
      CrossOriginAccessControl::CheckAccess(
          response_for_access_control,
          resource->GetResourceRequest().GetFetchCredentialsMode(),
          source_origin);
  if (cors_status != CrossOriginAccessControl::kAccessAllowed) {
    resource->SetCORSFailed();
    if (!unused_preload) {
      String resource_type = Resource::ResourceTypeToString(
          resource->GetType(), resource->Options().initiator_info.name);
      StringBuilder builder;
      builder.Append("Access to ");
      builder.Append(resource_type);
      builder.Append(" at '");
      builder.Append(response.Url().GetString());
      builder.Append("' from origin '");
      builder.Append(source_origin->ToString());
      builder.Append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::AccessControlErrorString(
          builder, cors_status, response_for_access_control, source_origin,
          resource->LastResourceRequest().GetRequestContext());
      Context().AddConsoleMessage(builder.ToString());
    }
    return ResourceRequestBlockedReason::kOther;
  }
  return ResourceRequestBlockedReason::kNone;
}

void ResourceLoader::DidReceiveResponse(
    const WebURLResponse& web_url_response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!web_url_response.IsNull());

  const ResourceResponse& response = web_url_response.ToResourceResponse();

  if (response.WasFetchedViaServiceWorker()) {
    if (resource_->Options().cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        resource_->GetResourceRequest().GetFetchRequestMode() ==
            WebURLRequest::kFetchRequestModeCORS &&
        response.WasFallbackRequiredByServiceWorker()) {
      ResourceRequest request = resource_->LastResourceRequest();
      DCHECK_EQ(request.GetServiceWorkerMode(),
                WebURLRequest::ServiceWorkerMode::kAll);
      // This code handles the case when a regular controlling service worker
      // doesn't handle a cross origin request. When this happens we still want
      // to give foreign fetch a chance to handle the request, so only skip the
      // controlling service worker for the fallback request. This is currently
      // safe because of http://crbug.com/604084 the
      // wasFallbackRequiredByServiceWorker flag is never set when foreign fetch
      // handled a request.
      if (!Context().ShouldLoadNewResource(resource_->GetType())) {
        // Cancel the request if we should not trigger a reload now.
        HandleError(ResourceError::CancelledError(response.Url()));
        return;
      }
      request.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kForeign);
      Restart(request);
      return;
    }

    // If the response is fetched via ServiceWorker, the original URL of the
    // response could be different from the URL of the request. We check the URL
    // not to load the resources which are forbidden by the page CSP.
    // https://w3c.github.io/webappsec-csp/#should-block-response
    const KURL& original_url = response.OriginalURLViaServiceWorker();
    if (!original_url.IsEmpty()) {
      ResourceRequestBlockedReason blocked_reason = Context().AllowResponse(
          resource_->GetType(), resource_->GetResourceRequest(), original_url,
          resource_->Options());
      if (blocked_reason != ResourceRequestBlockedReason::kNone) {
        HandleError(ResourceError::CancelledDueToAccessCheckError(
            original_url, blocked_reason));
        return;
      }
    }
  } else if (resource_->Options().cors_handling_by_resource_fetcher ==
                 kEnableCORSHandlingByResourceFetcher &&
             resource_->GetResourceRequest().GetFetchRequestMode() ==
                 WebURLRequest::kFetchRequestModeCORS) {
    ResourceRequestBlockedReason blocked_reason =
        CanAccessResponse(resource_, response);
    if (blocked_reason != ResourceRequestBlockedReason::kNone) {
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response.Url(), blocked_reason));
      return;
    }
  }

  Context().DispatchDidReceiveResponse(
      resource_->Identifier(), response,
      resource_->GetResourceRequest().GetFrameType(),
      resource_->GetResourceRequest().GetRequestContext(), resource_,
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

}  // namespace blink
