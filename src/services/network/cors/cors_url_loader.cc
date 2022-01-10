// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/cors/cors_util.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "services/network/url_loader.h"
#include "services/network/url_loader_factory.h"
#include "url/url_util.h"

namespace network {

namespace cors {

namespace {

enum class PreflightRequiredReason {
  // TODO(https://crbug.com/1263483): Remove this.
  kExternalRequest,
  kPrivateNetworkAccess,
  kCorsWithForcedPreflightMode,
  kDisallowedMethod,
  kDisallowedHeader
};

// Returns absl::nullopt when a CORS preflight isn't needed. Otherwise
// returns the reason why a preflight is needed.
absl::optional<PreflightRequiredReason> NeedsPreflight(
    const ResourceRequest& request) {
  if (request.target_ip_address_space != mojom::IPAddressSpace::kUnknown) {
    // Force a preflight after a private network request was detected. See the
    // HTTP-no-service-worker fetch algorithm defined in the Private Network
    // Access spec:
    // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
    return PreflightRequiredReason::kPrivateNetworkAccess;
  }

  if (!IsCorsEnabledRequestMode(request.mode))
    return absl::nullopt;

  // TODO(https://crbug.com/1263483): Remove this.
  if (request.is_external_request)
    return PreflightRequiredReason::kExternalRequest;

  if (request.mode == mojom::RequestMode::kCorsWithForcedPreflight) {
    return PreflightRequiredReason::kCorsWithForcedPreflightMode;
  }

  if (request.cors_preflight_policy ==
      mojom::CorsPreflightPolicy::kPreventPreflight) {
    return absl::nullopt;
  }

  if (!IsCorsSafelistedMethod(request.method))
    return PreflightRequiredReason::kDisallowedMethod;

  if (!CorsUnsafeNotForbiddenRequestHeaderNames(
           request.headers.GetHeaderVector(), request.is_revalidating)
           .empty())
    return PreflightRequiredReason::kDisallowedHeader;

  return absl::nullopt;
}

base::Value NetLogCorsURLLoaderStartParams(const ResourceRequest& request) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", request.url.possibly_invalid_spec());
  dict.SetStringKey("method", request.method);
  dict.SetStringKey("headers", request.headers.ToString());
  dict.SetBoolKey("is_external_request", request.is_external_request);
  dict.SetBoolKey("is_revalidating", request.is_revalidating);
  std::string cors_preflight_policy;
  switch (request.cors_preflight_policy) {
    case mojom::CorsPreflightPolicy::kConsiderPreflight:
      cors_preflight_policy = "consider_preflight";
      break;
    case mojom::CorsPreflightPolicy::kPreventPreflight:
      cors_preflight_policy = "prevent_preflight";
      break;
  }
  dict.SetStringKey("cors_preflight_policy", cors_preflight_policy);
  return dict;
}

base::Value NetLogPreflightRequiredParams(
    absl::optional<PreflightRequiredReason> preflight_required_reason) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey("preflight_required", preflight_required_reason.has_value());
  if (preflight_required_reason) {
    std::string preflight_required_reason_param;
    switch (preflight_required_reason.value()) {
      // TODO(https://crbug.com/1263483): Remove this.
      case PreflightRequiredReason::kExternalRequest:
        preflight_required_reason_param = "external_request";
        break;
      case PreflightRequiredReason::kPrivateNetworkAccess:
        preflight_required_reason_param = "private_network_access";
        break;
      case PreflightRequiredReason::kCorsWithForcedPreflightMode:
        preflight_required_reason_param = "cors_with_forced_preflight_mode";
        break;
      case PreflightRequiredReason::kDisallowedMethod:
        preflight_required_reason_param = "disallowed_method";
        break;
      case PreflightRequiredReason::kDisallowedHeader:
        preflight_required_reason_param = "disallowed_header";
        break;
    }
    dict.SetStringKey("preflight_required_reason",
                      preflight_required_reason_param);
  }
  return dict;
}

// Returns the response tainting value
// (https://fetch.spec.whatwg.org/#concept-request-response-tainting) for a
// request and the CORS flag, as specified in
// https://fetch.spec.whatwg.org/#main-fetch.
// Keep this in sync with the identical function
// blink::cors::CalculateResponseTainting.
mojom::FetchResponseType CalculateResponseTainting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const absl::optional<url::Origin>& origin,
    const absl::optional<url::Origin>& isolated_world_origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList& origin_access_list) {
  if (url.SchemeIs(url::kDataScheme))
    return mojom::FetchResponseType::kBasic;

  if (cors_flag) {
    DCHECK(IsCorsEnabledRequestMode(request_mode));
    return mojom::FetchResponseType::kCors;
  }

  if (!origin) {
    // This is actually not defined in the fetch spec, but in this case CORS
    // is disabled so no one should care this value.
    return mojom::FetchResponseType::kBasic;
  }

  // OriginAccessList is in practice used to disable CORS for Chrome Extensions.
  // The extension origin can be found in either:
  // 1) `isolated_world_origin` (if this is a request from a content
  //    script;  in this case there is no point looking at (2) below.
  // 2) `origin` (if this is a request from an extension
  //    background page or from other extension frames).
  //
  // Note that similar code is present in OriginAccessList::CheckAccessState.
  //
  // TODO(lukasza): https://crbug.com/936310 and https://crbug.com/920638:
  // Once 1) there is no global OriginAccessList and 2) per-factory
  // OriginAccessList is only populated for URLLoaderFactory used by allowlisted
  // content scripts, then 3) there should no longer be a need to use origins as
  // a key in an OriginAccessList.
  const url::Origin& source_origin = isolated_world_origin.value_or(*origin);

  if (request_mode == mojom::RequestMode::kNoCors) {
    if (tainted_origin ||
        (!origin->IsSameOriginWith(url::Origin::Create(url)) &&
         origin_access_list.CheckAccessState(source_origin, url) !=
             OriginAccessList::AccessState::kAllowed)) {
      return mojom::FetchResponseType::kOpaque;
    }
  }
  return mojom::FetchResponseType::kBasic;
}

