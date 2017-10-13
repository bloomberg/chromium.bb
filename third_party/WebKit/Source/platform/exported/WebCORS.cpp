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

#include "public/platform/WebCORS.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "platform/http_names.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURLResponse.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace blink {

namespace WebCORS {

namespace {

bool IsInterestingStatusCode(int status_code) {
  // Predicate that gates what status codes should be included in console error
  // messages for responses containing no access control headers.
  return status_code >= 400;
}

// Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
String CreateAccessControlRequestHeadersHeader(
    const WebHTTPHeaderMap& headers) {
  Vector<String> filtered_headers;
  for (const auto& header : headers.GetHTTPHeaderMap()) {
    if (FetchUtils::IsCORSSafelistedHeader(header.key, header.value)) {
      // Exclude CORS-safelisted headers.
      continue;
    }
    // TODO(hintzed) replace with EqualIgnoringASCIICase()
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

  return header_buffer.ToString();
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
  void Parse(WebHTTPHeaderSet& output) {
    DCHECK(output.empty());

    while (true) {
      ConsumeSpaces();

      size_t token_start = pos_;
      ConsumeTokenChars();
      size_t token_size = pos_ - token_start;
      if (token_size == 0) {
        output.clear();
        return;
      }

      const CString& name = value_.Substring(token_start, token_size).Ascii();
      output.emplace(name.data(), name.length());

      ConsumeSpaces();

      if (pos_ == value_.length())
        return;

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
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c != ' ' && c != '\t')
        return;
      ++pos_;
    }
  }

  // Consumes zero or more tchars from value_.
  void ConsumeTokenChars() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c > 0x7F || !net::HttpUtil::IsTokenChar(c))
        return;
      ++pos_;
    }
  }

  const String value_;
  size_t pos_;
};

static bool IsOriginSeparator(UChar ch) {
  return IsASCIISpace(ch) || ch == ',';
}

}  // namespace

AccessStatus CheckAccess(
    const WebURL response_url,
    const int response_status_code,
    const WebHTTPHeaderMap& response_header,
    const WebURLRequest::FetchCredentialsMode credentials_mode,
    const WebSecurityOrigin& security_origin) {
  if (!response_status_code)
    return AccessStatus::kInvalidResponse;

  const WebString& allow_origin_header_value =
      response_header.Get(HTTPNames::Access_Control_Allow_Origin);

  // Check Suborigins, unless the Access-Control-Allow-Origin is '*', which
  // implies that all Suborigins are okay as well.
  if (!security_origin.Suborigin().IsEmpty() &&
      allow_origin_header_value != WebString(g_star_atom)) {
    const WebString& allow_suborigin_header_value =
        response_header.Get(HTTPNames::Access_Control_Allow_Suborigin);
    if (allow_suborigin_header_value != WebString(g_star_atom) &&
        allow_suborigin_header_value != security_origin.Suborigin()) {
      return AccessStatus::kSubOriginMismatch;
    }
  }

  if (allow_origin_header_value == "*") {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    if (!FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode))
      return AccessStatus::kAccessAllowed;
    // TODO(hintzed): Is the following a sound substitute for
    // blink::ResourceResponse::IsHTTP()?
    if (GURL(response_url.GetString().Utf16()).SchemeIsHTTPOrHTTPS())
      return AccessStatus::kWildcardOriginNotAllowed;
  } else if (allow_origin_header_value != security_origin.ToString()) {
    if (allow_origin_header_value.IsNull())
      return AccessStatus::kMissingAllowOriginHeader;
    if (String(allow_origin_header_value).Find(IsOriginSeparator, 0) !=
        kNotFound) {
      return AccessStatus::kMultipleAllowOriginValues;
    }
    KURL header_origin(NullURL(), allow_origin_header_value);
    if (!header_origin.IsValid())
      return AccessStatus::kInvalidAllowOriginValue;

    return AccessStatus::kAllowOriginMismatch;
  }

  if (FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode)) {
    const WebString& allow_credentials_header_value =
        response_header.Get(HTTPNames::Access_Control_Allow_Credentials);
    if (allow_credentials_header_value != "true")
      return AccessStatus::kDisallowCredentialsNotSetToTrue;
  }
  return AccessStatus::kAccessAllowed;
}

