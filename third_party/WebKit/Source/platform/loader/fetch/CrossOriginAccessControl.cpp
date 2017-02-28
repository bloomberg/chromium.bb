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

#include "platform/loader/fetch/CrossOriginAccessControl.h"

#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <memory>

namespace blink {

bool isOnAccessControlResponseHeaderWhitelist(const String& name) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      HTTPHeaderSet, allowedCrossOriginResponseHeaders,
      (new HTTPHeaderSet({
          "cache-control", "content-language", "content-type", "expires",
          "last-modified", "pragma",
      })));
  return allowedCrossOriginResponseHeaders.contains(name);
}

// Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
static AtomicString createAccessControlRequestHeadersHeader(
    const HTTPHeaderMap& headers) {
  Vector<String> filteredHeaders;
  for (const auto& header : headers) {
    if (FetchUtils::isSimpleHeader(header.key, header.value)) {
      // Exclude simple headers.
      continue;
    }
    if (equalIgnoringCase(header.key, "referer")) {
      // When the request is from a Worker, referrer header was added by
      // WorkerThreadableLoader. But it should not be added to
      // Access-Control-Request-Headers header.
      continue;
    }
    filteredHeaders.push_back(header.key.lower());
  }
  if (!filteredHeaders.size())
    return nullAtom;

  // Sort header names lexicographically.
  std::sort(filteredHeaders.begin(), filteredHeaders.end(),
            WTF::codePointCompareLessThan);
  StringBuilder headerBuffer;
  for (const String& header : filteredHeaders) {
    if (!headerBuffer.isEmpty())
      headerBuffer.append(",");
    headerBuffer.append(header);
  }

  return AtomicString(headerBuffer.toString());
}

ResourceRequest createAccessControlPreflightRequest(
    const ResourceRequest& request) {
  const KURL& requestURL = request.url();

  DCHECK(requestURL.user().isEmpty());
  DCHECK(requestURL.pass().isEmpty());

  ResourceRequest preflightRequest(requestURL);
  preflightRequest.setAllowStoredCredentials(false);
  preflightRequest.setHTTPMethod(HTTPNames::OPTIONS);
  preflightRequest.setHTTPHeaderField(HTTPNames::Access_Control_Request_Method,
                                      AtomicString(request.httpMethod()));
  preflightRequest.setPriority(request.priority());
  preflightRequest.setRequestContext(request.requestContext());
  preflightRequest.setServiceWorkerMode(WebURLRequest::ServiceWorkerMode::None);

  if (request.isExternalRequest()) {
    preflightRequest.setHTTPHeaderField(
        HTTPNames::Access_Control_Request_External, "true");
  }

  AtomicString requestHeaders =
      createAccessControlRequestHeadersHeader(request.httpHeaderFields());
  if (requestHeaders != nullAtom) {
    preflightRequest.setHTTPHeaderField(
        HTTPNames::Access_Control_Request_Headers, requestHeaders);
  }

  return preflightRequest;
}

static bool isOriginSeparator(UChar ch) {
  return isASCIISpace(ch) || ch == ',';
}

static bool isInterestingStatusCode(int statusCode) {
  // Predicate that gates what status codes should be included in console error
  // messages for responses containing no access control headers.
  return statusCode >= 400;
}

static void appendOriginDeniedMessage(StringBuilder& builder,
                                      const SecurityOrigin* securityOrigin) {
  builder.append(" Origin '");
  builder.append(securityOrigin->toString());
  builder.append("' is therefore not allowed access.");
}

static void appendNoCORSInformationalMessage(
    StringBuilder& builder,
    WebURLRequest::RequestContext context) {
  if (context != WebURLRequest::RequestContextFetch)
    return;
  builder.append(
      " Have the server send the header with a valid value, or, if an "
      "opaque response serves your needs, set the request's mode to "
      "'no-cors' to fetch the resource with CORS disabled.");
}

