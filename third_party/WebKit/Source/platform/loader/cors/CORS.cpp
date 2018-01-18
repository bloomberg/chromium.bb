// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/cors/CORS.h"

#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/http_names.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/AtomicString.h"
#include "services/network/public/cpp/cors/cors.h"
#include "url/gurl.h"

namespace blink {

namespace {

bool IsInterestingStatusCode(int status_code) {
  // Predicate that gates what status codes should be included in console error
  // messages for responses containing no access control headers.
  return status_code >= 400;
}

base::Optional<std::string> GetHeaderValue(const HTTPHeaderMap& header_map,
                                           const AtomicString& header_name) {
  if (header_map.Contains(header_name)) {
    const AtomicString& atomic_value = header_map.Get(header_name);
    CString string_value = atomic_value.GetString().Utf8();
    return std::string(string_value.data(), string_value.length());
  }
  return base::nullopt;
}

}  // namespace

namespace CORS {

String GetErrorString(const network::mojom::CORSError error,
                      const KURL& request_url,
                      const KURL& redirect_url,
                      const int response_status_code,
                      const HTTPHeaderMap& response_header,
                      const SecurityOrigin& origin,
                      const WebURLRequest::RequestContext context) {
  static const char kNoCorsInformation[] =
      " Have the server send the header with a valid value, or, if an opaque "
      "response serves your needs, set the request's mode to 'no-cors' to "
      "fetch the resource with CORS disabled.";

  String redirect_denied =
      redirect_url.IsValid()
          ? String::Format(
                "Redirect from '%s' to '%s' has been blocked by CORS policy: ",
                request_url.GetString().Utf8().data(),
                redirect_url.GetString().Utf8().data())
          : String();

  switch (error) {
    case network::mojom::CORSError::kDisallowedByMode:
      return String::Format(
          "Failed to load '%s': Cross origin requests are not allowed by "
          "request mode.",
          request_url.GetString().Utf8().data());
    case network::mojom::CORSError::kInvalidResponse:
      return String::Format(
          "%sInvalid response. Origin '%s' is therefore not allowed access.",
          redirect_denied.Utf8().data(), origin.ToString().Utf8().data());
    case network::mojom::CORSError::kSubOriginMismatch:
      return String::Format(
          "%sThe 'Access-Control-Allow-Suborigin' header has a value '%s' that "
          "is not equal to the supplied suborigin. Origin '%s' is therefore "
          "not allowed access.",
          redirect_denied.Utf8().data(),
          response_header.Get(HTTPNames::Access_Control_Allow_Suborigin)
              .Utf8()
              .data(),
          origin.ToString().Utf8().data());
    case network::mojom::CORSError::kWildcardOriginNotAllowed:
      return String::Format(
          "%sThe value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'. Origin '%s' is therefore not allowed "
          "access.%s",
          redirect_denied.Utf8().data(), origin.ToString().Utf8().data(),
          context == WebURLRequest::kRequestContextXMLHttpRequest
              ? " The credentials mode of requests initiated by the "
                "XMLHttpRequest is controlled by the withCredentials attribute."
              : "");
    case network::mojom::CORSError::kMissingAllowOriginHeader:
      return String::Format(
          "%sNo 'Access-Control-Allow-Origin' header is present on the "
          "requested resource. Origin '%s' is therefore not allowed access."
          "%s%s",
          redirect_denied.Utf8().data(), origin.ToString().Utf8().data(),
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
    case network::mojom::CORSError::kMultipleAllowOriginValues:
      return String::Format(
          "%sThe 'Access-Control-Allow-Origin' header contains multiple values "
          "'%s', but only one is allowed. Origin '%s' is therefore not allowed "
          "access.%s",
          redirect_denied.Utf8().data(),
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin.ToString().Utf8().data(),
          context == WebURLRequest::kRequestContextFetch ? kNoCorsInformation
                                                         : "");
    case network::mojom::CORSError::kInvalidAllowOriginValue:
      return String::Format(
          "%sThe 'Access-Control-Allow-Origin' header contains the invalid "
          "value '%s'. Origin '%s' is therefore not allowed access.%s",
          redirect_denied.Utf8().data(),
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin.ToString().Utf8().data(),
          context == WebURLRequest::kRequestContextFetch ? kNoCorsInformation
                                                         : "");
    case network::mojom::CORSError::kAllowOriginMismatch:
      return String::Format(
          "%sThe 'Access-Control-Allow-Origin' header has a value '%s' that is "
          "not equal to the supplied origin. Origin '%s' is therefore not "
          "allowed access.%s",
          redirect_denied.Utf8().data(),
          response_header.Get(HTTPNames::Access_Control_Allow_Origin)
              .Utf8()
              .data(),
          origin.ToString().Utf8().data(),
          context == WebURLRequest::kRequestContextFetch ? kNoCorsInformation
                                                         : "");
    case network::mojom::CORSError::kDisallowCredentialsNotSetToTrue:
      return String::Format(
          "%sThe value of the 'Access-Control-Allow-Credentials' header in "
          "the response is '%s' which must be 'true' when the request's "
          "credentials mode is 'include'. Origin '%s' is therefore not allowed "
          "access.%s",
          redirect_denied.Utf8().data(),
          response_header.Get(HTTPNames::Access_Control_Allow_Credentials)
              .Utf8()
              .data(),
          origin.ToString().Utf8().data(),
          (context == WebURLRequest::kRequestContextXMLHttpRequest
               ? " The credentials mode of requests initiated by the "
                 "XMLHttpRequest is controlled by the withCredentials "
                 "attribute."
               : ""));
    case network::mojom::CORSError::kPreflightInvalidStatus:
      return String::Format(
          "Response for preflight has invalid HTTP status code %d.",
          response_status_code);
    case network::mojom::CORSError::kPreflightMissingAllowExternal:
      return WebString(
          "No 'Access-Control-Allow-External' header was present in the "
          "preflight response for this external request (This is an "
          "experimental header which is defined in "
          "'https://wicg.github.io/cors-rfc1918/').");
    case network::mojom::CORSError::kPreflightInvalidAllowExternal:
      return String::Format(
          "The 'Access-Control-Allow-External' header in the preflight "
          "response for this external request had a value of '%s',  not 'true' "
          "(This is an experimental header which is defined in "
          "'https://wicg.github.io/cors-rfc1918/').",
          response_header.Get(HTTPNames::Access_Control_Allow_External)
              .Utf8()
              .data());
    case network::mojom::CORSError::kRedirectDisallowedScheme:
      return String::Format(
          "%sRedirect location '%s' has a disallowed scheme for cross-origin "
          "requests.",
          redirect_denied.Utf8().data(),
          redirect_url.GetString().Utf8().data());
    case network::mojom::CORSError::kRedirectContainsCredentials:
      return String::Format(
          "%sRedirect location '%s' contains a username and password, which is "
          "disallowed for cross-origin requests.",
          redirect_denied.Utf8().data(),
          redirect_url.GetString().Utf8().data());
  }
  NOTREACHED();
  return WebString();
}

WTF::Optional<network::mojom::CORSError> CheckAccess(
    const KURL& response_url,
    const int response_status_code,
    const HTTPHeaderMap& response_header,
    network::mojom::FetchCredentialsMode credentials_mode,
    const SecurityOrigin& origin) {
  return network::cors::CheckAccess(
      response_url, response_status_code,
      GetHeaderValue(response_header, HTTPNames::Access_Control_Allow_Origin),
      GetHeaderValue(response_header,
                     HTTPNames::Access_Control_Allow_Suborigin),
      GetHeaderValue(response_header,
                     HTTPNames::Access_Control_Allow_Credentials),
      credentials_mode, origin.ToUrlOrigin());
}

WTF::Optional<network::mojom::CORSError> CheckRedirectLocation(
    const KURL& url) {
  static const bool run_blink_side_scheme_check =
      !RuntimeEnabledFeatures::OutOfBlinkCORSEnabled();
  // TODO(toyoshim): Deprecate Blink side scheme check when we enable
  // out-of-renderer CORS support. This will need to deprecate Blink APIs that
  // are currently used by an embedder. See https://crbug.com/800669.
  if (run_blink_side_scheme_check &&
      !SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(url.Protocol())) {
    return network::mojom::CORSError::kRedirectDisallowedScheme;
  }
  return network::cors::CheckRedirectLocation(url, run_blink_side_scheme_check);
}

WTF::Optional<network::mojom::CORSError> CheckPreflight(
    const int preflight_response_status_code) {
  return network::cors::CheckPreflight(preflight_response_status_code);
}

WTF::Optional<network::mojom::CORSError> CheckExternalPreflight(
    const HTTPHeaderMap& response_header) {
  return network::cors::CheckExternalPreflight(GetHeaderValue(
      response_header, HTTPNames::Access_Control_Allow_External));
}

bool IsCORSEnabledRequestMode(network::mojom::FetchRequestMode request_mode) {
  return network::cors::IsCORSEnabledRequestMode(request_mode);
}

}  // namespace CORS

}  // namespace blink
