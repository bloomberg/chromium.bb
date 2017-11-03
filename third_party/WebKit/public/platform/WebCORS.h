/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WebCORS_h
#define WebCORS_h

#include "base/optional.h"
#include "public/platform/WebHTTPHeaderMap.h"
#include "public/platform/WebHTTPHeaderSet.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/interfaces/cors.mojom-shared.h"

namespace blink {

class WebURLResponse;
class WebSecurityOrigin;
struct ResourceLoaderOptions;

namespace WebCORS {

// Perform a CORS access check on the response parameters.
//
// Use |GetErrorString()| to construct a user-friendly error message.
BLINK_PLATFORM_EXPORT base::Optional<network::mojom::CORSError> CheckAccess(
    const WebURL,
    const int response_status_code,
    const WebHTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    const WebSecurityOrigin&);

// Given a redirected-to URL, check if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
//
// Use |GetErrorString()| to construct a user-friendly error message.
BLINK_PLATFORM_EXPORT base::Optional<network::mojom::CORSError>
CheckRedirectLocation(const WebURL&);

// Perform the required CORS checks on the response to a preflight request.
// Returns |kPreflightSuccess| if preflight response was successful.
//
// Use |GetErrorString()| to construct a user-friendly error message.
BLINK_PLATFORM_EXPORT base::Optional<network::mojom::CORSError> CheckPreflight(
    const int preflight_response_status_code);

// Error checking for the currently experimental
// "Access-Control-Allow-External:" header. Shares error conditions with
// standard preflight checking.
//
// Use |GetErrorString()| to construct a user-friendly error message.
BLINK_PLATFORM_EXPORT base::Optional<network::mojom::CORSError>
CheckExternalPreflight(const WebHTTPHeaderMap&);

BLINK_PLATFORM_EXPORT WebURLRequest
CreateAccessControlPreflightRequest(const WebURLRequest&);

// TODO(tyoshino): Using platform/loader/fetch/ResourceLoaderOptions violates
// the DEPS rule. This will be fixed soon by making HandleRedirect() not
// depending on ResourceLoaderOptions.
BLINK_PLATFORM_EXPORT base::Optional<network::mojom::CORSError> HandleRedirect(
    WebSecurityOrigin&,
    WebURLRequest&,
    const WebURL,
    const int redirect_response_status_code,
    const WebHTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    ResourceLoaderOptions&);

// Stringify CORSError mainly for inspector messages. Generated string should
// not be exposed to JavaScript for security reasons.
// For errors during the redirect check, valid WebURL should be set to
// |redirect_url|. Otherwise, it should be WebURL(), the invalid instance.
BLINK_PLATFORM_EXPORT WebString
GetErrorString(const network::mojom::CORSError,
               const WebURL& request_url,
               const WebURL& redirect_url,
               const int response_status_code,
               const WebHTTPHeaderMap&,
               const WebSecurityOrigin&,
               const WebURLRequest::RequestContext);

BLINK_PLATFORM_EXPORT void ParseAccessControlExposeHeadersAllowList(
    const WebString&,
    WebHTTPHeaderSet&);

BLINK_PLATFORM_EXPORT void ExtractCorsExposedHeaderNamesList(
    const WebURLResponse&,
    WebHTTPHeaderSet&);

BLINK_PLATFORM_EXPORT bool IsOnAccessControlResponseHeaderWhitelist(
    const WebString&);

BLINK_PLATFORM_EXPORT bool IsCORSEnabledRequestMode(
    network::mojom::FetchRequestMode);

// Checks whether request mode 'no-cors' is allowed for a certain context and
// service-worker mode.
BLINK_PLATFORM_EXPORT bool IsNoCORSAllowedContext(
    WebURLRequest::RequestContext,
    WebURLRequest::ServiceWorkerMode);

// TODO(hintzed): The following three methods delegate to SchemeRegistry and
// FetchUtils respectively to expose them for outofblink-CORS in CORSURLLoader.
// This is a temporary solution with the mid-term goal being to move e.g.
// FetchUtils somewhere where it can be used from /content. The long term goal
// is that CORS will be handled ouf of blink (https://crbug/736308).

BLINK_PLATFORM_EXPORT WebString ListOfCORSEnabledURLSchemes();

BLINK_PLATFORM_EXPORT bool IsCORSSafelistedMethod(const WebString&);

BLINK_PLATFORM_EXPORT bool ContainsOnlyCORSSafelistedOrForbiddenHeaders(
    const WebHTTPHeaderMap&);

}  // namespace WebCORS

}  // namespace blink

#endif  // WebCORS_h
