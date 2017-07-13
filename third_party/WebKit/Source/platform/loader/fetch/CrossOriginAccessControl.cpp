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

#include <algorithm>
#include <memory>
#include "net/http/http_util.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

bool CrossOriginAccessControl::IsOnAccessControlResponseHeaderWhitelist(
    const String& name) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      HTTPHeaderSet, allowed_cross_origin_response_headers,
      ({
          "cache-control", "content-language", "content-type", "expires",
          "last-modified", "pragma",
      }));
  return allowed_cross_origin_response_headers.Contains(name);
}

// Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
static AtomicString CreateAccessControlRequestHeadersHeader(
    const HTTPHeaderMap& headers) {
  Vector<String> filtered_headers;
  for (const auto& header : headers) {
    if (FetchUtils::IsCORSSafelistedHeader(header.key, header.value)) {
      // Exclude CORS-safelisted headers.
      continue;
    }
    if (DeprecatedEqualIgnoringCase(header.key, "referer")) {
      // When the request is from a Worker, referrer header was added by
      // WorkerThreadableLoader. But it should not be added to
      // Access-Control-Request-Headers header.
      continue;
    }
    filtered_headers.push_back(header.key.DeprecatedLower());
  }
  if (!filtered_headers.size())
    return g_null_atom;

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end(),
            WTF::CodePointCompareLessThan);
  StringBuilder header_buffer;
  for (const String& header : filtered_headers) {
    if (!header_buffer.IsEmpty())
      header_buffer.Append(",");
    header_buffer.Append(header);
  }

  return AtomicString(header_buffer.ToString());
}

ResourceRequest CrossOriginAccessControl::CreateAccessControlPreflightRequest(
    const ResourceRequest& request) {
  const KURL& request_url = request.Url();

  DCHECK(request_url.User().IsEmpty());
  DCHECK(request_url.Pass().IsEmpty());

  ResourceRequest preflight_request(request_url);
  preflight_request.SetHTTPMethod(HTTPNames::OPTIONS);
  preflight_request.SetHTTPHeaderField(HTTPNames::Access_Control_Request_Method,
                                       AtomicString(request.HttpMethod()));
  preflight_request.SetPriority(request.Priority());
  preflight_request.SetRequestContext(request.GetRequestContext());
  preflight_request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeOmit);
  preflight_request.SetServiceWorkerMode(
      WebURLRequest::ServiceWorkerMode::kNone);

  if (request.IsExternalRequest()) {
    preflight_request.SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_External, "true");
  }

  AtomicString request_headers =
      CreateAccessControlRequestHeadersHeader(request.HttpHeaderFields());
  if (request_headers != g_null_atom) {
    preflight_request.SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_Headers, request_headers);
  }

  return preflight_request;
}

static bool IsOriginSeparator(UChar ch) {
  return IsASCIISpace(ch) || ch == ',';
}

CrossOriginAccessControl::AccessStatus CrossOriginAccessControl::CheckAccess(
    const ResourceResponse& response,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const SecurityOrigin* security_origin) {
  static const char allow_origin_header_name[] = "access-control-allow-origin";
  static const char allow_credentials_header_name[] =
      "access-control-allow-credentials";
  static const char allow_suborigin_header_name[] =
      "access-control-allow-suborigin";
  int status_code = response.HttpStatusCode();
  if (!status_code)
    return kInvalidResponse;

  const AtomicString& allow_origin_header_value =
      response.HttpHeaderField(allow_origin_header_name);

  // Check Suborigins, unless the Access-Control-Allow-Origin is '*', which
  // implies that all Suborigins are okay as well.
  if (security_origin->HasSuborigin() &&
      allow_origin_header_value != g_star_atom) {
    const AtomicString& allow_suborigin_header_value =
        response.HttpHeaderField(allow_suborigin_header_name);
    AtomicString atomic_suborigin_name(
        security_origin->GetSuborigin()->GetName());
    if (allow_suborigin_header_value != g_star_atom &&
        allow_suborigin_header_value != atomic_suborigin_name) {
      return kSubOriginMismatch;
    }
  }

  if (allow_origin_header_value == "*") {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    if (!FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode))
      return kAccessAllowed;
    if (response.IsHTTP()) {
      return kWildcardOriginNotAllowed;
    }
  } else if (allow_origin_header_value != security_origin->ToAtomicString()) {
    if (allow_origin_header_value.IsNull())
      return kMissingAllowOriginHeader;
    if (allow_origin_header_value.GetString().Find(IsOriginSeparator, 0) !=
        kNotFound) {
      return kMultipleAllowOriginValues;
    }
    KURL header_origin(NullURL(), allow_origin_header_value);
    if (!header_origin.IsValid())
      return kInvalidAllowOriginValue;

    return kAllowOriginMismatch;
  }

  if (FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode)) {
    const AtomicString& allow_credentials_header_value =
        response.HttpHeaderField(allow_credentials_header_name);
    if (allow_credentials_header_value != "true") {
      return kDisallowCredentialsNotSetToTrue;
    }
  }
  return kAccessAllowed;
}

