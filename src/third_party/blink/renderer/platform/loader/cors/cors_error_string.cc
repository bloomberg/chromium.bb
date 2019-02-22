// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"

#include <initializer_list>

#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace CORS {

namespace {

void Append(StringBuilder& builder, std::initializer_list<StringView> views) {
  for (const StringView& view : views) {
    builder.Append(view);
  }
}

bool IsPreflightError(network::mojom::CORSError error_code) {
  switch (error_code) {
    case network::mojom::CORSError::kPreflightWildcardOriginNotAllowed:
    case network::mojom::CORSError::kPreflightMissingAllowOriginHeader:
    case network::mojom::CORSError::kPreflightMultipleAllowOriginValues:
    case network::mojom::CORSError::kPreflightInvalidAllowOriginValue:
    case network::mojom::CORSError::kPreflightAllowOriginMismatch:
    case network::mojom::CORSError::kPreflightInvalidAllowCredentials:
    case network::mojom::CORSError::kPreflightInvalidStatus:
    case network::mojom::CORSError::kPreflightDisallowedRedirect:
    case network::mojom::CORSError::kPreflightMissingAllowExternal:
    case network::mojom::CORSError::kPreflightInvalidAllowExternal:
      return true;
    default:
      return false;
  }
}

}  // namespace

String GetErrorString(const network::CORSErrorStatus& status,
                      const KURL& initial_request_url,
                      const KURL& last_request_url,
                      const SecurityOrigin& origin,
                      ResourceType resource_type,
                      const AtomicString& initiator_name) {
  StringBuilder builder;
  static constexpr char kNoCorsInformation[] =
      " Have the server send the header with a valid value, or, if an opaque "
      "response serves your needs, set the request's mode to 'no-cors' to "
      "fetch the resource with CORS disabled.";

  using CORSError = network::mojom::CORSError;
  const StringView hint(status.failed_parameter.data(),
                        status.failed_parameter.size());

  const char* resource_kind_raw =
      Resource::ResourceTypeToString(resource_type, initiator_name);
  String resource_kind(resource_kind_raw);
  if (strlen(resource_kind_raw) >= 2 && IsASCIILower(resource_kind_raw[1]))
    resource_kind = resource_kind.LowerASCII();

  Append(builder, {"Access to ", resource_kind, " at '",
                   last_request_url.GetString(), "' "});
  if (initial_request_url != last_request_url) {
    Append(builder,
           {"(redirected from '", initial_request_url.GetString(), "') "});
  }
  Append(builder, {"from origin '", origin.ToString(),
                   "' has been blocked by CORS policy: "});

  if (IsPreflightError(status.cors_error)) {
    builder.Append(
        "Response to preflight request doesn't pass access control check: ");
  }

  switch (status.cors_error) {
    case CORSError::kDisallowedByMode:
      builder.Append("Cross origin requests are not allowed by request mode.");
      break;
    case CORSError::kInvalidResponse:
      builder.Append("The response is invalid.");
      break;
    case CORSError::kWildcardOriginNotAllowed:
    case CORSError::kPreflightWildcardOriginNotAllowed:
      builder.Append(
          "The value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'.");
      if (initiator_name == FetchInitiatorTypeNames::xmlhttprequest) {
        builder.Append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials attribute.");
      }
      break;
    case CORSError::kMissingAllowOriginHeader:
    case CORSError::kPreflightMissingAllowOriginHeader:
      builder.Append(
          "No 'Access-Control-Allow-Origin' header is present on the "
          "requested resource.");
      if (initiator_name == FetchInitiatorTypeNames::fetch) {
        builder.Append(
            " If an opaque response serves your needs, set the request's "
            "mode to 'no-cors' to fetch the resource with CORS disabled.");
      }
      break;
    case CORSError::kMultipleAllowOriginValues:
    case CORSError::kPreflightMultipleAllowOriginValues:
      Append(builder,
             {"The 'Access-Control-Allow-Origin' header contains multiple "
              "values '",
              hint, "', but only one is allowed."});
      if (initiator_name == FetchInitiatorTypeNames::fetch)
        builder.Append(kNoCorsInformation);
      break;
    case CORSError::kInvalidAllowOriginValue:
    case CORSError::kPreflightInvalidAllowOriginValue:
      Append(builder, {"The 'Access-Control-Allow-Origin' header contains the "
                       "invalid value '",
                       hint, "'."});
      if (initiator_name == FetchInitiatorTypeNames::fetch)
        builder.Append(kNoCorsInformation);
      break;
    case CORSError::kAllowOriginMismatch:
    case CORSError::kPreflightAllowOriginMismatch:
      Append(builder, {"The 'Access-Control-Allow-Origin' header has a value '",
                       hint, "' that is not equal to the supplied origin."});
      if (initiator_name == FetchInitiatorTypeNames::fetch)
        builder.Append(kNoCorsInformation);
      break;
    case CORSError::kInvalidAllowCredentials:
    case CORSError::kPreflightInvalidAllowCredentials:
      Append(builder,
             {"The value of the 'Access-Control-Allow-Credentials' header in "
              "the response is '",
              hint,
              "' which must be 'true' when the request's credentials mode is "
              "'include'."});
      if (initiator_name == FetchInitiatorTypeNames::xmlhttprequest) {
        builder.Append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials "
            "attribute.");
      }
      break;
    case CORSError::kCORSDisabledScheme:
      Append(builder,
             {"Cross origin requests are only supported for protocol schemes: ",
              SchemeRegistry::ListOfCORSEnabledURLSchemes(), "."});
      break;
    case CORSError::kPreflightInvalidStatus:
      builder.Append("It does not have HTTP ok status.");
      break;
    case CORSError::kPreflightDisallowedRedirect:
      builder.Append("Redirect is not allowed for a preflight request.");
      break;
    case CORSError::kPreflightMissingAllowExternal:
      builder.Append(
          "No 'Access-Control-Allow-External' header was present in the "
          "preflight response for this external request (This is an "
          "experimental header which is defined in "
          "'https://wicg.github.io/cors-rfc1918/').");
      break;
    case CORSError::kPreflightInvalidAllowExternal:
      Append(builder,
             {"The 'Access-Control-Allow-External' header in the preflight "
              "response for this external request had a value of '",
              hint,
              "',  not 'true' (This is an experimental header which is defined "
              "in 'https://wicg.github.io/cors-rfc1918/')."});
      break;
    case CORSError::kInvalidAllowMethodsPreflightResponse:
      builder.Append(
          "Cannot parse Access-Control-Allow-Methods response header field in "
          "preflight response.");
      break;
    case CORSError::kInvalidAllowHeadersPreflightResponse:
      builder.Append(
          "Cannot parse Access-Control-Allow-Headers response header field in "
          "preflight response.");
      break;
    case CORSError::kMethodDisallowedByPreflightResponse:
      Append(builder, {"Method ", hint,
                       " is not allowed by Access-Control-Allow-Methods in "
                       "preflight response."});
      break;
    case CORSError::kHeaderDisallowedByPreflightResponse:
      Append(builder, {"Request header field ", hint,
                       " is not allowed by "
                       "Access-Control-Allow-Headers in preflight response."});
      break;
    case CORSError::kRedirectContainsCredentials:
      Append(builder, {"Redirect location '", hint,
                       "' contains a username and password, which is "
                       "disallowed for cross-origin requests."});
      break;
  }
  return builder.ToString();
}

}  // namespace CORS

}  // namespace blink
