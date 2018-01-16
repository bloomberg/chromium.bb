// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_

#include <string>

#include "base/optional.h"
#include "services/network/public/interfaces/cors.mojom-shared.h"
#include "services/network/public/interfaces/fetch_api.mojom-shared.h"

class GURL;
namespace url {
class Origin;
}  // namespace url

namespace network {

namespace cors {

namespace header_names {

extern const char kAccessControlAllowCredentials[];
extern const char kAccessControlAllowOrigin[];
extern const char kAccessControlAllowSuborigin[];

}  // namespace header_names

// Performs a CORS access check on the response parameters.
base::Optional<mojom::CORSError> CheckAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_suborigin_header,
    const base::Optional<std::string>& allow_credentials_header,
    network::mojom::FetchCredentialsMode credentials_mode,
    const url::Origin& origin);

bool IsCORSEnabledRequestMode(mojom::FetchRequestMode mode);

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