CrossOriginAccessControl::AccessStatus CrossOriginAccessControl::checkAccess(
    const ResourceResponse& response,
    StoredCredentials includeCredentials,
    const SecurityOrigin* securityOrigin) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowOriginHeaderName,
      (new AtomicString("access-control-allow-origin")));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowCredentialsHeaderName,
      (new AtomicString("access-control-allow-credentials")));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowSuboriginHeaderName,
      (new AtomicString("access-control-allow-suborigin")));

  int statusCode = response.httpStatusCode();
  if (!statusCode)
    return kInvalidResponse;

  const AtomicString& allowOriginHeaderValue =
      response.httpHeaderField(allowOriginHeaderName);

  // Check Suborigins, unless the Access-Control-Allow-Origin is '*', which
  // implies that all Suborigins are okay as well.
  if (securityOrigin->hasSuborigin() && allowOriginHeaderValue != starAtom) {
    const AtomicString& allowSuboriginHeaderValue =
        response.httpHeaderField(allowSuboriginHeaderName);
    AtomicString atomicSuboriginName(securityOrigin->suborigin()->name());
    if (allowSuboriginHeaderValue != starAtom &&
        allowSuboriginHeaderValue != atomicSuboriginName) {
      return kSubOriginMismatch;
    }
  }

  if (allowOriginHeaderValue == starAtom) {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    if (includeCredentials == DoNotAllowStoredCredentials)
      return kAccessAllowed;
    if (response.isHTTP()) {
      return kWildcardOriginNotAllowed;
    }
  } else if (allowOriginHeaderValue != securityOrigin->toAtomicString()) {
    if (allowOriginHeaderValue.isNull())
      return kMissingAllowOriginHeader;
    if (allowOriginHeaderValue.getString().find(isOriginSeparator, 0) !=
        kNotFound) {
      return kMultipleAllowOriginValues;
    }
    KURL headerOrigin(KURL(), allowOriginHeaderValue);
    if (!headerOrigin.isValid())
      return kInvalidAllowOriginValue;

    return kAllowOriginMismatch;
  }

  if (includeCredentials == AllowStoredCredentials) {
    const AtomicString& allowCredentialsHeaderValue =
        response.httpHeaderField(allowCredentialsHeaderName);
    if (allowCredentialsHeaderValue != "true") {
      return kDisallowCredentialsNotSetToTrue;
    }
  }
  return kAccessAllowed;
}

