/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_H_

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "ui/base/page_transition_types.h"

// TODO(crbug.com/922875): Need foo.mojom.shared-forward.h.
namespace network {
namespace mojom {
enum class CorsPreflightPolicy : int32_t;
enum class CredentialsMode : int32_t;
enum class RedirectMode : int32_t;
enum class ReferrerPolicy : int32_t;
enum class RequestMode : int32_t;
enum class RequestDestination : int32_t;
}  // namespace mojom

class OptionalTrustTokenParams;
}  // namespace network

namespace net {
class SiteForCookies;
}  // namespace net

namespace blink {

namespace mojom {
enum class FetchCacheMode : int32_t;
enum class RequestContextType : int32_t;
enum class RequestContextFrameType : int32_t;
}  // namespace mojom

class ResourceRequest;
class WebHTTPBody;
class WebHTTPHeaderVisitor;
class WebURLRequestExtraData;
class WebSecurityOrigin;
class WebString;
class WebURL;

class WebURLRequest {
 public:
  // The enum values should remain synchronized with the enum
  // WebURLRequestPriority in tools/metrics/histograms.enums.xml.
  enum class Priority {
    kUnresolved = -1,
    kVeryLow,
    kLow,
    kMedium,
    kHigh,
    kVeryHigh,
    kLowest = kVeryLow,
    kHighest = kVeryHigh,
  };

  BLINK_PLATFORM_EXPORT ~WebURLRequest();
  BLINK_PLATFORM_EXPORT WebURLRequest();
  WebURLRequest(const WebURLRequest&) = delete;
  BLINK_PLATFORM_EXPORT WebURLRequest(WebURLRequest&&);
  BLINK_PLATFORM_EXPORT explicit WebURLRequest(const WebURL&);
  WebURLRequest& operator=(const WebURLRequest&) = delete;
  BLINK_PLATFORM_EXPORT WebURLRequest& operator=(WebURLRequest&&);
  BLINK_PLATFORM_EXPORT void CopyFrom(const WebURLRequest&);

  BLINK_PLATFORM_EXPORT bool IsNull() const;

  BLINK_PLATFORM_EXPORT WebURL Url() const;
  BLINK_PLATFORM_EXPORT void SetUrl(const WebURL&);

  // Used to implement third-party cookie blocking.
  BLINK_PLATFORM_EXPORT const net::SiteForCookies& SiteForCookies() const;
  BLINK_PLATFORM_EXPORT void SetSiteForCookies(const net::SiteForCookies&);

  BLINK_PLATFORM_EXPORT absl::optional<WebSecurityOrigin> TopFrameOrigin()
      const;
  BLINK_PLATFORM_EXPORT void SetTopFrameOrigin(const WebSecurityOrigin&);

  // https://fetch.spec.whatwg.org/#concept-request-origin
  BLINK_PLATFORM_EXPORT WebSecurityOrigin RequestorOrigin() const;
  BLINK_PLATFORM_EXPORT void SetRequestorOrigin(const WebSecurityOrigin&);

  // The origin of the isolated world - set if this is a fetch/XHR initiated by
  // an isolated world.
  BLINK_PLATFORM_EXPORT WebSecurityOrigin IsolatedWorldOrigin() const;

  // Controls whether user name, password, and cookies may be sent with the
  // request.
  BLINK_PLATFORM_EXPORT bool AllowStoredCredentials() const;
  BLINK_PLATFORM_EXPORT void SetAllowStoredCredentials(bool);

  BLINK_PLATFORM_EXPORT mojom::FetchCacheMode GetCacheMode() const;
  BLINK_PLATFORM_EXPORT void SetCacheMode(mojom::FetchCacheMode);

  BLINK_PLATFORM_EXPORT base::TimeDelta TimeoutInterval() const;

  BLINK_PLATFORM_EXPORT WebString HttpMethod() const;
  BLINK_PLATFORM_EXPORT void SetHttpMethod(const WebString&);

  BLINK_PLATFORM_EXPORT WebString HttpHeaderField(const WebString& name) const;
  // It's not possible to set the referrer header using this method. Use
  // SetReferrerString instead.
  BLINK_PLATFORM_EXPORT void SetHttpHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void AddHttpHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void ClearHttpHeaderField(const WebString& name);
  BLINK_PLATFORM_EXPORT void VisitHttpHeaderFields(WebHTTPHeaderVisitor*) const;

  BLINK_PLATFORM_EXPORT WebHTTPBody HttpBody() const;
  BLINK_PLATFORM_EXPORT void SetHttpBody(const WebHTTPBody&);

  BLINK_PLATFORM_EXPORT WebHTTPBody AttachedCredential() const;
  BLINK_PLATFORM_EXPORT void SetAttachedCredential(const WebHTTPBody&);