// Given a redirected-to URL, checks if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
absl::optional<CorsErrorStatus> CheckRedirectLocation(
    const GURL& url,
    mojom::RequestMode request_mode,
    const absl::optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted) {
  // If `actualResponse`’s location URL’s scheme is not an HTTP(S) scheme,
  // then return a network error.
  // This should be addressed in //net.

  // Note: The redirect count check is done elsewhere.

  const bool url_has_credentials = url.has_username() || url.has_password();
  // If `request`’s mode is "cors", `actualResponse`’s location URL includes
  // credentials, and either `request`’s tainted origin flag is set or
  // `request`’s origin is not same origin with `actualResponse`’s location
  // URL’s origin, then return a network error.
  DCHECK(!IsCorsEnabledRequestMode(request_mode) || origin);
  if (IsCorsEnabledRequestMode(request_mode) && url_has_credentials &&
      (tainted || !origin->IsSameOriginWith(url::Origin::Create(url)))) {
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);
  }

  // If CORS flag is set and `actualResponse`’s location URL includes
  // credentials, then return a network error.
  if (cors_flag && url_has_credentials)
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);

  return absl::nullopt;
}

constexpr const char kTimingAllowOrigin[] = "Timing-Allow-Origin";
}  // namespace

CorsURLLoader::CorsURLLoader(
    mojo::PendingReceiver<mojom::URLLoader> loader_receiver,
    int32_t process_id,
    int32_t request_id,
    uint32_t options,
    DeleteCallback delete_callback,
    const ResourceRequest& resource_request,
    bool ignore_isolated_world_origin,
    bool skip_cors_enabled_scheme_check,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory,
    URLLoaderFactory* sync_network_loader_factory,
    const OriginAccessList* origin_access_list,
    PreflightController* preflight_controller,
    const base::flat_set<std::string>* allowed_exempt_headers,
    bool allow_any_cors_exempt_header,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    const mojom::ClientSecurityState* factory_client_security_state)
    : receiver_(this, std::move(loader_receiver)),
      process_id_(process_id),
      request_id_(request_id),
      options_(options),
      delete_callback_(std::move(delete_callback)),
      network_loader_factory_(network_loader_factory),
      sync_network_loader_factory_(sync_network_loader_factory),
      request_(resource_request),
      forwarding_client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      origin_access_list_(origin_access_list),
      preflight_controller_(preflight_controller),
      allowed_exempt_headers_(allowed_exempt_headers),
      skip_cors_enabled_scheme_check_(skip_cors_enabled_scheme_check),
      allow_any_cors_exempt_header_(allow_any_cors_exempt_header),
      non_wildcard_request_headers_support_(
          non_wildcard_request_headers_support),
      isolation_info_(isolation_info),
      factory_client_security_state_(factory_client_security_state),
      devtools_observer_(std::move(devtools_observer)),
      // CORS preflight related events are logged in a series of URL_REQUEST
      // logs.
      net_log_(
          net::NetLogWithSource::Make(net::NetLog::Get(),
                                      net::NetLogSourceType::URL_REQUEST)) {
  if (ignore_isolated_world_origin)
    request_.isolated_world_origin = absl::nullopt;

  receiver_.set_disconnect_handler(
      base::BindOnce(&CorsURLLoader::OnMojoDisconnect, base::Unretained(this)));
  request_.net_log_create_info = net_log_.source();
  DCHECK(network_loader_factory_);
  DCHECK(origin_access_list_);
  DCHECK(preflight_controller_);
  DCHECK(allowed_exempt_headers_);
  SetCorsFlagIfNeeded();
}