void CrossOriginAccessControl::accessControlErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::AccessStatus status,
    const ResourceResponse& response,
    const SecurityOrigin* securityOrigin,
    WebURLRequest::RequestContext context) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowOriginHeaderName,
      (new AtomicString("access-control-allow-origin")));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowCredentialsHeaderName,
      (new AtomicString("access-control-allow-credentials")));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      AtomicString, allowSuboriginHeaderName,
      (new AtomicString("access-control-allow-suborigin")));

  switch (status) {
    case kInvalidResponse: {
      builder.append("Invalid response.");
      appendOriginDeniedMessage(builder, securityOrigin);
      return;
    }
    case kSubOriginMismatch: {
      const AtomicString& allowSuboriginHeaderValue =
          response.httpHeaderField(allowSuboriginHeaderName);
      builder.append(
          "The 'Access-Control-Allow-Suborigin' header has a value '");
      builder.append(allowSuboriginHeaderValue);
      builder.append("' that is not equal to the supplied suborigin.");
      appendOriginDeniedMessage(builder, securityOrigin);
      return;
    }
    case kWildcardOriginNotAllowed: {
      builder.append(
          "The value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'.");
      appendOriginDeniedMessage(builder, securityOrigin);
      if (context == WebURLRequest::RequestContextXMLHttpRequest) {
        builder.append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials attribute.");
      }
      return;
    }
    case kMissingAllowOriginHeader: {
      builder.append(
          "No 'Access-Control-Allow-Origin' header is present on the requested "
          "resource.");
      appendOriginDeniedMessage(builder, securityOrigin);
      int statusCode = response.httpStatusCode();
      if (isInterestingStatusCode(statusCode)) {
        builder.append(" The response had HTTP status code ");
        builder.append(String::number(statusCode));
        builder.append('.');
      }
      if (context == WebURLRequest::RequestContextFetch) {
        builder.append(
            " If an opaque response serves your needs, set the request's mode "
            "to 'no-cors' to fetch the resource with CORS disabled.");
      }
      return;
    }
    case kMultipleAllowOriginValues: {
      const AtomicString& allowOriginHeaderValue =
          response.httpHeaderField(allowOriginHeaderName);
      builder.append(
          "The 'Access-Control-Allow-Origin' header contains multiple values "
          "'");
      builder.append(allowOriginHeaderValue);
      builder.append("', but only one is allowed.");
      appendOriginDeniedMessage(builder, securityOrigin);
      appendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kInvalidAllowOriginValue: {
      const AtomicString& allowOriginHeaderValue =
          response.httpHeaderField(allowOriginHeaderName);
      builder.append(
          "The 'Access-Control-Allow-Origin' header contains the invalid "
          "value '");
      builder.append(allowOriginHeaderValue);
      builder.append("'.");
      appendOriginDeniedMessage(builder, securityOrigin);
      appendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kAllowOriginMismatch: {
      const AtomicString& allowOriginHeaderValue =
          response.httpHeaderField(allowOriginHeaderName);
      builder.append("The 'Access-Control-Allow-Origin' header has a value '");
      builder.append(allowOriginHeaderValue);
      builder.append("' that is not equal to the supplied origin.");
      appendOriginDeniedMessage(builder, securityOrigin);
      appendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kDisallowCredentialsNotSetToTrue: {
      const AtomicString& allowCredentialsHeaderValue =
          response.httpHeaderField(allowCredentialsHeaderName);
      builder.append(
          "The value of the 'Access-Control-Allow-Credentials' header in "
          "the response is '");
      builder.append(allowCredentialsHeaderValue);
      builder.append(
          "' which must "
          "be 'true' when the request's credentials mode is 'include'.");
      appendOriginDeniedMessage(builder, securityOrigin);
      if (context == WebURLRequest::RequestContextXMLHttpRequest) {
        builder.append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials attribute.");
      }
      return;
    }
    default:
      NOTREACHED();
  }
}

CrossOriginAccessControl::PreflightStatus
CrossOriginAccessControl::checkPreflight(const ResourceResponse& response) {
  // CORS preflight with 3XX is considered network error in
  // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // CORS Spec: http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
  // https://crbug.com/452394
  int statusCode = response.httpStatusCode();
  if (!FetchUtils::isOkStatus(statusCode))
    return kPreflightInvalidStatus;

  return kPreflightSuccess;
}

CrossOriginAccessControl::PreflightStatus
CrossOriginAccessControl::checkExternalPreflight(
    const ResourceResponse& response) {
  AtomicString result =
      response.httpHeaderField(HTTPNames::Access_Control_Allow_External);
  if (result.isNull())
    return kPreflightMissingAllowExternal;
  if (!equalIgnoringCase(result, "true"))
    return kPreflightInvalidAllowExternal;
  return kPreflightSuccess;
}

void CrossOriginAccessControl::preflightErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::PreflightStatus status,
    const ResourceResponse& response) {
  switch (status) {
    case kPreflightInvalidStatus: {
      int statusCode = response.httpStatusCode();
      builder.append("Response for preflight has invalid HTTP status code ");
      builder.append(String::number(statusCode));
      return;
    }
    case kPreflightMissingAllowExternal: {
      builder.append(
          "No 'Access-Control-Allow-External' header was present in ");
      builder.append(
          "the preflight response for this external request (This is");
      builder.append(" an experimental header which is defined in ");
      builder.append("'https://wicg.github.io/cors-rfc1918/').");
      return;
    }
    case kPreflightInvalidAllowExternal: {
      String result =
          response.httpHeaderField(HTTPNames::Access_Control_Allow_External);
      builder.append("The 'Access-Control-Allow-External' header in the ");
      builder.append(
          "preflight response for this external request had a value");
      builder.append(" of '");
      builder.append(result);
      builder.append("',  not 'true' (This is an experimental header which is");
      builder.append(" defined in 'https://wicg.github.io/cors-rfc1918/').");
      return;
    }
    default:
      NOTREACHED();
  }
}

void parseAccessControlExposeHeadersAllowList(const String& headerValue,
                                              HTTPHeaderSet& headerSet) {
  Vector<String> headers;
  headerValue.split(',', false, headers);
  for (unsigned headerCount = 0; headerCount < headers.size(); headerCount++) {
    String strippedHeader = headers[headerCount].stripWhiteSpace();
    if (!strippedHeader.isEmpty())
      headerSet.insert(strippedHeader);
  }
}

void extractCorsExposedHeaderNamesList(const ResourceResponse& response,
                                       HTTPHeaderSet& headerSet) {
  // If a response was fetched via a service worker, it will always have
  // corsExposedHeaderNames set, either from the Access-Control-Expose-Headers
  // header, or explicitly via foreign fetch. For requests that didn't come from
  // a service worker, foreign fetch doesn't apply so just parse the CORS
  // header.
  if (response.wasFetchedViaServiceWorker()) {
    for (const auto& header : response.corsExposedHeaderNames())
      headerSet.insert(header);
    return;
  }
  parseAccessControlExposeHeadersAllowList(
      response.httpHeaderField(HTTPNames::Access_Control_Expose_Headers),
      headerSet);
}

CrossOriginAccessControl::RedirectStatus
CrossOriginAccessControl::checkRedirectLocation(const KURL& requestURL) {
  // Block non HTTP(S) schemes as specified in the step 4 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch. Chromium also allows
  // the data scheme.
  //
  // TODO(tyoshino): This check should be performed regardless of the CORS flag
  // and request's mode.
  if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(requestURL.protocol()))
    return kRedirectDisallowedScheme;

  // Block URLs including credentials as specified in the step 9 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch.
  //
  // TODO(tyoshino): This check should be performed also when request's
  // origin is not same origin with the redirect destination's origin.
  if (!(requestURL.user().isEmpty() && requestURL.pass().isEmpty()))
    return kRedirectContainsCredentials;

  return kRedirectSuccess;
}