bool HandleRedirect(WebSecurityOrigin& current_security_origin,
                    WebURLRequest& new_request,
                    const WebURL redirect_response_url,
                    const int redirect_response_status_code,
                    const WebHTTPHeaderMap& redirect_response_header,
                    WebURLRequest::FetchCredentialsMode credentials_mode,
                    ResourceLoaderOptions& options,
                    WebString& error_message) {
  const KURL& last_url = redirect_response_url;
  const KURL& new_url = new_request.Url();

  WebSecurityOrigin& new_security_origin = current_security_origin;

  // TODO(tyoshino): This should be fixed to check not only the last one but
  // all redirect responses.
  if (!current_security_origin.CanRequest(last_url)) {
    RedirectStatus redirect_status = CheckRedirectLocation(new_url);
    if (redirect_status != RedirectStatus::kRedirectSuccess) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(last_url.GetString());
      builder.Append("' has been blocked by CORS policy: ");
      builder.Append(RedirectErrorString(redirect_status, new_url));
      error_message = builder.ToString();
      return false;
    }

    AccessStatus cors_status = CheckAccess(
        redirect_response_url, redirect_response_status_code,
        redirect_response_header, credentials_mode, current_security_origin);
    if (cors_status != AccessStatus::kAccessAllowed) {
      StringBuilder builder;
      builder.Append("Redirect from '");
      builder.Append(last_url.GetString());
      builder.Append("' has been blocked by CORS policy: ");
      builder.Append(AccessControlErrorString(
          cors_status, redirect_response_status_code, redirect_response_header,
          WebSecurityOrigin(current_security_origin.Get()),
          new_request.GetRequestContext()));
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

  if (!current_security_origin.CanRequest(new_url)) {
    new_request.ClearHTTPHeaderField(WebString(HTTPNames::Suborigin));
    new_request.SetHTTPHeaderField(WebString(HTTPNames::Origin),
                                   new_security_origin.ToString());
    if (!new_security_origin.Suborigin().IsEmpty()) {
      new_request.SetHTTPHeaderField(WebString(HTTPNames::Suborigin),
                                     new_security_origin.Suborigin());
    }

    options.cors_flag = true;
  }
  return true;
}

RedirectStatus CheckRedirectLocation(const WebURL& web_request_url) {
  // Block non HTTP(S) schemes as specified in the step 4 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch. Chromium also allows
  // the data scheme.
  //
  // TODO(tyoshino): This check should be performed regardless of the CORS flag
  // and request's mode.
  KURL request_url = web_request_url;

  if (!SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request_url.Protocol())) {
    return RedirectStatus::kRedirectDisallowedScheme;
  }

  // Block URLs including credentials as specified in the step 9 in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch.
  //
  // TODO(tyoshino): This check should be performed also when request's
  // origin is not same origin with the redirect destination's origin.
  if (!(request_url.User().IsEmpty() && request_url.Pass().IsEmpty()))
    return RedirectStatus::kRedirectContainsCredentials;

  return RedirectStatus::kRedirectSuccess;
}

PreflightStatus CheckPreflight(const int preflight_response_status_code) {
  // CORS preflight with 3XX is considered network error in
  // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // CORS Spec: http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
  // https://crbug.com/452394
  if (!FetchUtils::IsOkStatus(preflight_response_status_code))
    return PreflightStatus::kPreflightInvalidStatus;

  return PreflightStatus::kPreflightSuccess;
}

PreflightStatus CheckExternalPreflight(
    const WebHTTPHeaderMap& response_header) {
  WebString result =
      response_header.Get(HTTPNames::Access_Control_Allow_External);
  if (result.IsNull())
    return PreflightStatus::kPreflightMissingAllowExternal;
  if (!EqualIgnoringASCIICase(result, "true"))
    return PreflightStatus::kPreflightInvalidAllowExternal;
  return PreflightStatus::kPreflightSuccess;
}

WebURLRequest CreateAccessControlPreflightRequest(
    const WebURLRequest& request) {
  const KURL& request_url = request.Url();

  DCHECK(request_url.User().IsEmpty());
  DCHECK(request_url.Pass().IsEmpty());

  WebURLRequest preflight_request(request_url);
  preflight_request.SetHTTPMethod(HTTPNames::OPTIONS);
  preflight_request.SetHTTPHeaderField(HTTPNames::Access_Control_Request_Method,
                                       request.HttpMethod());
  preflight_request.SetPriority(request.GetPriority());
  preflight_request.SetRequestContext(request.GetRequestContext());
  preflight_request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeOmit);
  preflight_request.SetServiceWorkerMode(
      WebURLRequest::ServiceWorkerMode::kNone);
  preflight_request.SetHTTPReferrer(request.HttpHeaderField(HTTPNames::Referer),
                                    request.GetReferrerPolicy());

  if (request.IsExternalRequest()) {
    preflight_request.SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_External, "true");
  }

  String request_headers = CreateAccessControlRequestHeadersHeader(
      request.ToResourceRequest().HttpHeaderFields());
  if (request_headers != g_null_atom) {
    preflight_request.SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_Headers, request_headers);
  }

  return preflight_request;
}