  // Controls whether upload progress events are generated when a request
  // has a body.
  BLINK_PLATFORM_EXPORT bool ReportUploadProgress() const;
  BLINK_PLATFORM_EXPORT void SetReportUploadProgress(bool);

  BLINK_PLATFORM_EXPORT mojom::RequestContextType GetRequestContext() const;
  BLINK_PLATFORM_EXPORT void SetRequestContext(mojom::RequestContextType);

  BLINK_PLATFORM_EXPORT network::mojom::RequestDestination
  GetRequestDestination() const;
  BLINK_PLATFORM_EXPORT void SetRequestDestination(
      network::mojom::RequestDestination);

  BLINK_PLATFORM_EXPORT void SetReferrerString(const WebString& referrer);
  BLINK_PLATFORM_EXPORT void SetReferrerPolicy(
      network::mojom::ReferrerPolicy referrer_policy);

  BLINK_PLATFORM_EXPORT WebString ReferrerString() const;
  BLINK_PLATFORM_EXPORT network::mojom::ReferrerPolicy GetReferrerPolicy()
      const;

  // Sets an HTTP origin header if it is empty and the HTTP method of the
  // request requires it.
  BLINK_PLATFORM_EXPORT void SetHttpOriginIfNeeded(const WebSecurityOrigin&);

  // True if the request was user initiated.
  BLINK_PLATFORM_EXPORT bool HasUserGesture() const;
  BLINK_PLATFORM_EXPORT void SetHasUserGesture(bool);

  BLINK_PLATFORM_EXPORT bool HasTextFragmentToken() const;

  // A consumer controlled value intended to be used to identify the
  // requestor.
  BLINK_PLATFORM_EXPORT int RequestorID() const;
  BLINK_PLATFORM_EXPORT void SetRequestorID(int);

  // If true, the client expects to receive the raw response pipe. Similar to
  // UseStreamOnResponse but the stream will be a mojo DataPipe rather than a
  // WebDataConsumerHandle.
  // If the request is fetched synchronously the response will instead be piped
  // to a blob if this flag is set to true.
  BLINK_PLATFORM_EXPORT bool PassResponsePipeToClient() const;

  // True if the requestor wants to receive the response body as a stream.
  BLINK_PLATFORM_EXPORT bool UseStreamOnResponse() const;
  BLINK_PLATFORM_EXPORT void SetUseStreamOnResponse(bool);

  // True if the request can work after the fetch group is terminated.
  BLINK_PLATFORM_EXPORT bool GetKeepalive() const;
  BLINK_PLATFORM_EXPORT void SetKeepalive(bool);

  // True if the service workers should not get events for the request.
  BLINK_PLATFORM_EXPORT bool GetSkipServiceWorker() const;
  BLINK_PLATFORM_EXPORT void SetSkipServiceWorker(bool);

  // True if corresponding AppCache group should be resetted.
  BLINK_PLATFORM_EXPORT bool ShouldResetAppCache() const;
  BLINK_PLATFORM_EXPORT void SetShouldResetAppCache(bool);

  // The request mode which will be passed to the ServiceWorker.
  BLINK_PLATFORM_EXPORT network::mojom::RequestMode GetMode() const;
  BLINK_PLATFORM_EXPORT void SetMode(network::mojom::RequestMode);

  // True if the request is for a favicon.
  BLINK_PLATFORM_EXPORT bool GetFavicon() const;
  BLINK_PLATFORM_EXPORT void SetFavicon(bool);

  // The credentials mode which will be passed to the ServiceWorker.
  BLINK_PLATFORM_EXPORT network::mojom::CredentialsMode GetCredentialsMode()
      const;
  BLINK_PLATFORM_EXPORT void SetCredentialsMode(
      network::mojom::CredentialsMode);

  // The redirect mode which is used in Fetch API.
  BLINK_PLATFORM_EXPORT network::mojom::RedirectMode GetRedirectMode() const;
  BLINK_PLATFORM_EXPORT void SetRedirectMode(network::mojom::RedirectMode);

  // The integrity which is used in Fetch API.
  BLINK_PLATFORM_EXPORT WebString GetFetchIntegrity() const;
  BLINK_PLATFORM_EXPORT void SetFetchIntegrity(const WebString&);

  // Extra data associated with the underlying resource request. Resource
  // requests can be copied. If non-null, each copy of a resource requests
  // holds a pointer to the extra data, and the extra data pointer will be
  // deleted when the last resource request is destroyed. Setting the extra
  // data pointer will cause the underlying resource request to be
  // dissociated from any existing non-null extra data pointer.
  BLINK_PLATFORM_EXPORT const scoped_refptr<WebURLRequestExtraData>&
  GetURLRequestExtraData() const;
  BLINK_PLATFORM_EXPORT void SetURLRequestExtraData(
      scoped_refptr<WebURLRequestExtraData>);