CorsURLLoader::~CorsURLLoader() {
  // Reset pipes first to ignore possible subsequent callback invocations
  // caused by `network_loader_`
  network_client_receiver_.reset();
}

void CorsURLLoader::Start() {
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    // Username and password should be stripped in a CORS-enabled request.
    if (request_.url.has_username() || request_.url.has_password()) {
      GURL::Replacements replacements;
      replacements.SetUsernameStr("");
      replacements.SetPasswordStr("");
      request_.url = request_.url.ReplaceComponents(replacements);
    }
  }
  net_log_.BeginEvent(net::NetLogEventType::CORS_REQUEST,
                      [&] { return NetLogCorsURLLoaderStartParams(request_); });
  StartRequest();
}

void CorsURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  // If this is a navigation from a renderer, then its a service worker
  // passthrough of a navigation request.  Since this case uses manual
  // redirect mode FollowRedirect() should never be called.
  if (process_id_ != mojom::kBrowserProcessId &&
      request_.mode == mojom::RequestMode::kNavigate) {
    mojo::ReportBadMessage(
        "CorsURLLoader: navigate from non-browser-process should not call "
        "FollowRedirect");
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (!network_loader_ || !deferred_redirect_url_) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (new_url && (new_url->DeprecatedGetOriginAsURL() !=
                  deferred_redirect_url_->DeprecatedGetOriginAsURL())) {
    NOTREACHED() << "Can only change the URL within the same origin.";
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  deferred_redirect_url_.reset();

  // When the redirect mode is "error", the client is not expected to
  // call this function. Let's abort the request.
  if (request_.redirect_mode == mojom::RedirectMode::kError) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Does not allow modifying headers that are stored in `cors_exempt_headers`.
  for (const auto& header : modified_headers.GetHeaderVector()) {
    if (request_.cors_exempt_headers.HasHeader(header.key)) {
      LOG(WARNING) << "A client is trying to modify header value for '"
                   << header.key << "', but it is not permitted.";
      HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }
  }

  network::URLLoader::LogConcerningRequestHeaders(
      modified_headers, /*added_during_redirect=*/true);
  network::URLLoader::LogConcerningRequestHeaders(
      modified_cors_exempt_headers, /*added_during_redirect=*/true);

  for (const auto& name : removed_headers) {
    request_.headers.RemoveHeader(name);
    request_.cors_exempt_headers.RemoveHeader(name);
  }
  request_.headers.MergeFrom(modified_headers);

  if (!allow_any_cors_exempt_header_ &&
      !CorsURLLoaderFactory::IsValidCorsExemptHeaders(
          *allowed_exempt_headers_, modified_cors_exempt_headers)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }
  request_.cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);

  if (!AreRequestHeadersSafe(request_.headers)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  const std::string original_method = std::move(request_.method);
  request_.url = redirect_info_.new_url;
  request_.method = redirect_info_.new_method;
  request_.referrer = GURL(redirect_info_.new_referrer);
  request_.referrer_policy = redirect_info_.new_referrer_policy;
  request_.site_for_cookies = redirect_info_.new_site_for_cookies;

  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(request_.url));
  }

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;

  const bool original_fetch_cors_flag = fetch_cors_flag_;
  SetCorsFlagIfNeeded();

  // We cannot use FollowRedirect for a request with preflight (i.e., when
  // `fetch_cors_flag_` is true and `NeedsPreflight(request_)` is not nullopt).
  //
  // When `original_fetch_cors_flag` is false, `fetch_cors_flag_` is true and
  // `NeedsPreflight(request)` is nullopt, the net/ implementation won't attach
  // an "origin" header on redirect, as the original request didn't have one.
  //
  // When the request method is changed (due to 302 status code, for example),
  // the net/ implementation removes the origin header.
  //
  // In such cases we need to re-issue a request manually in order to attach the
  // correct origin header. For "no-cors" requests we rely on redirect logic in
  // net/ (specifically in net/url_request/redirect_util.cc).
  //
  // After both OOR-CORS and network service are fully shipped, we may be able
  // to remove the logic in net/.
  if ((fetch_cors_flag_ && NeedsPreflight(request_)) ||
      (!original_fetch_cors_flag && fetch_cors_flag_) ||
      (fetch_cors_flag_ && original_method != request_.method)) {
    DCHECK_NE(request_.mode, mojom::RequestMode::kNoCors);
    network_client_receiver_.reset();
    sync_client_receiver_factory_.InvalidateWeakPtrs();
    StartRequest();
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator,
      request_.isolated_world_origin, fetch_cors_flag_, tainted_,
      *origin_access_list_);
  network_loader_->FollowRedirect(removed_headers, modified_headers,
                                  modified_cors_exempt_headers, new_url);
}

void CorsURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void CorsURLLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void CorsURLLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

void CorsURLLoader::OnReceiveEarlyHints(mojom::EarlyHintsPtr early_hints) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  // Only forward Early Hints for navigation.
  if (request_.mode == mojom::RequestMode::kNavigate)
    forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void CorsURLLoader::OnReceiveResponse(mojom::URLResponseHeadPtr response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  // See 10.7.4 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
  const bool is_304_for_revalidation =
      request_.is_revalidating && response_head->headers &&
      response_head->headers->response_code() == 304;
  if (fetch_cors_flag_ && !is_304_for_revalidation) {
    const auto error_status = CheckAccessAndReportMetrics(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);

  response_head->response_type = response_tainting_;
  response_head->timing_allow_passed = !timing_allow_failed_flag_;
  response_head->has_authorization_covered_by_wildcard_on_preflight =
      has_authorization_covered_by_wildcard_;
  forwarding_client_->OnReceiveResponse(std::move(response_head));
}

void CorsURLLoader::OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                                      mojom::URLResponseHeadPtr response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  // If `CORS flag` is set and a CORS check for `request` and `response` returns
  // failure, then return a network error.
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    const auto error_status = CheckAccessAndReportMetrics(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);
    forwarding_client_->OnReceiveRedirect(redirect_info,
                                          std::move(response_head));
    return;
  }

  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);

  // Because we initiate a new request on redirect in some cases, we cannot
  // rely on the redirect logic in the network stack. Hence we need to
  // implement some logic in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch here.

  // If `request`’s redirect count is twenty, return a network error.
  // Increase `request`’s redirect count by one.
  if (redirect_count_++ == 20) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  const auto error_status = CheckRedirectLocation(
      redirect_info.new_url, request_.mode, request_.request_initiator,
      fetch_cors_flag_, tainted_);
  if (error_status) {
    HandleComplete(URLLoaderCompletionStatus(*error_status));
    return;
  }

  // If `actualResponse`’s status is not 303, `request`’s body is non-null, and
  // `request`’s body’s source is null, then return a network error.
  if (redirect_info.status_code != net::HTTP_SEE_OTHER &&
      network::URLLoader::HasFetchStreamingUploadBody(&request_)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  // If `actualResponse`’s location URL’s origin is not same origin with
  // `request`’s current url’s origin and `request`’s origin is not same origin
  // with `request`’s current url’s origin, then set `request`’s tainted origin
  // flag.
  if (request_.request_initiator &&
      (!url::Origin::Create(redirect_info.new_url)
            .IsSameOriginWith(url::Origin::Create(request_.url)) &&
       !request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(request_.url)))) {
    tainted_ = true;
  }

  // TODO(crbug.com/1073353): Implement the following:
  // If either `actualResponse`’s status is 301 or 302 and `request`’s method is
  // `POST`, or `actualResponse`’s status is 303, set `request`’s method to
  // `GET` and request’s body to null, and remove request-body-header name from
  // request's headers. Some of them are implemented in //net, but when we
  // create another request on exceptional redirect cases, such newly created
  // request doesn't reflect the spec comformant request modifications. See the
  // linked crbug for details. See also 4.4. HTTP-redirect fetch
  // (https://fetch.spec.whatwg.org/#http-redirect-fetch), step 11.

  // TODO(crbug.com/1073353): Implement the following:
  // Invoke `set request’s referrer policy on redirect` on `request` and
  // `actualResponse`. See 4.4. HTTP-redirect fetch
  // (https://fetch.spec.whatwg.org/#http-redirect-fetch), step 14.

  redirect_info_ = redirect_info;

  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    response_head->response_type = mojom::FetchResponseType::kOpaqueRedirect;
  } else {
    response_head->response_type = response_tainting_;
  }
  response_head->timing_allow_passed = !timing_allow_failed_flag_;
  forwarding_client_->OnReceiveRedirect(redirect_info,
                                        std::move(response_head));
}

void CorsURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CorsURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnReceiveCachedMetadata(std::move(data));
}

void CorsURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CorsURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CorsURLLoader::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  // `network_loader_` will call OnComplete at anytime when a problem happens
  // inside the URLLoader, e.g. on URLLoader::OnMojoDisconnect call. We need
  // to expect it also happens even during redirect handling.
  DCHECK(!deferred_redirect_url_ || status.error_code != net::OK);

  HandleComplete(status);
}

void CorsURLLoader::StartRequest() {
  if (fetch_cors_flag_ && !skip_cors_enabled_scheme_check_ &&
      !base::Contains(url::GetCorsEnabledSchemes(), request_.url.scheme())) {
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  // If the `CORS flag` is set, `httpRequest`’s method is neither `GET` nor
  // `HEAD`, or `httpRequest`’s mode is "websocket", then append
  // `Origin`/the result of serializing a request origin with `httpRequest`, to
  // `httpRequest`’s header list.
  //
  // We exclude navigation requests to keep the existing behavior.
  // TODO(yhirano): Reconsider this.
  if (request_.mode != network::mojom::RequestMode::kNavigate &&
      request_.request_initiator &&
      (fetch_cors_flag_ ||
       (request_.method != net::HttpRequestHeaders::kGetMethod &&
        request_.method != net::HttpRequestHeaders::kHeadMethod))) {
    if (tainted_) {
      request_.headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 url::Origin().Serialize());
    } else {
      request_.headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 request_.request_initiator->Serialize());
    }
  }

  if (fetch_cors_flag_ && request_.mode == mojom::RequestMode::kSameOrigin) {
    DCHECK(request_.request_initiator);
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kDisallowedByMode)));
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator,
      request_.isolated_world_origin, fetch_cors_flag_, tainted_,
      *origin_access_list_);

  // Note that even when `needs_preflight` holds we might not make a preflight
  // request. This happens when `fetch_cors_flag_` is false, e.g. when the
  // origin of the url is equal to the origin of the request, and the preflight
  // reason is not `kPrivateNetworkAccess`. In the case of a private network
  // access we always send a preflight, even for CORS-disabled requests.
  //
  // See the first step of the HTTP-no-service-worker fetch algorithm defined in
  // the Private Network Access spec:
  // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
  absl::optional<PreflightRequiredReason> needs_preflight =
      NeedsPreflight(request_);
  bool preflight_required =
      needs_preflight.has_value() &&
      (fetch_cors_flag_ ||
       *needs_preflight == PreflightRequiredReason::kPrivateNetworkAccess);
  net_log_.AddEvent(net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED, [&] {
    return NetLogPreflightRequiredParams(needs_preflight);
  });

  has_authorization_covered_by_wildcard_ = false;
  if (!preflight_required) {
    StartNetworkRequest();
    return;
  }

  // Clone the devtools observer only if the original request has a
  // |devtools_request_id|.
  mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
  if (devtools_observer_ && request_.devtools_request_id) {
    devtools_observer_->Clone(
        devtools_observer.InitWithNewPipeAndPassReceiver());
  }

  // Since we're doing a preflight, we won't reuse the original request. Cancel
  // it now to free up the socket.
  network_loader_.reset();

  preflight_controller_->PerformPreflightCheck(
      base::BindOnce(&CorsURLLoader::OnPreflightRequestComplete,
                     weak_factory_.GetWeakPtr()),
      request_,
      PreflightController::WithTrustedHeaderClient(
          options_ & mojom::kURLLoadOptionUseHeaderClient),
      non_wildcard_request_headers_support_,
      PreflightController::EnforcePrivateNetworkAccessHeader(
          !ShouldIgnorePrivateNetworkAccessErrors()),
      tainted_, net::NetworkTrafficAnnotationTag(traffic_annotation_),
      network_loader_factory_, isolation_info_, CloneClientSecurityState(),
      std::move(devtools_observer), net_log_);
}

