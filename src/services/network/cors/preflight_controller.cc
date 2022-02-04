// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "services/network/cors/cors_util.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

const char kLowerCaseTrue[] = "true";

int RetrieveCacheFlags(int load_flags) {
  return load_flags & (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                       net::LOAD_DISABLE_CACHE);
}

absl::optional<std::string> GetHeaderString(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(header_name, &header_value))
    return absl::nullopt;
  return header_value;
}

// Algorithm step 3 of the CORS-preflight fetch,
// https://fetch.spec.whatwg.org/#cors-preflight-fetch-0, that requires
//  - CORS-safelisted request-headers excluded
//  - duplicates excluded
//  - sorted lexicographically
//  - byte-lowercased
std::string CreateAccessControlRequestHeadersHeader(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating) {
  // Exclude the forbidden headers because they may be added by the user
  // agent. They must be checked separately and rejected for
  // JavaScript-initiated requests.
  std::vector<std::string> filtered_headers =
      CorsUnsafeNotForbiddenRequestHeaderNames(headers.GetHeaderVector(),
                                               is_revalidating);
  if (filtered_headers.empty())
    return std::string();

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end());

  return base::JoinString(filtered_headers, ",");
}

std::unique_ptr<ResourceRequest> CreatePreflightRequest(
    const ResourceRequest& request,
    bool tainted,
    const net::NetLogWithSource& net_log_for_actual_request,
    const absl::optional<base::UnguessableToken>& devtools_request_id) {
  DCHECK(!request.url.has_username());
  DCHECK(!request.url.has_password());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>();

  // Algorithm step 1 through 5 of the CORS-preflight fetch,
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch.
  preflight_request->url = request.url;
  preflight_request->method = net::HttpRequestHeaders::kOptionsMethod;
  preflight_request->priority = request.priority;
  preflight_request->destination = request.destination;
  preflight_request->referrer = request.referrer;
  preflight_request->referrer_policy = request.referrer_policy;
  preflight_request->mode = mojom::RequestMode::kCors;

  preflight_request->credentials_mode = mojom::CredentialsMode::kOmit;
  preflight_request->load_flags = RetrieveCacheFlags(request.load_flags);
  preflight_request->resource_type = request.resource_type;
  preflight_request->fetch_window_id = request.fetch_window_id;

  preflight_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                       kDefaultAcceptHeaderValue);

  preflight_request->headers.SetHeader(
      header_names::kAccessControlRequestMethod, request.method);

  std::string request_headers = CreateAccessControlRequestHeadersHeader(
      request.headers, request.is_revalidating);
  if (!request_headers.empty()) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestHeaders, request_headers);
  }

  preflight_request->target_ip_address_space = request.target_ip_address_space;
  if (preflight_request->target_ip_address_space !=
      mojom::IPAddressSpace::kUnknown) {
    // See the CORS-preflight fetch algorithm modifications laid out in the
    // Private Network Access spec, in step 4 of the CORS preflight section as
    // of writing: https://wicg.github.io/private-network-access/#cors-preflight
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestPrivateNetwork, "true");
  }

  DCHECK(request.request_initiator);
  preflight_request->request_initiator = request.request_initiator;
  preflight_request->headers.SetHeader(
      net::HttpRequestHeaders::kOrigin,
      (tainted ? url::Origin() : *request.request_initiator).Serialize());

  // We normally set User-Agent down in the network stack, but the DevTools
  // emulation override is applied on a higher level (renderer or browser),
  // so copy User-Agent from the original request, if present.
  // TODO(caseq, morlovich): do the same for client hints.
  std::string user_agent;
  if (request.headers.GetHeader(net::HttpRequestHeaders::kUserAgent,
                                &user_agent)) {
    preflight_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                         user_agent);
  }

  // Additional headers that the algorithm in the spec does not require, but
  // it's better that CORS preflight requests have them.
  preflight_request->headers.SetHeader("Sec-Fetch-Mode", "cors");

  if (devtools_request_id) {
    // Set `enable_load_timing` flag to make URLLoader fill the LoadTimingInfo
    // in URLResponseHead, which will be sent to DevTools.
    preflight_request->enable_load_timing = true;
    // Set `devtools_request_id` to make URLLoader send the raw request and the
    // raw response to DevTools.
    preflight_request->devtools_request_id = devtools_request_id->ToString();
  }
  preflight_request->is_fetch_like_api = request.is_fetch_like_api;
  preflight_request->is_favicon = request.is_favicon;

  // Set `net_log_reference_info` to reference actual request from preflight
  // request in NetLog.
  preflight_request->net_log_reference_info =
      net_log_for_actual_request.source();

  net::NetLogSource net_log_source_for_preflight = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  net_log_for_actual_request.AddEventReferencingSource(
      net::NetLogEventType::CORS_PREFLIGHT_URL_REQUEST,
      net_log_source_for_preflight);
  // Set `net_log_create_info` to specify NetLog source used in preflight
  // URL Request.
  preflight_request->net_log_create_info = net_log_source_for_preflight;

  return preflight_request;
}

