// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_

#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

class GURL;
namespace url {
class Origin;
}  // namespace url

namespace network {

namespace cors {

namespace header_names {

COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowCredentials[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowOrigin[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestExternal[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestHeaders[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestMethod[];

}  // namespace header_names

// Performs a CORS access check on the response parameters.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<mojom::CORSError> CheckAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    network::mojom::FetchCredentialsMode credentials_mode,
    const url::Origin& origin,
    bool allow_file_origin = false);

// Given a redirected-to URL, checks if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
// TODO(toyoshim): Remove |skip_scheme_check| that is used when customized
// scheme check runs in Blink side in the legacy mode.
// See https://crbug.com/800669.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<mojom::CORSError> CheckRedirectLocation(const GURL& redirect_url,
                                                       bool skip_scheme_check);

// Performs the required CORS checks on the response to a preflight request.
// Returns |kPreflightSuccess| if preflight response was successful.
// TODO(toyoshim): Rename to CheckPreflightStatus.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<mojom::CORSError> CheckPreflight(const int status_code);

// Checks errors for the currently experimental "Access-Control-Allow-External:"
// header. Shares error conditions with standard preflight checking.
// See https://crbug.com/590714.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<mojom::CORSError> CheckExternalPreflight(
    const base::Optional<std::string>& allow_external);

COMPONENT_EXPORT(NETWORK_CPP)
bool IsCORSEnabledRequestMode(mojom::FetchRequestMode mode);

// Checks safelisted request parameters.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCORSSafelistedMethod(const std::string& method);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCORSSafelistedContentType(const std::string& name);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCORSSafelistedHeader(const std::string& name, const std::string& value);

// Checks forbidden header in the fetch spec.
// See https://fetch.spec.whatwg.org/#forbidden-header-name.
COMPONENT_EXPORT(NETWORK_CPP) bool IsForbiddenHeader(const std::string& name);

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
