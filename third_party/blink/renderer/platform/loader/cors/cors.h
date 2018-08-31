// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_H_

#include "base/optional.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class HTTPHeaderMap;
class KURL;
class SecurityOrigin;

enum class CORSFlag : uint8_t {
  Unset,
  Set,
};

// CORS related utility functions.
namespace CORS {

// Thin wrapper functions below are for calling ::network::cors functions from
// Blink core. Once Out-of-renderer CORS is enabled, following functions will
// be removed.
PLATFORM_EXPORT base::Optional<network::CORSErrorStatus> CheckAccess(
    const KURL&,
    const int response_status_code,
    const HTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    const SecurityOrigin&);

PLATFORM_EXPORT base::Optional<network::CORSErrorStatus> CheckPreflightAccess(
    const KURL&,
    const int response_status_code,
    const HTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    const SecurityOrigin&);

PLATFORM_EXPORT base::Optional<network::CORSErrorStatus> CheckRedirectLocation(
    const KURL&,
    network::mojom::FetchRequestMode,
    const SecurityOrigin*,
    CORSFlag);

PLATFORM_EXPORT base::Optional<network::mojom::CORSError> CheckPreflight(
    const int preflight_response_status_code);

PLATFORM_EXPORT base::Optional<network::CORSErrorStatus> CheckExternalPreflight(
    const HTTPHeaderMap&);

PLATFORM_EXPORT bool IsCORSEnabledRequestMode(network::mojom::FetchRequestMode);

PLATFORM_EXPORT base::Optional<network::CORSErrorStatus>
EnsurePreflightResultAndCacheOnSuccess(
    const HTTPHeaderMap& response_header_map,
    const String& origin,
    const KURL& request_url,
    const String& request_method,
    const HTTPHeaderMap& request_header_map,
    network::mojom::FetchCredentialsMode request_credentials_mode);

PLATFORM_EXPORT bool CheckIfRequestCanSkipPreflight(
    const String& origin,
    const KURL&,
    network::mojom::FetchCredentialsMode,
    const String& method,
    const HTTPHeaderMap& request_header_map);

PLATFORM_EXPORT network::mojom::FetchResponseType CalculateResponseTainting(
    const KURL& url,
    network::mojom::FetchRequestMode request_mode,
    const SecurityOrigin* origin,
    CORSFlag cors_flag);

PLATFORM_EXPORT bool CalculateCredentialsFlag(
    network::mojom::FetchCredentialsMode credentials_mode,
    network::mojom::FetchResponseType response_tainting);

// Thin wrapper functions that will not be removed even after out-of-renderer
// CORS is enabled.
PLATFORM_EXPORT bool IsCORSSafelistedMethod(const String& method);
PLATFORM_EXPORT bool IsCORSSafelistedContentType(const String&);
PLATFORM_EXPORT bool IsCORSSafelistedHeader(const String& name,
                                            const String& value);
PLATFORM_EXPORT bool IsForbiddenHeaderName(const String& name);
PLATFORM_EXPORT bool ContainsOnlyCORSSafelistedHeaders(const HTTPHeaderMap&);
PLATFORM_EXPORT bool ContainsOnlyCORSSafelistedOrForbiddenHeaders(
    const HTTPHeaderMap&);

PLATFORM_EXPORT bool IsOkStatus(int status);

// Calculates and returns the CORS flag used in several "fetch" algorithms in
// https://fetch.spec.whatwg.org/. This function is corresponding to the CORS
// flag setting logic in https://fetch.spec.whatwg.org/#main-fetch.
// This function can return true even when |request_mode| is |kSameOrigin|.
// |origin| must not be nullptr when |request_mode| is neither |kNoCORS| nor
// |kNavigate|.
// This should be identical to CalculateCORSFlag defined in
// //services/network/cors/cors_url_loader.cc.
// This function will be removed when out-of-renderer CORS is enabled.
PLATFORM_EXPORT bool CalculateCORSFlag(
    const KURL& url,
    const SecurityOrigin* origin,
    network::mojom::FetchRequestMode request_mode);

}  // namespace CORS

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_H_