  // The request is downloaded to the network cache, but not rendered or
  // executed.
  BLINK_PLATFORM_EXPORT bool IsDownloadToNetworkCacheOnly() const;
  BLINK_PLATFORM_EXPORT void SetDownloadToNetworkCacheOnly(bool);

  BLINK_PLATFORM_EXPORT Priority GetPriority() const;
  BLINK_PLATFORM_EXPORT void SetPriority(Priority);

  BLINK_PLATFORM_EXPORT network::mojom::CorsPreflightPolicy
  GetCorsPreflightPolicy() const;

  // If this request was created from an anchor with a download attribute, this
  // is the value provided there.
  BLINK_PLATFORM_EXPORT absl::optional<WebString> GetSuggestedFilename() const;

  // Returns true if this request is tagged as an ad. This is done using various
  // heuristics so it is not expected to be 100% accurate.
  BLINK_PLATFORM_EXPORT bool IsAdResource() const;

  // Should be set to true if this request (including redirects) should be
  // upgraded to HTTPS due to an Upgrade-Insecure-Requests requirement.
  BLINK_PLATFORM_EXPORT void SetUpgradeIfInsecure(bool);

  // Returns true if request (including redirects) should be upgraded to HTTPS
  // due to an Upgrade-Insecure-Requests requirement.
  BLINK_PLATFORM_EXPORT bool UpgradeIfInsecure() const;

  BLINK_PLATFORM_EXPORT bool SupportsAsyncRevalidation() const;

  // Returns true when the request is for revalidation.
  BLINK_PLATFORM_EXPORT bool IsRevalidating() const;

  // Returns the DevTools ID to throttle the network request.
  BLINK_PLATFORM_EXPORT const absl::optional<base::UnguessableToken>&
  GetDevToolsToken() const;

  // Remembers 'X-Requested-With' header value. Blink should not set this header
  // value until CORS checks are done to avoid running checks even against
  // headers that are internally set.
  BLINK_PLATFORM_EXPORT const WebString GetRequestedWithHeader() const;
  BLINK_PLATFORM_EXPORT void SetRequestedWithHeader(const WebString&);

  // Remembers 'Purpose' header value. Blink should not set this header value
  // until CORS checks are done to avoid running checks even against headers
  // that are internally set.
  BLINK_PLATFORM_EXPORT const WebString GetPurposeHeader() const;

  // https://fetch.spec.whatwg.org/#concept-request-window
  // See network::ResourceRequest::fetch_window_id for details.
  BLINK_PLATFORM_EXPORT const base::UnguessableToken& GetFetchWindowId() const;
  BLINK_PLATFORM_EXPORT void SetFetchWindowId(const base::UnguessableToken&);

  BLINK_PLATFORM_EXPORT absl::optional<WebString> GetDevToolsId() const;

  BLINK_PLATFORM_EXPORT int GetLoadFlagsForWebUrlRequest() const;

  BLINK_PLATFORM_EXPORT bool IsFromOriginDirtyStyleSheet() const;

  BLINK_PLATFORM_EXPORT bool IsSignedExchangePrefetchCacheEnabled() const;

  BLINK_PLATFORM_EXPORT absl::optional<base::UnguessableToken>
  RecursivePrefetchToken() const;

  // Specifies a Trust Tokens protocol operation to execute alongside the
  // request's load (https://github.com/wicg/trust-token-api).
  BLINK_PLATFORM_EXPORT network::OptionalTrustTokenParams TrustTokenParams()
      const;

  BLINK_PLATFORM_EXPORT absl::optional<WebURL> WebBundleUrl() const;
  BLINK_PLATFORM_EXPORT absl::optional<base::UnguessableToken> WebBundleToken()
      const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT ResourceRequest& ToMutableResourceRequest();
  BLINK_PLATFORM_EXPORT const ResourceRequest& ToResourceRequest() const;

 protected:
  // Permit subclasses to set arbitrary ResourceRequest pointer as
  // |resource_request_|. |owned_resource_request_| is not set in this case.
  BLINK_PLATFORM_EXPORT explicit WebURLRequest(ResourceRequest&);
#endif

 private:
  // If this instance owns a ResourceRequest then |owned_resource_request_|
  // is non-null and |resource_request_| points to the ResourceRequest
  // instance it contains.
  std::unique_ptr<ResourceRequest> owned_resource_request_;

  // Should never be null.
  ResourceRequest* resource_request_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_H_
