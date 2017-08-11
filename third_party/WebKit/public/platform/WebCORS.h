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

#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/StringHash.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class WebURLResponse;
class WebSecurityOrigin;

namespace WebCORS {

typedef HashSet<String, CaseFoldingHash> HTTPHeaderSet;

// Enumerating the error conditions that the CORS
// access control check can report, including success.
//
// See |CheckAccess()| and |AccessControlErrorString()| which respectively
// produce and consume these error values, for precise meaning.
enum AccessStatus {
  kAccessAllowed,
  kInvalidResponse,
  kAllowOriginMismatch,
  kSubOriginMismatch,
  kWildcardOriginNotAllowed,
  kMissingAllowOriginHeader,
  kMultipleAllowOriginValues,
  kInvalidAllowOriginValue,
  kDisallowCredentialsNotSetToTrue,
};

// Enumerating the error conditions that CORS preflight
// can report, including success.
//
// See |CheckPreflight()| methods and |PreflightErrorString()| which
// respectively produce and consume these error values, for precise meaning.
enum PreflightStatus {
  kPreflightSuccess,
  kPreflightInvalidStatus,
  // "Access-Control-Allow-External:"
  // ( https://wicg.github.io/cors-rfc1918/#headers ) specific error
  // conditions:
  kPreflightMissingAllowExternal,
  kPreflightInvalidAllowExternal,
};

// Enumerating the error conditions that CORS redirect target URL
// checks can report, including success.
//
// See |CheckRedirectLocation()| methods and |RedirectErrorString()| which
// respectively produce and consume these error values, for precise meaning.
enum RedirectStatus {
  kRedirectSuccess,
  kRedirectDisallowedScheme,
  kRedirectContainsCredentials,
};

// Perform a CORS access check on the response parameters. Returns
// |kAccessAllowed| if access is allowed. Use |AccessControlErrorString()| to
// construct a user-friendly error message for any of the other (error)
// conditions.
BLINK_PLATFORM_EXPORT AccessStatus
CheckAccess(const WebURL,
            const int response_status_code,
            const HTTPHeaderMap&,
            WebURLRequest::FetchCredentialsMode,
            const WebSecurityOrigin&);

// Given a redirected-to URL, check if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
//
// Returns |kRedirectSuccess| in all other cases. Use
// |RedirectErrorString()| to construct a user-friendly error
// message for any of the error conditions.
BLINK_PLATFORM_EXPORT RedirectStatus CheckRedirectLocation(const WebURL&);

// Perform the required CORS checks on the response to a preflight request.
// Returns |kPreflightSuccess| if preflight response was successful.
// Use |PreflightErrorString()| to construct a user-friendly error message
// for any of the other (error) conditions.
BLINK_PLATFORM_EXPORT PreflightStatus
CheckPreflight(const int preflight_response_status_code);

// Error checking for the currently experimental
// "Access-Control-Allow-External:" header. Shares error conditions with
// standard preflight checking.
BLINK_PLATFORM_EXPORT PreflightStatus
CheckExternalPreflight(const HTTPHeaderMap&);

BLINK_PLATFORM_EXPORT WebURLRequest
CreateAccessControlPreflightRequest(const WebURLRequest&);

// TODO(tyoshino): Using platform/loader/fetch/ResourceLoaderOptions violates
// the DEPS rule. This will be fixed soon by making HandleRedirect() not
// depending on ResourceLoaderOptions.
BLINK_PLATFORM_EXPORT bool HandleRedirect(
    WebSecurityOrigin&,
    WebURLRequest&,
    const WebURL,
    const int redirect_response_status_code,
    const HTTPHeaderMap&,
    WebURLRequest::FetchCredentialsMode,
    ResourceLoaderOptions&,
    WebString&);

// Stringify errors from CORS access checks, preflight or redirect checks.
BLINK_PLATFORM_EXPORT WebString
AccessControlErrorString(const AccessStatus,
                         const int response_status_code,
                         const HTTPHeaderMap&,
                         const WebSecurityOrigin&,
                         const WebURLRequest::RequestContext);

BLINK_PLATFORM_EXPORT WebString
PreflightErrorString(const PreflightStatus,
                     const HTTPHeaderMap&,
                     const int preflight_response_status_code);

BLINK_PLATFORM_EXPORT WebString RedirectErrorString(const RedirectStatus,
                                                    const WebURL&);

BLINK_PLATFORM_EXPORT void ParseAccessControlExposeHeadersAllowList(
    const WebString&,
    HTTPHeaderSet&);

BLINK_PLATFORM_EXPORT void ExtractCorsExposedHeaderNamesList(
    const WebURLResponse&,
    HTTPHeaderSet&);

BLINK_PLATFORM_EXPORT bool IsOnAccessControlResponseHeaderWhitelist(
    const WebString&);

}  // namespace WebCORS

}  // namespace blink

#endif  // WebCORS_h
