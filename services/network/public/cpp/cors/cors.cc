// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace header_names {

const char kAccessControlAllowCredentials[] =
    "Access-Control-Allow-Credentials";
const char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
const char kAccessControlAllowSuborigin[] = "Access-Control-Allow-Suborigin";

}  // namespace header_names

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
  bool allow_all_origins = allow_origin_header == "*";
  if (!origin.suborigin().empty() && !allow_all_origins) {
    if (allow_suborigin_header != "*" &&
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
  } else if (!origin.IsSameOriginWith(
                 url::Origin::Create(GURL(*allow_origin_header)))) {
    // Does not allow to have multiple origins in the allow origin header.
    // See https://fetch.spec.whatwg.org/#http-access-control-allow-origin.
    if (allow_origin_header->find_first_of(" ,") != std::string::npos)
      return mojom::CORSError::kMultipleAllowOriginValues;
    GURL header_origin(*allow_origin_header);
    if (!header_origin.is_valid())
      return mojom::CORSError::kInvalidAllowOriginValue;
    return mojom::CORSError::kAllowOriginMismatch;
  }

  if (credentials_mode == mojom::FetchCredentialsMode::kInclude) {
    // https://fetch.spec.whatwg.org/#http-access-control-allow-credentials.
    // This check should be case sensitive.
    // See also https://fetch.spec.whatwg.org/#http-new-header-syntax.
    if (allow_credentials_header != "true")
      return mojom::CORSError::kDisallowCredentialsNotSetToTrue;
  }
  return base::nullopt;
}

bool IsCORSEnabledRequestMode(mojom::FetchRequestMode mode) {
  return mode == mojom::FetchRequestMode::kCORS ||
         mode == mojom::FetchRequestMode::kCORSWithForcedPreflight;
}

}  // namespace cors

}  // namespace network
