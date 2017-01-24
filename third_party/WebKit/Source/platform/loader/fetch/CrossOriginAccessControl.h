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

#ifndef CrossOriginAccessControl_h
#define CrossOriginAccessControl_h

#include "platform/PlatformExport.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"

namespace blink {

using HTTPHeaderSet = HashSet<String, CaseFoldingHash>;

struct ResourceLoaderOptions;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;

class PLATFORM_EXPORT CrossOriginAccessControl {
  STATIC_ONLY(CrossOriginAccessControl);

 public:
  // Enumerating the error conditions that the CORS
  // access control check can report, including success.
  //
  // See |checkAccess()| and |accessControlErrorString()| which respectively
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
  // See |checkPreflight()| methods and |preflightErrorString()| which
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
  // See |checkRedirectLocation()| methods and |redirectErrorString()| which
  // respectively produce and consume these error values, for precise meaning.
  enum RedirectStatus {
    kRedirectSuccess,
    kRedirectDisallowedScheme,
    kRedirectContainsCredentials,
  };

  // Perform a CORS access check on the response. Returns |kAccessAllowed| if
  // access is allowed. Use |accessControlErrorString()| to construct a
  // user-friendly error message for any of the other (error) conditions.
  static AccessStatus checkAccess(const ResourceResponse&,
                                  StoredCredentials,
                                  const SecurityOrigin*);

  // Perform the required CORS checks on the response to a preflight request.
  // Returns |kPreflightSuccess| if preflight response was successful.
  // Use |preflightErrorString()| to construct a user-friendly error message
  // for any of the other (error) conditions.
  static PreflightStatus checkPreflight(const ResourceResponse&);

  // Error checking for the currently experimental
  // "Access-Control-Allow-External:" header. Shares error conditions with
  // standard preflight checking.
  static PreflightStatus checkExternalPreflight(const ResourceResponse&);

  // Given a redirected-to URL, check if the location is allowed
  // according to CORS. That is:
  // - the URL has a CORS supported scheme and
  // - the URL does not contain the userinfo production.
  //
  // Returns |kRedirectSuccess| in all other cases. Use
  // |redirectErrorString()| to construct a user-friendly error
  // message for any of the error conditions.
  static RedirectStatus checkRedirectLocation(const KURL&);

  static bool handleRedirect(PassRefPtr<SecurityOrigin>,
                             ResourceRequest&,
                             const ResourceResponse&,
                             StoredCredentials,
                             ResourceLoaderOptions&,
                             String&);

  // Stringify errors from CORS access checks, preflight or redirect checks.
  static void accessControlErrorString(StringBuilder&,
                                       AccessStatus,
                                       const ResourceResponse&,
                                       const SecurityOrigin*,
                                       WebURLRequest::RequestContext);
  static void preflightErrorString(StringBuilder&,
                                   PreflightStatus,
                                   const ResourceResponse&);
  static void redirectErrorString(StringBuilder&, RedirectStatus, const KURL&);
};

// TODO: also migrate these into the above static class.
PLATFORM_EXPORT bool isOnAccessControlResponseHeaderWhitelist(const String&);

PLATFORM_EXPORT ResourceRequest
createAccessControlPreflightRequest(const ResourceRequest&);

PLATFORM_EXPORT void parseAccessControlExposeHeadersAllowList(
    const String& headerValue,
    HTTPHeaderSet&);
PLATFORM_EXPORT void extractCorsExposedHeaderNamesList(const ResourceResponse&,
                                                       HTTPHeaderSet&);

}  // namespace blink

#endif  // CrossOriginAccessControl_h