WebString AccessControlErrorString(
    const AccessStatus status,
    const int response_status_code,
    const WebHTTPHeaderMap& response_header,
    const WebSecurityOrigin& origin,
    const WebURLRequest::RequestContext context) {
  String origin_denied =
      String::Format("Origin '%s' is therefore not allowed access.",
                     origin.ToString().Utf8().data());

  String no_cors_information =
      context == WebURLRequest::kRequestContextFetch
          ? " Have the server send the header with a valid value, or, if an "
            "opaque response serves your needs, set the request's mode to "
            "'no-cors' to fetch the resource with CORS disabled."
          : "";

  switch (status) {
    case AccessStatus::kAccessAllowed:
      NOTREACHED();
      return "";
    case AccessStatus::kInvalidResponse:
      return String::Format("Invalid response. %s",
                            origin_denied.Utf8().data());
    case AccessStatus::kSubOriginMismatch:
      return String::Format(
          "The 'Access-Control-Allow-Suborigin' header has a value '%s' that "
          "is not equal to the supplied suborigin. %s",
          response_header.Get(HTTPNames::Access_Control_Allow_Suborigin)
              .Utf8()
              .data(),
          origin_denied.Utf8().data());
    case AccessStatus::kWildcardOriginNotAllowed:
      return String::Format(
          "The value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'. %s%s",
          origin_denied.Utf8().data(),
          context == WebURLRequest::kRequestContextXMLHttpRequest
              ? " The credentials mode of requests initiated by the "
                "XMLHttpRequest is controlled by the withCredentials attribute."
              : "");
    case AccessStatus::kMissingAllowOriginHeader:
      return String::Format(
          "No 'Access-Control-Allow-Origin' header is present on the "
          "requested resource. %s%s%s",
          origin_denied.Utf8().data(),
          IsInterestingStatusCode(response_status_code)
              ? String::Format(" The response had HTTP status code %d.",
                               response_status_code)
                    .Utf8()
                    .data()
              : "",
          context == WebURLRequest::kRequestContextFetch
              ? " If an opaque response serves your needs, set the request's "
                "mode to 'no-cors' to fetch the resource with CORS disabled."
              : "");
    case AccessStatus::kMultipleAllowOriginValues:
      return String::Format(
          "The 'Access-Control-Allow-Origin' header contains multiple values "
          "'%s', but only one is allowed. %s%s",
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin_denied.Utf8().data(), no_cors_information.Utf8().data());
    case AccessStatus::kInvalidAllowOriginValue:
      return String::Format(
          "The 'Access-Control-Allow-Origin' header contains the invalid value "
          "'%s'. %s%s",
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin_denied.Utf8().data(), no_cors_information.Utf8().data());
    case AccessStatus::kAllowOriginMismatch:
      return String::Format(
          "The 'Access-Control-Allow-Origin' header has a value '%s' that is "
          "not equal to the supplied origin. %s%s",
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin_denied.Utf8().data(), no_cors_information.Utf8().data());
    case AccessStatus::kDisallowCredentialsNotSetToTrue:
      return String::Format(
          "The value of the 'Access-Control-Allow-Credentials' header in "
          "the response is '%s' which must be 'true' when the request's "
          "credentials mode is 'include'. %s%s",
          response_header.Get(HTTPNames::Access_Control_Allow_Credentials)
              .Utf8()
              .data(),
          origin_denied.Utf8().data(),
          (context == WebURLRequest::kRequestContextXMLHttpRequest
               ? " The credentials mode of requests initiated by the "
                 "XMLHttpRequest is controlled by the withCredentials "
                 "attribute."
               : ""));
  }
  NOTREACHED();
  return "";
}

WebString PreflightErrorString(const PreflightStatus status,
                               const WebHTTPHeaderMap& response_header,
                               const int preflight_response_status_code) {
  switch (status) {
    case PreflightStatus::kPreflightSuccess:
      NOTREACHED();
      return "";
    case PreflightStatus::kPreflightInvalidStatus:
      return String::Format(
          "Response for preflight has invalid HTTP status code %d",
          preflight_response_status_code);
    case PreflightStatus::kPreflightMissingAllowExternal:
      return String::Format(
          "No 'Access-Control-Allow-External' header was present in the "
          "preflight response for this external request (This is an "
          "experimental header which is defined in "
          "'https://wicg.github.io/cors-rfc1918/').");
    case PreflightStatus::kPreflightInvalidAllowExternal:
      return String::Format(
          "The 'Access-Control-Allow-External' header in the preflight "
          "response for this external request had a value of '%s',  not 'true' "
          "(This is an experimental header which is defined in "
          "'https://wicg.github.io/cors-rfc1918/').",
          response_header.Get(HTTPNames::Access_Control_Allow_External)
              .Utf8()
              .data());
  }
  NOTREACHED();
  return "";
}