// Performs a CORS access check on the CORS-preflight response parameters.
// According to the note at https://fetch.spec.whatwg.org/#cors-preflight-fetch
// step 6, even for a preflight check, `credentials_mode` should be checked on
// the actual request rather than preflight one.
absl::optional<CorsErrorStatus> CheckPreflightAccess(
    const GURL& response_url,
    const int response_status_code,
    const absl::optional<std::string>& allow_origin_header,
    const absl::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  // Step 7 of https://fetch.spec.whatwg.org/#cors-preflight-fetch
  auto error_status =
      CheckAccess(response_url, allow_origin_header, allow_credentials_header,
                  actual_credentials_mode, origin);
  const bool has_ok_status = IsOkStatus(response_status_code);

  AccessCheckResult result = (error_status || !has_ok_status)
                                 ? AccessCheckResult::kNotPermittedInPreflight
                                 : AccessCheckResult::kPermittedInPreflight;
  UMA_HISTOGRAM_ENUMERATION("Net.Cors.AccessCheckResult", result);
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    UMA_HISTOGRAM_ENUMERATION("Net.Cors.AccessCheckResult.NotSecureRequestor",
                              result);
  }

  // Prefer using a preflight specific error code.
  if (error_status) {
    switch (error_status->cors_error) {
      case mojom::CorsError::kWildcardOriginNotAllowed:
        error_status->cors_error =
            mojom::CorsError::kPreflightWildcardOriginNotAllowed;
        break;
      case mojom::CorsError::kMissingAllowOriginHeader:
        error_status->cors_error =
            mojom::CorsError::kPreflightMissingAllowOriginHeader;
        break;
      case mojom::CorsError::kMultipleAllowOriginValues:
        error_status->cors_error =
            mojom::CorsError::kPreflightMultipleAllowOriginValues;
        break;
      case mojom::CorsError::kInvalidAllowOriginValue:
        error_status->cors_error =
            mojom::CorsError::kPreflightInvalidAllowOriginValue;
        break;
      case mojom::CorsError::kAllowOriginMismatch:
        error_status->cors_error =
            mojom::CorsError::kPreflightAllowOriginMismatch;
        break;
      case mojom::CorsError::kInvalidAllowCredentials:
        error_status->cors_error =
            mojom::CorsError::kPreflightInvalidAllowCredentials;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (!has_ok_status) {
    error_status = absl::make_optional<CorsErrorStatus>(
        mojom::CorsError::kPreflightInvalidStatus);
  } else {
    return absl::nullopt;
  }

  return error_status;
}

// Checks errors for the "Access-Control-Allow-Private-Network" header.
//
// See the CORS-preflight fetch algorithm modifications laid out in the Private
// Network Access spec, in step 4 of the CORS preflight section as of writing:
// https://wicg.github.io/private-network-access/#cors-preflight
absl::optional<CorsErrorStatus> CheckAllowPrivateNetworkHeader(
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request) {
  if (original_request.target_ip_address_space ==
      mojom::IPAddressSpace::kUnknown) {
    // Not a Private Network Access preflight.
    return absl::nullopt;
  }

  absl::optional<std::string> header = GetHeaderString(
      head.headers, header_names::kAccessControlAllowPrivateNetwork);
  if (!header) {
    CorsErrorStatus status(
        mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
    status.target_address_space = original_request.target_ip_address_space;
    return status;
  }

  if (*header != kLowerCaseTrue) {
    CorsErrorStatus status(
        mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, *header);
    status.target_address_space = original_request.target_ip_address_space;
    return status;
  }

  return absl::nullopt;
}

std::unique_ptr<PreflightResult> CreatePreflightResult(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    PreflightController::EnforcePrivateNetworkAccessHeader
        enforce_private_network_access_header,
    const mojom::ClientSecurityStatePtr& client_security_state,
    mojom::DevToolsObserver* devtools_observer,
    absl::optional<CorsErrorStatus>* detected_error_status) {
  DCHECK(detected_error_status);

  *detected_error_status = CheckPreflightAccess(
      final_url, head.headers ? head.headers->response_code() : 0,
      GetHeaderString(head.headers, header_names::kAccessControlAllowOrigin),
      GetHeaderString(head.headers,
                      header_names::kAccessControlAllowCredentials),
      original_request.credentials_mode,
      tainted ? url::Origin() : *original_request.request_initiator);
  if (*detected_error_status)
    return nullptr;

  absl::optional<CorsErrorStatus> status =
      CheckAllowPrivateNetworkHeader(head, original_request);
  if (status) {
    if (enforce_private_network_access_header) {
      *detected_error_status = std::move(status);
      return nullptr;
    }

    // We only report these errors as warnings when they are suppressed, since
    // `CorsURLLoader` already reports them otherwise.
    if (devtools_observer) {
      devtools_observer->OnCorsError(
          original_request.devtools_request_id,
          original_request.request_initiator, client_security_state.Clone(),
          original_request.url, *status, /*is_warning=*/true);
    }

    base::UmaHistogramEnumeration(kPreflightWarningHistogramName,
                                  status->cors_error);
  }

  absl::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(
      original_request.credentials_mode,
      GetHeaderString(head.headers, header_names::kAccessControlAllowMethods),
      GetHeaderString(head.headers, header_names::kAccessControlAllowHeaders),
      GetHeaderString(head.headers, header_names::kAccessControlMaxAge),
      &error);

  if (error)
    *detected_error_status = CorsErrorStatus(*error);

  return result;
}

absl::optional<CorsErrorStatus> CheckPreflightResult(
    PreflightResult* result,
    const ResourceRequest& original_request,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support) {
  absl::optional<CorsErrorStatus> status =
      result->EnsureAllowedCrossOriginMethod(original_request.method);
  if (status)
    return status;

  return result->EnsureAllowedCrossOriginHeaders(
      original_request.headers, original_request.is_revalidating,
      non_wildcard_request_headers_support);
}

}  // namespace

const char kPreflightErrorHistogramName[] = "Net.Cors.PreflightCheckError2";
const char kPreflightWarningHistogramName[] = "Net.Cors.PreflightCheckWarning";

class PreflightController::PreflightLoader final {
 public:
  PreflightLoader(
      PreflightController* controller,
      CompletionCallback completion_callback,
      const ResourceRequest& request,
      WithTrustedHeaderClient with_trusted_header_client,
      NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
      EnforcePrivateNetworkAccessHeader enforce_private_network_access_header,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      const net::NetLogWithSource net_log)
      : controller_(controller),
        completion_callback_(std::move(completion_callback)),
        original_request_(request),
        non_wildcard_request_headers_support_(
            non_wildcard_request_headers_support),
        enforce_private_network_access_header_(
            enforce_private_network_access_header),
        tainted_(tainted),
        network_isolation_key_(network_isolation_key),
        client_security_state_(std::move(client_security_state)),
        devtools_observer_(std::move(devtools_observer)),
        net_log_(net_log) {
    if (devtools_observer_)
      devtools_request_id_ = base::UnguessableToken::Create();
    auto preflight_request =
        CreatePreflightRequest(request, tainted, net_log, devtools_request_id_);

    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      network::mojom::URLRequestDevToolsInfoPtr request_info =
          network::ExtractDevToolsInfo(*preflight_request);
      devtools_observer_->OnCorsPreflightRequest(
          *devtools_request_id_, preflight_request->headers,
          std::move(request_info), original_request_.url,
          original_request_.devtools_request_id.value_or(""));
    }
    loader_ =
        SimpleURLLoader::Create(std::move(preflight_request), annotation_tag);
    uint32_t options = mojom::kURLLoadOptionAsCorsPreflight;
    if (with_trusted_header_client) {
      options |= mojom::kURLLoadOptionUseHeaderClient;
    }
    loader_->SetURLLoaderFactoryOptions(options);
  }

  PreflightLoader(const PreflightLoader&) = delete;
  PreflightLoader& operator=(const PreflightLoader&) = delete;

  void Request(mojom::URLLoaderFactory* loader_factory) {
    DCHECK(loader_);

    loader_->SetOnRedirectCallback(base::BindRepeating(
        &PreflightLoader::HandleRedirect, base::Unretained(this)));
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &PreflightLoader::HandleResponseHeader, base::Unretained(this)));

    loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&PreflightLoader::HandleResponseBody,
                       base::Unretained(this)),
        0);
  }

 private:
  void HandleRedirect(const net::RedirectInfo& redirect_info,
                      const network::mojom::URLResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers) {
    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      devtools_observer_->OnCorsPreflightRequestCompleted(
          *devtools_request_id_,
          network::URLLoaderCompletionStatus(net::ERR_INVALID_REDIRECT));
    }

    std::move(completion_callback_)
        .Run(net::ERR_FAILED,
             CorsErrorStatus(mojom::CorsError::kPreflightDisallowedRedirect),
             false);

    RemoveFromController();
    // `this` is deleted here.
  }

  void HandleResponseHeader(const GURL& final_url,
                            const mojom::URLResponseHead& head) {
    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      mojom::URLResponseHeadDevToolsInfoPtr head_info =
          ExtractDevToolsInfo(head);
      devtools_observer_->OnCorsPreflightResponse(
          *devtools_request_id_, original_request_.url, std::move(head_info));
      devtools_observer_->OnCorsPreflightRequestCompleted(
          *devtools_request_id_, network::URLLoaderCompletionStatus(net::OK));
    }

    absl::optional<CorsErrorStatus> detected_error_status;
    bool has_authorization_covered_by_wildcard = false;
    std::unique_ptr<PreflightResult> result = CreatePreflightResult(
        final_url, head, original_request_, tainted_,
        enforce_private_network_access_header_, client_security_state_,
        devtools_observer_ ? devtools_observer_.get() : nullptr,
        &detected_error_status);

    if (result) {
      // Only log if there is a result to log.
      net_log_.AddEvent(net::NetLogEventType::CORS_PREFLIGHT_RESULT,
                        [&result] { return result->NetLogParams(); });

      // Preflight succeeded. Check `original_request_` with `result`.
      DCHECK(!detected_error_status);
      detected_error_status =
          CheckPreflightResult(result.get(), original_request_,
                               non_wildcard_request_headers_support_);
      has_authorization_covered_by_wildcard =
          result->HasAuthorizationCoveredByWildcard(original_request_.headers);
    }

    if (!(original_request_.load_flags & net::LOAD_DISABLE_CACHE) &&
        // TODO(https://crbug.com/1268312): Key the cache by target address
        // space and remove this guard.
        original_request_.target_ip_address_space ==
            mojom::IPAddressSpace::kUnknown &&
        !detected_error_status) {
      controller_->AppendToCache(*original_request_.request_initiator,
                                 original_request_.url, network_isolation_key_,
                                 std::move(result));
    }

    std::move(completion_callback_)
        .Run(detected_error_status ? net::ERR_FAILED : net::OK,
             detected_error_status, has_authorization_covered_by_wildcard);
  }

  void HandleResponseBody(std::unique_ptr<std::string> response_body) {
    const int error = loader_->NetError();
    const absl::optional<URLLoaderCompletionStatus>& status =
        loader_->CompletionStatus();

    if (!completion_callback_.is_null()) {
      // As HandleResponseHeader() isn't called due to a request failure, such
      // as unknown hosts. unreachable remote, reset by peer, and so on, we
      // still hold `completion_callback_` to invoke.
      if (devtools_observer_) {
        DCHECK(devtools_request_id_);
        devtools_observer_->OnCorsPreflightRequestCompleted(
            *devtools_request_id_, network::URLLoaderCompletionStatus(error));
      }
      std::move(completion_callback_)
          .Run(error,
               status.has_value() ? status->cors_error_status : absl::nullopt,
               false);
    }

    RemoveFromController();
    // `this` is deleted here.
  }

  // Removes `this` instance from `controller_`. Once the method returns, `this`
  // is already removed.
  void RemoveFromController() { controller_->RemoveLoader(this); }

  // PreflightController owns all PreflightLoader instances, and should outlive.
  const raw_ptr<PreflightController> controller_;

  // Holds SimpleURLLoader instance for the CORS-preflight request.
  std::unique_ptr<SimpleURLLoader> loader_;

  // Holds caller's information.
  PreflightController::CompletionCallback completion_callback_;
  const ResourceRequest original_request_;

  const NonWildcardRequestHeadersSupport non_wildcard_request_headers_support_;
  const EnforcePrivateNetworkAccessHeader
      enforce_private_network_access_header_;
  const bool tainted_;
  absl::optional<base::UnguessableToken> devtools_request_id_;
  const net::NetworkIsolationKey network_isolation_key_;
  const mojom::ClientSecurityStatePtr client_security_state_;
  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;
  const net::NetLogWithSource net_log_;
};