void CrossOriginAccessControl::redirectErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::RedirectStatus status,
    const KURL& requestURL) {
  switch (status) {
    case kRedirectDisallowedScheme: {
      builder.append("Redirect location '");
      builder.append(requestURL.getString());
      builder.append("' has a disallowed scheme for cross-origin requests.");
      return;
    }
    case kRedirectContainsCredentials: {
      builder.append("Redirect location '");
      builder.append(requestURL.getString());
      builder.append(
          "' contains a username and password, which is disallowed for"
          " cross-origin requests.");
      return;
    }
    default:
      NOTREACHED();
  }
}

bool CrossOriginAccessControl::handleRedirect(
    PassRefPtr<SecurityOrigin> securityOrigin,
    ResourceRequest& newRequest,
    const ResourceResponse& redirectResponse,
    StoredCredentials withCredentials,
    ResourceLoaderOptions& options,
    String& errorMessage) {
  // http://www.w3.org/TR/cors/#redirect-steps terminology:
  const KURL& lastURL = redirectResponse.url();
  const KURL& newURL = newRequest.url();

  RefPtr<SecurityOrigin> currentSecurityOrigin = securityOrigin;

  RefPtr<SecurityOrigin> newSecurityOrigin = currentSecurityOrigin;

  // TODO(tyoshino): This should be fixed to check not only the last one but
  // all redirect responses.
  if (!currentSecurityOrigin->canRequest(lastURL)) {
    // Follow http://www.w3.org/TR/cors/#redirect-steps
    CrossOriginAccessControl::RedirectStatus redirectStatus =
        CrossOriginAccessControl::checkRedirectLocation(newURL);
    if (redirectStatus != kRedirectSuccess) {
      StringBuilder builder;
      builder.append("Redirect from '");
      builder.append(lastURL.getString());
      builder.append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::redirectErrorString(builder, redirectStatus,
                                                    newURL);
      errorMessage = builder.toString();
      return false;
    }

    // Step 5: perform resource sharing access check.
    CrossOriginAccessControl::AccessStatus corsStatus =
        CrossOriginAccessControl::checkAccess(redirectResponse, withCredentials,
                                              currentSecurityOrigin.get());
    if (corsStatus != kAccessAllowed) {
      StringBuilder builder;
      builder.append("Redirect from '");
      builder.append(lastURL.getString());
      builder.append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::accessControlErrorString(
          builder, corsStatus, redirectResponse, currentSecurityOrigin.get(),
          newRequest.requestContext());
      errorMessage = builder.toString();
      return false;
    }

    RefPtr<SecurityOrigin> lastOrigin = SecurityOrigin::create(lastURL);
    // Set request's origin to a globally unique identifier as specified in
    // the step 10 in https://fetch.spec.whatwg.org/#http-redirect-fetch.
    if (!lastOrigin->canRequest(newURL)) {
      options.securityOrigin = SecurityOrigin::createUnique();
      newSecurityOrigin = options.securityOrigin;
    }
  }

  if (!currentSecurityOrigin->canRequest(newURL)) {
    newRequest.clearHTTPOrigin();
    newRequest.setHTTPOrigin(newSecurityOrigin.get());

    // Unset credentials flag if request's credentials mode is "same-origin" as
    // request's response tainting becomes "cors".
    //
    // This is equivalent to the step 2 in
    // https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
    if (options.credentialsRequested == ClientDidNotRequestCredentials)
      options.allowCredentials = DoNotAllowStoredCredentials;
  }
  return true;
}

}  // namespace blink