WebString RedirectErrorString(const RedirectStatus status,
                              const WebURL& redirect_url) {
  switch (status) {
    case RedirectStatus::kRedirectSuccess:
      NOTREACHED();
      return "";
    case RedirectStatus::kRedirectDisallowedScheme:
      return String::Format(
          "Redirect location '%s' has a disallowed scheme for cross-origin "
          "requests.",
          redirect_url.GetString().Utf8().data());
    case RedirectStatus::kRedirectContainsCredentials:
      return String::Format(
          "Redirect location '%s' contains a username and password, which is "
          "disallowed for cross-origin requests.",
          redirect_url.GetString().Utf8().data());
  }
  NOTREACHED();
  return "";
}

void ExtractCorsExposedHeaderNamesList(const WebURLResponse& response,
                                       WebHTTPHeaderSet& header_set) {
  // If a response was fetched via a service worker, it will always have
  // CorsExposedHeaderNames set, either from the Access-Control-Expose-Headers
  // header, or explicitly via foreign fetch. For requests that didn't come from
  // a service worker, foreign fetch doesn't apply so just parse the CORS
  // header.
  if (response.WasFetchedViaServiceWorker()) {
    for (const auto& header : response.CorsExposedHeaderNames())
      header_set.emplace(header.Ascii().data(), header.Ascii().length());
    return;
  }
  ParseAccessControlExposeHeadersAllowList(
      response.HttpHeaderField(
          WebString(HTTPNames::Access_Control_Expose_Headers)),
      header_set);
}

void ParseAccessControlExposeHeadersAllowList(const WebString& header_value,
                                              WebHTTPHeaderSet& header_set) {
  HTTPHeaderNameListParser parser(header_value);
  parser.Parse(header_set);
}

bool IsOnAccessControlResponseHeaderWhitelist(const WebString& name) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      WebHTTPHeaderSet, allowed_cross_origin_response_headers,
      ({
          "cache-control", "content-language", "content-type", "expires",
          "last-modified", "pragma",
      }));
  return allowed_cross_origin_response_headers.find(name.Ascii().data()) !=
         allowed_cross_origin_response_headers.end();
}

WebString ListOfCORSEnabledURLSchemes() {
  return SchemeRegistry::ListOfCORSEnabledURLSchemes();
}

// https://fetch.spec.whatwg.org/#cors-safelisted-method
bool IsCORSSafelistedMethod(const WebString& method) {
  return FetchUtils::IsCORSSafelistedMethod(method);
}

bool ContainsOnlyCORSSafelistedOrForbiddenHeaders(const WebHTTPHeaderMap& map) {
  return FetchUtils::ContainsOnlyCORSSafelistedOrForbiddenHeaders(
      map.GetHTTPHeaderMap());
}

bool IsCORSEnabledRequestMode(WebURLRequest::FetchRequestMode mode) {
  return mode == WebURLRequest::kFetchRequestModeCORS ||
         mode == WebURLRequest::kFetchRequestModeCORSWithForcedPreflight;
}

// No-CORS requests are allowed for all these contexts, and plugin contexts with
// private permission when we set ServiceWorkerMode to None in
// PepperURLLoaderHost.
bool IsNoCORSAllowedContext(
    WebURLRequest::RequestContext context,
    WebURLRequest::ServiceWorkerMode service_worker_mode) {
  switch (context) {
    case WebURLRequest::kRequestContextAudio:
    case WebURLRequest::kRequestContextFavicon:
    case WebURLRequest::kRequestContextFetch:
    case WebURLRequest::kRequestContextImage:
    case WebURLRequest::kRequestContextObject:
    case WebURLRequest::kRequestContextScript:
    case WebURLRequest::kRequestContextSharedWorker:
    case WebURLRequest::kRequestContextVideo:
    case WebURLRequest::kRequestContextWorker:
      return true;
    case WebURLRequest::kRequestContextPlugin:
      return service_worker_mode == WebURLRequest::ServiceWorkerMode::kNone;
    default:
      return false;
  }
}

}  // namespace WebCORS

}  // namespace blink