void CorsURLLoader::ReportCorsErrorToDevTools(const CorsErrorStatus& status,
                                              bool is_warning) {
  DCHECK(devtools_observer_);

  devtools_observer_->OnCorsError(
      request_.devtools_request_id, request_.request_initiator,
      CloneClientSecurityState(), request_.url, status, is_warning);
}

absl::optional<URLLoaderCompletionStatus> CorsURLLoader::ConvertPreflightResult(
    int net_error,
    absl::optional<CorsErrorStatus> status) {
  if (net_error == net::OK) {
    DCHECK(!status) << *status;
    return absl::nullopt;
  }

  // `kInvalidResponse` is never returned by the preflight controller, so we use
  // it to record the case where there was a net error and no CORS error.
  auto histogram_error = mojom::CorsError::kInvalidResponse;
  if (status) {
    DCHECK(status->cors_error != mojom::CorsError::kInvalidResponse);
    histogram_error = status->cors_error;
  }

  if (should_ignore_preflight_errors_) {
    // Even if we ignore the error, record the warning in metrics and DevTools.
    base::UmaHistogramEnumeration(kPreflightWarningHistogramName,
                                  histogram_error);
    if (devtools_observer_) {
      if (!status) {
        // Set the resource IP address space to the target IP address space for
        // better error messages in DevTools. If the resource address space had
        // not matched, the request would likely have failed with
        // `CorsError::kInvalidPrivateNetwork`. If the error happened before we
        // ever obtained a connection to the remote endpoint, then this value
        // is incorrect - we cannot tell what value it would have been. Given
        // that this is used for debugging only, the slight incorrectness is
        // worth the increased debuggability.
        status = CorsErrorStatus(mojom::CorsError::kInvalidResponse,
                                 request_.target_ip_address_space,
                                 request_.target_ip_address_space);
      }

      ReportCorsErrorToDevTools(*status, /*is_warning=*/true);
    }

    return absl::nullopt;
  }

  base::UmaHistogramEnumeration(kPreflightErrorHistogramName, histogram_error);
  if (status) {
    return URLLoaderCompletionStatus(*std::move(status));
  }

  return URLLoaderCompletionStatus(net_error);
}

