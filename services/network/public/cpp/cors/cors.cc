// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace {

const char kAsterisk[] = "*";
const char kLowerCaseTrue[] = "true";

}  // namespace

namespace network {

namespace cors {

namespace header_names {

const char kAccessControlAllowCredentials[] =
    "Access-Control-Allow-Credentials";
const char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
const char kAccessControlAllowSuborigin[] = "Access-Control-Allow-Suborigin";

}  // namespace header_names

// See https://fetch.spec.whatwg.org/#cors-check.
base::Optional<mojom::CORSError> CheckAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_suborigin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::FetchCredentialsMode credentials_mode,
    const url::Origin& origin) {
  if (!response_status_code)
    return mojom::CORSError::kInvalidResponse;

  // Check Suborigins, unless the Access-Control-Allow-Origin is '*', which
  // implies that all Suborigins are okay as well.
  bool allow_all_origins = allow_origin_header == kAsterisk;
  if (!origin.suborigin().empty() && !allow_all_origins) {
    if (allow_suborigin_header != kAsterisk &&
        allow_suborigin_header != origin.suborigin()) {
      return mojom::CORSError::kSubOriginMismatch;
    }
  }

  if (allow_all_origins) {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    // See https://fetch.spec.whatwg.org/#cors-protocol-and-credentials.
    if (credentials_mode != mojom::FetchCredentialsMode::kInclude)
      return base::nullopt;
    // Since the credential is a concept for network schemes, we perform the
    // wildcard check only for HTTP and HTTPS. This is a quick hack to allow
    // data URL (see https://crbug.com/315152).
    // TODO(https://crbug.com/736308): Once the callers exist only in the
    // browser process or network service, this check won't be needed any more
    // because it is always for network requests there.
    if (response_url.SchemeIsHTTPOrHTTPS())
      return mojom::CORSError::kWildcardOriginNotAllowed;
  } else if (!allow_origin_header) {
    return mojom::CORSError::kMissingAllowOriginHeader;
  } else if (*allow_origin_header != origin.Serialize()) {
    // We do not use url::Origin::IsSameOriginWith() here for two reasons below.
    //  1. Allow "null" to match here. The latest spec does not have a clear
    //     information about this (https://fetch.spec.whatwg.org/#cors-check),
    //     but the old spec explicitly says that "null" works here
    //     (https://www.w3.org/TR/cors/#resource-sharing-check-0).
    //  2. We do not have a good way to construct url::Origin from the string,
    //     *allow_origin_header, that may be broken. Unfortunately
    //     url::Origin::Create(GURL(*allow_origin_header)) accepts malformed
    //     URL and constructs a valid origin with unexpected fixes, which
    //     results in unexpected behavior.

    // We run some more value checks below to provide better information to
    // developers.

    // Does not allow to have multiple origins in the allow origin header.
    // See https://fetch.spec.whatwg.org/#http-access-control-allow-origin.
    if (allow_origin_header->find_first_of(" ,") != std::string::npos)
      return mojom::CORSError::kMultipleAllowOriginValues;

    // Check valid "null" first since GURL assumes it as invalid.
    if (*allow_origin_header == "null")
      return mojom::CORSError::kAllowOriginMismatch;

    // As commented above, this validation is not strict as an origin
    // validation, but should be ok for providing error details to developers.
    GURL header_origin(*allow_origin_header);
    if (!header_origin.is_valid())
      return mojom::CORSError::kInvalidAllowOriginValue;

    return mojom::CORSError::kAllowOriginMismatch;
  }

  if (credentials_mode == mojom::FetchCredentialsMode::kInclude) {
    // https://fetch.spec.whatwg.org/#http-access-control-allow-credentials.
    // This check should be case sensitive.
    // See also https://fetch.spec.whatwg.org/#http-new-header-syntax.
    if (allow_credentials_header != kLowerCaseTrue)
      return mojom::CORSError::kDisallowCredentialsNotSetToTrue;
  }
  return base::nullopt;
}

base::Optional<mojom::CORSError> CheckRedirectLocation(const GURL& redirect_url,
                                                       bool skip_scheme_check) {
  if (!skip_scheme_check) {
    // Block non HTTP(S) schemes as specified in the step 4 in
    // https://fetch.spec.whatwg.org/#http-redirect-fetch. Chromium also allows
    // the data scheme.
    auto& schemes = url::GetCORSEnabledSchemes();
    if (std::find(std::begin(schemes), std::end(schemes),
                  redirect_url.scheme()) == std::end(schemes)) {
      return mojom::CORSError::kRedirectDisallowedScheme;
    }
  }

  // Block URLs including credentials as specified in the step 9 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch.
  //
  // TODO(tyoshino): This check should be performed also when request's
  // origin is not same origin with the redirect destination's origin.
  if (redirect_url.has_username() || redirect_url.has_password())
    return mojom::CORSError::kRedirectContainsCredentials;

  return base::nullopt;
}

base::Optional<mojom::CORSError> CheckPreflight(const int status_code) {
  // CORS preflight with 3XX is considered network error in
  // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // CORS Spec: http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
  // https://crbug.com/452394
  if (200 <= status_code && status_code < 300)
    return base::nullopt;
  return mojom::CORSError::kPreflightInvalidStatus;
}

// https://wicg.github.io/cors-rfc1918/#http-headerdef-access-control-allow-external
base::Optional<mojom::CORSError> CheckExternalPreflight(
    const base::Optional<std::string>& allow_external) {
  if (!allow_external)
    return mojom::CORSError::kPreflightMissingAllowExternal;
  if (*allow_external == kLowerCaseTrue)
    return base::nullopt;
  return mojom::CORSError::kPreflightInvalidAllowExternal;
}

bool IsCORSEnabledRequestMode(mojom::FetchRequestMode mode) {
  return mode == mojom::FetchRequestMode::kCORS ||
         mode == mojom::FetchRequestMode::kCORSWithForcedPreflight;
}

}  // namespace cors

}  // namespace network
