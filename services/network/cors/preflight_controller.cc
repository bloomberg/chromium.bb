// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

// Algorithm step 3 of the CORS-preflight fetch,
// https://fetch.spec.whatwg.org/#cors-preflight-fetch-0, that requires
//  - CORS-safelisted request-headers excluded
//  - duplicates excluded
//  - sorted lexicographically
//  - byte-lowercased
std::string CreateAccessControlRequestHeadersHeader(
    const net::HttpRequestHeaders& headers) {
  std::vector<std::string> filtered_headers;
  for (const auto& header : headers.GetHeaderVector()) {
    // Exclude CORS-safelisted headers.
    if (cors::IsCORSSafelistedHeader(header.key, header.value))
      continue;
    // Exclude the forbidden headers because they may be added by the user
    // agent. They must be checked separately and rejected for
    // JavaScript-initiated requests.
    if (cors::IsForbiddenHeader(header.key))
      continue;
    filtered_headers.push_back(base::ToLowerASCII(header.key));
  }
  if (filtered_headers.empty())
    return std::string();

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end());

  return base::JoinString(filtered_headers, ",");
}

}  // namespace

// static
std::unique_ptr<ResourceRequest> PreflightController::CreatePreflightRequest(
    const ResourceRequest& request) {
  DCHECK(!request.url.has_username());
  DCHECK(!request.url.has_password());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>();

  // Algorithm step 1 through 4 of the CORS-preflight fetch,
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0.
  preflight_request->url = request.url;
  preflight_request->method = "OPTIONS";
  preflight_request->priority = request.priority;
  preflight_request->fetch_request_context_type =
      request.fetch_request_context_type;
  preflight_request->referrer = request.referrer;
  preflight_request->referrer_policy = request.referrer_policy;

  preflight_request->fetch_credentials_mode =
      mojom::FetchCredentialsMode::kOmit;

  preflight_request->headers.SetHeader(
      cors::header_names::kAccessControlRequestMethod, request.method);

  std::string request_headers =
      CreateAccessControlRequestHeadersHeader(request.headers);
  if (!request_headers.empty()) {
    preflight_request->headers.SetHeader(
        cors::header_names::kAccessControlRequestHeaders, request_headers);
  }

  if (request.is_external_request) {
    preflight_request->headers.SetHeader(
        cors::header_names::kAccessControlRequestExternal, "true");
  }

  DCHECK(request.request_initiator);
  preflight_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                       request.request_initiator->Serialize());

  // TODO(toyoshim): Remove the following line once the network service is
  // enabled by default.
  preflight_request->skip_service_worker = true;

  return preflight_request;
}

}  // namespace cors

}  // namespace network
