// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORS_h
#define CORS_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/text/WTFString.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace blink {

class HTTPHeaderMap;
class KURL;
class SecurityOrigin;

// CORS related utility functions.
namespace CORS {

// Thin wrapper functions below are for calling ::network::cors functions from
// Blink core. Once Out-of-renderer CORS is enabled, following functions will
// be removed.
PLATFORM_EXPORT WTF::Optional<network::mojom::CORSError> CheckAccess(
    const KURL&,
    const int response_status_code,
    const HTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    const SecurityOrigin&);

PLATFORM_EXPORT WTF::Optional<network::mojom::CORSError> CheckRedirectLocation(
    const KURL&);

PLATFORM_EXPORT WTF::Optional<network::mojom::CORSError> CheckPreflight(
    const int preflight_response_status_code);

PLATFORM_EXPORT WTF::Optional<network::mojom::CORSError> CheckExternalPreflight(
    const HTTPHeaderMap&);

PLATFORM_EXPORT bool IsCORSEnabledRequestMode(network::mojom::FetchRequestMode);

PLATFORM_EXPORT bool EnsurePreflightResultAndCacheOnSuccess(
    const HTTPHeaderMap& response_header_map,
    const String& origin,
    const KURL& request_url,
    const String& request_method,
    const HTTPHeaderMap& request_header_map,
    network::mojom::FetchCredentialsMode request_credentials_mode,
    String* error_description);

PLATFORM_EXPORT bool CheckIfRequestCanSkipPreflight(
    const String& origin,
    const KURL&,
    network::mojom::FetchCredentialsMode,
    const String& method,
    const HTTPHeaderMap& request_header_map);

// Thin wrapper functions that will not be removed even after out-of-renderer
// CORS is enabled.
PLATFORM_EXPORT bool IsCORSSafelistedMethod(const String& method);
PLATFORM_EXPORT bool IsCORSSafelistedContentType(const String&);
PLATFORM_EXPORT bool IsCORSSafelistedHeader(const String& name,
                                            const String& value);

}  // namespace CORS

}  // namespace blink

#endif  // CORS_h