// static
std::unique_ptr<ResourceRequest>
PreflightController::CreatePreflightRequestForTesting(
    const ResourceRequest& request,
    bool tainted) {
  return CreatePreflightRequest(
      request, tainted,
      net::NetLogWithSource::Make(net::NetLog::Get(),
                                  net::NetLogSourceType::URL_REQUEST),
      /*devtools_request_id=*/absl::nullopt);
}

// static
std::unique_ptr<PreflightResult>
PreflightController::CreatePreflightResultForTesting(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    EnforcePrivateNetworkAccessHeader enforce_private_network_access_header,
    absl::optional<CorsErrorStatus>* detected_error_status) {
  return CreatePreflightResult(final_url, head, original_request, tainted,
                               enforce_private_network_access_header,
                               /*client_security_state=*/nullptr,
                               /*devtools_observer=*/nullptr,
                               detected_error_status);
}

// static
absl::optional<CorsErrorStatus>
PreflightController::CheckPreflightAccessForTesting(
    const GURL& response_url,
    const int response_status_code,
    const absl::optional<std::string>& allow_origin_header,
    const absl::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  return CheckPreflightAccess(response_url, response_status_code,
                              allow_origin_header, allow_credentials_header,
                              actual_credentials_mode, origin);
}

PreflightController::PreflightController(NetworkService* network_service)
    : network_service_(network_service) {}