void CorsURLLoader::OnPreflightRequestComplete(
    int net_error,
    absl::optional<CorsErrorStatus> status,
    bool has_authorization_covered_by_wildcard) {
  has_authorization_covered_by_wildcard_ =
      has_authorization_covered_by_wildcard;

  absl::optional<URLLoaderCompletionStatus> completion_status =
      ConvertPreflightResult(net_error, std::move(status));
  if (completion_status) {
    HandleComplete(*std::move(completion_status));
    return;
  }

  StartNetworkRequest();
}

void CorsURLLoader::StartNetworkRequest() {
  // Here we overwrite the credentials mode sent to URLLoader because
  // network::URLLoader doesn't understand |kSameOrigin|.
  // TODO(crbug.com/943939): Fix this.
  auto original_credentials_mode = request_.credentials_mode;
  if (original_credentials_mode == mojom::CredentialsMode::kSameOrigin) {
    request_.credentials_mode =
        CalculateCredentialsFlag(original_credentials_mode, response_tainting_)
            ? mojom::CredentialsMode::kInclude
            : mojom::CredentialsMode::kOmit;
  }

  // Binding |this| as an unretained pointer is safe because
  // |network_client_receiver_| shares this object's lifetime.
  network_loader_.reset();
  if (sync_network_loader_factory_ &&
      base::FeatureList::IsEnabled(features::kURLLoaderSyncClient)) {
    sync_network_loader_factory_->CreateLoaderAndStartWithSyncClient(
        network_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, network_client_receiver_.BindNewPipeAndPassRemote(),
        sync_client_receiver_factory_.GetWeakPtr(), traffic_annotation_);
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        network_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, network_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
  }
  network_client_receiver_.set_disconnect_handler(
      base::BindOnce(&CorsURLLoader::OnMojoDisconnect, base::Unretained(this)));

  request_.credentials_mode = original_credentials_mode;
}

void CorsURLLoader::HandleComplete(const URLLoaderCompletionStatus& status) {
  if (request_.trust_token_params) {
    HistogramTrustTokenOperationNetError(request_.trust_token_params->type,
                                         status.trust_token_operation_status,
                                         status.error_code);
  }

  if (devtools_observer_ && status.cors_error_status) {
    ReportCorsErrorToDevTools(*status.cors_error_status);
  }

  // If we detect a private network access when we were not expecting one, we
  // restart the request and force a preflight request. This preflight and the
  // following request expect the resource to be in the same IP address space
  // as was originally observed. Spec:
  // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
  if (status.cors_error_status &&
      status.cors_error_status->cors_error ==
          mojom::CorsError::kUnexpectedPrivateNetworkAccess) {
    DCHECK(status.cors_error_status->resource_address_space !=
           mojom::IPAddressSpace::kUnknown);

    // If we only send a preflight because of Private Network Access, and we are
    // configured to ignore errors caused by Private Network Access, then we
    // should ignore any preflight error, as if we had never sent the preflight.
    // Otherwise, if we had sent a preflight before we noticed the private
    // network access, then we rely on `PreflightController` to ignore
    // PNA-specific preflight errors during this second preflight request.
    should_ignore_preflight_errors_ =
        ShouldIgnorePrivateNetworkAccessErrors() &&
        !NeedsPreflight(request_).has_value();

    network_client_receiver_.reset();
    request_.target_ip_address_space =
        status.cors_error_status->resource_address_space;
    StartRequest();
    return;
  }

  net_log_.EndEvent(net::NetLogEventType::CORS_REQUEST);
  forwarding_client_->OnComplete(status);
  std::move(delete_callback_).Run(this);
  // |this| is deleted here.
}