static bool IsInterestingStatusCode(int status_code) {
  // Predicate that gates what status codes should be included in console error
  // messages for responses containing no access control headers.
  return status_code >= 400;
}

static void AppendOriginDeniedMessage(StringBuilder& builder,
                                      const SecurityOrigin* security_origin) {
  builder.Append(" Origin '");
  builder.Append(security_origin->ToString());
  builder.Append("' is therefore not allowed access.");
}

static void AppendNoCORSInformationalMessage(
    StringBuilder& builder,
    WebURLRequest::RequestContext context) {
  if (context != WebURLRequest::kRequestContextFetch)
    return;
  builder.Append(
      " Have the server send the header with a valid value, or, if an "
      "opaque response serves your needs, set the request's mode to "
      "'no-cors' to fetch the resource with CORS disabled.");
}

void CrossOriginAccessControl::AccessControlErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::AccessStatus status,
    const ResourceResponse& response,
    const SecurityOrigin* security_origin,
    WebURLRequest::RequestContext context) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allow_origin_header_name,
                                  ("access-control-allow-origin"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allow_credentials_header_name,
                                  ("access-control-allow-credentials"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allow_suborigin_header_name,
                                  ("access-control-allow-suborigin"));

  switch (status) {
    case kInvalidResponse: {
      builder.Append("Invalid response.");
      AppendOriginDeniedMessage(builder, security_origin);
      return;
    }
    case kSubOriginMismatch: {
      const AtomicString& allow_suborigin_header_value =
          response.HttpHeaderField(allow_suborigin_header_name);
      builder.Append(
          "The 'Access-Control-Allow-Suborigin' header has a value '");
      builder.Append(allow_suborigin_header_value);
      builder.Append("' that is not equal to the supplied suborigin.");
      AppendOriginDeniedMessage(builder, security_origin);
      return;
    }
    case kWildcardOriginNotAllowed: {
      builder.Append(
          "The value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'.");
      AppendOriginDeniedMessage(builder, security_origin);
      if (context == WebURLRequest::kRequestContextXMLHttpRequest) {
        builder.Append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials attribute.");
      }
      return;
    }
    case kMissingAllowOriginHeader: {
      builder.Append(
          "No 'Access-Control-Allow-Origin' header is present on the requested "
          "resource.");
      AppendOriginDeniedMessage(builder, security_origin);
      int status_code = response.HttpStatusCode();
      if (IsInterestingStatusCode(status_code)) {
        builder.Append(" The response had HTTP status code ");
        builder.Append(String::Number(status_code));
        builder.Append('.');
      }
      if (context == WebURLRequest::kRequestContextFetch) {
        builder.Append(
            " If an opaque response serves your needs, set the request's mode "
            "to 'no-cors' to fetch the resource with CORS disabled.");
      }
      return;
    }
    case kMultipleAllowOriginValues: {
      const AtomicString& allow_origin_header_value =
          response.HttpHeaderField(allow_origin_header_name);
      builder.Append(
          "The 'Access-Control-Allow-Origin' header contains multiple values "
          "'");
      builder.Append(allow_origin_header_value);
      builder.Append("', but only one is allowed.");
      AppendOriginDeniedMessage(builder, security_origin);
      AppendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kInvalidAllowOriginValue: {
      const AtomicString& allow_origin_header_value =
          response.HttpHeaderField(allow_origin_header_name);
      builder.Append(
          "The 'Access-Control-Allow-Origin' header contains the invalid "
          "value '");
      builder.Append(allow_origin_header_value);
      builder.Append("'.");
      AppendOriginDeniedMessage(builder, security_origin);
      AppendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kAllowOriginMismatch: {
      const AtomicString& allow_origin_header_value =
          response.HttpHeaderField(allow_origin_header_name);
      builder.Append("The 'Access-Control-Allow-Origin' header has a value '");
      builder.Append(allow_origin_header_value);
      builder.Append("' that is not equal to the supplied origin.");
      AppendOriginDeniedMessage(builder, security_origin);
      AppendNoCORSInformationalMessage(builder, context);
      return;
    }
    case kDisallowCredentialsNotSetToTrue: {
      const AtomicString& allow_credentials_header_value =
          response.HttpHeaderField(allow_credentials_header_name);
      builder.Append(
          "The value of the 'Access-Control-Allow-Credentials' header in "
          "the response is '");
      builder.Append(allow_credentials_header_value);
      builder.Append(
          "' which must "
          "be 'true' when the request's credentials mode is 'include'.");
      AppendOriginDeniedMessage(builder, security_origin);
      if (context == WebURLRequest::kRequestContextXMLHttpRequest) {
        builder.Append(
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
CrossOriginAccessControl::CheckPreflight(const ResourceResponse& response) {
  // CORS preflight with 3XX is considered network error in
  // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // CORS Spec: http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
  // https://crbug.com/452394
  int status_code = response.HttpStatusCode();
  if (!FetchUtils::IsOkStatus(status_code))
    return kPreflightInvalidStatus;

  return kPreflightSuccess;
}

CrossOriginAccessControl::PreflightStatus
CrossOriginAccessControl::CheckExternalPreflight(
    const ResourceResponse& response) {
  AtomicString result =
      response.HttpHeaderField(HTTPNames::Access_Control_Allow_External);
  if (result.IsNull())
    return kPreflightMissingAllowExternal;
  if (!DeprecatedEqualIgnoringCase(result, "true"))
    return kPreflightInvalidAllowExternal;
  return kPreflightSuccess;
}

void CrossOriginAccessControl::PreflightErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::PreflightStatus status,
    const ResourceResponse& response) {
  switch (status) {
    case kPreflightInvalidStatus: {
      int status_code = response.HttpStatusCode();
      builder.Append("Response for preflight has invalid HTTP status code ");
      builder.Append(String::Number(status_code));
      return;
    }
    case kPreflightMissingAllowExternal: {
      builder.Append(
          "No 'Access-Control-Allow-External' header was present in ");
      builder.Append(
          "the preflight response for this external request (This is");
      builder.Append(" an experimental header which is defined in ");
      builder.Append("'https://wicg.github.io/cors-rfc1918/').");
      return;
    }
    case kPreflightInvalidAllowExternal: {
      String result =
          response.HttpHeaderField(HTTPNames::Access_Control_Allow_External);
      builder.Append("The 'Access-Control-Allow-External' header in the ");
      builder.Append(
          "preflight response for this external request had a value");
      builder.Append(" of '");
      builder.Append(result);
      builder.Append("',  not 'true' (This is an experimental header which is");
      builder.Append(" defined in 'https://wicg.github.io/cors-rfc1918/').");
      return;
    }
    default:
      NOTREACHED();
  }
}

// A parser for the value of the Access-Control-Expose-Headers header.
class HTTPHeaderNameListParser {
  STACK_ALLOCATED();

 public:
  explicit HTTPHeaderNameListParser(const String& value)
      : value_(value), pos_(0) {}

  // Tries parsing |value_| expecting it to be conforming to the #field-name
  // ABNF rule defined in RFC 7230. Returns with the field-name entries stored
  // in |output| when successful. Otherwise, returns with |output| kept empty.
  //
  // |output| must be empty.
  void Parse(HTTPHeaderSet& output) {
    DCHECK(output.IsEmpty());

    while (true) {
      ConsumeSpaces();

      size_t token_start = pos_;
      ConsumeTokenChars();
      size_t token_size = pos_ - token_start;
      if (token_size == 0) {
        output.clear();
        return;
      }
      output.insert(value_.Substring(token_start, token_size));

      ConsumeSpaces();

      if (pos_ == value_.length()) {
        return;
      }

      if (value_[pos_] == ',') {
        ++pos_;
      } else {
        output.clear();
        return;
      }
    }
  }

 private:
  // Consumes zero or more spaces (SP and HTAB) from value_.
  void ConsumeSpaces() {
    while (true) {
      if (pos_ == value_.length()) {
        return;
      }

      UChar c = value_[pos_];
      if (c != ' ' && c != '\t') {
        return;
      }
      ++pos_;
    }
  }

  // Consumes zero or more tchars from value_.
  void ConsumeTokenChars() {
    while (true) {
      if (pos_ == value_.length()) {
        return;
      }

      UChar c = value_[pos_];
      if (c > 0x7F || !net::HttpUtil::IsTokenChar(c)) {
        return;
      }
      ++pos_;
    }
  }

  const String& value_;
  size_t pos_;
};

void CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
    const String& header_value,
    HTTPHeaderSet& header_set) {
  HTTPHeaderNameListParser parser(header_value);
  parser.Parse(header_set);
}

void CrossOriginAccessControl::ExtractCorsExposedHeaderNamesList(
    const ResourceResponse& response,
    HTTPHeaderSet& header_set) {
  // If a response was fetched via a service worker, it will always have
  // corsExposedHeaderNames set, either from the Access-Control-Expose-Headers
  // header, or explicitly via foreign fetch. For requests that didn't come from
  // a service worker, foreign fetch doesn't apply so just parse the CORS
  // header.
  if (response.WasFetchedViaServiceWorker()) {
    for (const auto& header : response.CorsExposedHeaderNames())
      header_set.insert(header);
    return;
  }
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
      response.HttpHeaderField(HTTPNames::Access_Control_Expose_Headers),
      header_set);
}

CrossOriginAccessControl::RedirectStatus
CrossOriginAccessControl::CheckRedirectLocation(const KURL& request_url) {
  // Block non HTTP(S) schemes as specified in the step 4 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch. Chromium also allows
  // the data scheme.
  //
  // TODO(tyoshino): This check should be performed regardless of the CORS flag
  // and request's mode.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request_url.Protocol()))
    return kRedirectDisallowedScheme;

  // Block URLs including credentials as specified in the step 9 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch.
  //
  // TODO(tyoshino): This check should be performed also when request's
  // origin is not same origin with the redirect destination's origin.
  if (!(request_url.User().IsEmpty() && request_url.Pass().IsEmpty()))
    return kRedirectContainsCredentials;

  return kRedirectSuccess;
}

void CrossOriginAccessControl::RedirectErrorString(
    StringBuilder& builder,
    CrossOriginAccessControl::RedirectStatus status,
    const KURL& request_url) {
  switch (status) {
    case kRedirectDisallowedScheme: {
      builder.Append("Redirect location '");
      builder.Append(request_url.GetString());
      builder.Append("' has a disallowed scheme for cross-origin requests.");
      return;
    }
    case kRedirectContainsCredentials: {
      builder.Append("Redirect location '");
      builder.Append(request_url.GetString());
      builder.Append(
          "' contains a username and password, which is disallowed for"
          " cross-origin requests.");
      return;
    }
    default:
      NOTREACHED();
  }
}

bool CrossOriginAccessControl::HandleRedirect(
    RefPtr<SecurityOrigin> current_security_origin,
    ResourceRequest& new_request,
    const ResourceResponse& redirect_response,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    ResourceLoaderOptions& options,
    String& error_message) {
  // http://www.w3.org/TR/cors/#redirect-steps terminology:
  const KURL& last_url = redirect_response.Url();
  const KURL& new_url = new_request.Url();

  RefPtr<SecurityOrigin> new_security_origin = current_security_origin;

  // TODO(tyoshino): This should be fixed to check not only the last one but
  // all redirect responses.
  if (!current_security_origin->CanRequest(last_url)) {
    // Follow http://www.w3.org/TR/cors/#redirect-steps
    CrossOriginAccessControl::RedirectStatus redirect_status =
        CrossOriginAccessControl::CheckRedirectLocation(new_url);
    if (redirect_status != kRedirectSuccess) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(last_url.GetString());
      builder.Append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::RedirectErrorString(builder, redirect_status,
                                                    new_url);
      error_message = builder.ToString();
      return false;
    }

    // Step 5: perform resource sharing access check.
    CrossOriginAccessControl::AccessStatus cors_status =
        CrossOriginAccessControl::CheckAccess(
            redirect_response, credentials_mode, current_security_origin.Get());
    if (cors_status != kAccessAllowed) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(last_url.GetString());
      builder.Append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::AccessControlErrorString(
          builder, cors_status, redirect_response,
          current_security_origin.Get(), new_request.GetRequestContext());
      error_message = builder.ToString();
      return false;
    }

    RefPtr<SecurityOrigin> last_origin = SecurityOrigin::Create(last_url);
    // Set request's origin to a globally unique identifier as specified in
    // the step 10 in https://fetch.spec.whatwg.org/#http-redirect-fetch.
    if (!last_origin->CanRequest(new_url)) {
      options.security_origin = SecurityOrigin::CreateUnique();
      new_security_origin = options.security_origin;
    }
  }

  if (!current_security_origin->CanRequest(new_url)) {
    new_request.ClearHTTPOrigin();
    new_request.SetHTTPOrigin(new_security_origin.Get());

    options.cors_flag = true;
  }
  return true;
}

}  // namespace blink
