// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/cors/CORS.h"

#include <string>

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

bool IsCORSSafelistedMethod(const String& method) {
  DCHECK(!method.IsNull());
  CString utf8_method = method.Utf8();
  return network::cors::IsCORSSafelistedMethod(
      std::string(utf8_method.data(), utf8_method.length()));
}

bool IsCORSSafelistedContentType(const String& media_type) {
  CString utf8_media_type = media_type.Utf8();
  return network::cors::IsCORSSafelistedContentType(
      std::string(utf8_media_type.data(), utf8_media_type.length()));
}

bool IsCORSSafelistedHeader(const String& name, const String& value) {
  DCHECK(!name.IsNull());
  DCHECK(!value.IsNull());
  CString utf8_name = name.Utf8();
  CString utf8_value = value.Utf8();
  return network::cors::IsCORSSafelistedHeader(
      std::string(utf8_name.data(), utf8_name.length()),
      std::string(utf8_value.data(), utf8_value.length()));
}

}  // namespace CORS

}  // namespace blink