PreflightController::~PreflightController() = default;

void PreflightController::PerformPreflightCheck(
    CompletionCallback callback,
    const ResourceRequest& request,
    WithTrustedHeaderClient with_trusted_header_client,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
    EnforcePrivateNetworkAccessHeader enforce_private_network_access_header,
    bool tainted,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    mojom::URLLoaderFactory* loader_factory,
    const net::IsolationInfo& isolation_info,
    mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    const net::NetLogWithSource& net_log) {
  DCHECK(request.request_initiator);

  const net::NetworkIsolationKey& network_isolation_key =
      !isolation_info.IsEmpty()
          ? isolation_info.network_isolation_key()
          : request.trusted_params.has_value()
                ? request.trusted_params->isolation_info.network_isolation_key()
                : net::NetworkIsolationKey();
  if (!RetrieveCacheFlags(request.load_flags) &&
      // TODO(https://crbug.com/1263483): Remove this.
      !request.is_external_request &&
      // TODO(https://crbug.com/1268312): Key the cache by target address space
      // and remove this guard.
      request.target_ip_address_space == mojom::IPAddressSpace::kUnknown &&
      cache_.CheckIfRequestCanSkipPreflight(
          request.request_initiator.value(), request.url, network_isolation_key,
          request.credentials_mode, request.method, request.headers,
          request.is_revalidating, net_log)) {
    std::move(callback).Run(net::OK, absl::nullopt, false);
    return;
  }

  auto emplaced_pair = loaders_.emplace(std::make_unique<PreflightLoader>(
      this, std::move(callback), request, with_trusted_header_client,
      non_wildcard_request_headers_support,
      enforce_private_network_access_header, tainted, annotation_tag,
      network_isolation_key, std::move(client_security_state),
      std::move(devtools_observer), net_log));
  (*emplaced_pair.first)->Request(loader_factory);
}

void PreflightController::RemoveLoader(PreflightLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);
}

void PreflightController::AppendToCache(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    std::unique_ptr<PreflightResult> result) {
  cache_.AppendEntry(origin, url, network_isolation_key, std::move(result));
}

}  // namespace cors

}  // namespace network
