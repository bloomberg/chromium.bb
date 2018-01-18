// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORS_h
#define CORS_h

#include <string>

#include "platform/PlatformExport.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/interfaces/cors.mojom-shared.h"
#include "services/network/public/interfaces/fetch_api.mojom-shared.h"

namespace blink {

class HTTPHeaderMap;
class KURL;
class SecurityOrigin;

// CORS related utility functions.
namespace CORS {

// Stringify CORSError mainly for inspector messages. Generated string should
// not be exposed to JavaScript for security reasons.
// For errors during the redirect check, valid KURL should be set to
// |redirect_url|. Otherwise, it should be KURL(), the invalid instance.
PLATFORM_EXPORT String GetErrorString(const network::mojom::CORSError,
                                      const KURL& request_url,
                                      const KURL& redirect_url,
                                      const int response_status_code,
                                      const HTTPHeaderMap&,
                                      const SecurityOrigin&,
                                      const WebURLRequest::RequestContext);

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

}  // namespace CORS

}  // namespace blink

#endif  // CORS_h