void CorsURLLoader::OnMojoDisconnect() {
  HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
}

// This should be identical to CalculateCorsFlag defined in
// //third_party/blink/renderer/platform/loader/cors/cors.cc.
void CorsURLLoader::SetCorsFlagIfNeeded() {
  if (fetch_cors_flag_) {
    return;
  }

  if (!network::cors::ShouldCheckCors(request_.url, request_.request_initiator,
                                      request_.mode)) {
    return;
  }

  if (HasSpecialAccessToDestination())
    return;

  fetch_cors_flag_ = true;
}

bool CorsURLLoader::HasSpecialAccessToDestination() const {
  // The source origin and destination URL pair may be in the allow list.
  switch (origin_access_list_->CheckAccessState(request_)) {
    case OriginAccessList::AccessState::kAllowed:
      return true;
    case OriginAccessList::AccessState::kBlocked:
    case OriginAccessList::AccessState::kNotListed:
      return false;
  }
}

// static
mojom::FetchResponseType CorsURLLoader::CalculateResponseTaintingForTesting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const absl::optional<url::Origin>& origin,
    const absl::optional<url::Origin>& isolated_world_origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList& origin_access_list) {
  return CalculateResponseTainting(url, request_mode, origin,
                                   isolated_world_origin, cors_flag,
                                   tainted_origin, origin_access_list);
}

// static
absl::optional<CorsErrorStatus> CorsURLLoader::CheckRedirectLocationForTesting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const absl::optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted) {
  return CheckRedirectLocation(url, request_mode, origin, cors_flag, tainted);
}

bool CorsURLLoader::PassesTimingAllowOriginCheck(
    const mojom::URLResponseHead& response) const {
  if (timing_allow_failed_flag_)
    return false;

  if (response_tainting_ == mojom::FetchResponseType::kBasic)
    return true;

  absl::optional<std::string> tao_header_value =
      GetHeaderString(response, kTimingAllowOrigin);
  if (!tao_header_value)
    return false;

  mojom::TimingAllowOriginPtr tao = ParseTimingAllowOrigin(*tao_header_value);

  if (tao->which() == mojom::TimingAllowOrigin::Tag::kAll)
    return true;

  url::Origin origin = tainted_ ? url::Origin() : *request_.request_initiator;
  std::string serialized_origin = origin.Serialize();
  if (base::Contains(tao->get_serialized_origins(), serialized_origin)) {
    return true;
  }

  return false;
}

// Computes the client security state to use, given the factory and
// request-specific values.
//
// WARNING: This should be kept in sync with similar logic in
// `network::URLLoader::GetClientSecurityState()`.
const mojom::ClientSecurityState* CorsURLLoader::GetClientSecurityState()
    const {
  if (factory_client_security_state_) {
    return factory_client_security_state_;
  }

  if (request_.trusted_params) {
    // NOTE: This could return nullptr.
    return request_.trusted_params->client_security_state.get();
  }

  return nullptr;
}

mojom::ClientSecurityStatePtr CorsURLLoader::CloneClientSecurityState() const {
  const mojom::ClientSecurityState* state = GetClientSecurityState();
  if (!state) {
    return nullptr;
  }

  return state->Clone();
}

bool CorsURLLoader::ShouldIgnorePrivateNetworkAccessErrors() const {
  const mojom::ClientSecurityState* state = GetClientSecurityState();
  return state && state->private_network_request_policy ==
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
}

// static
absl::optional<std::string> CorsURLLoader::GetHeaderString(
    const mojom::URLResponseHead& response,
    const std::string& header_name) {
  if (!response.headers)
    return absl::nullopt;
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value))
    return absl::nullopt;
  return header_value;
}

}  // namespace cors

}  // namespace network
