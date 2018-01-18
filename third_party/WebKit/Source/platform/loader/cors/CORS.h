// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORS_h
#define CORS_h

#include <string>

#include "platform/PlatformExport.h"
#include "platform/wtf/Optional.h"
#include "services/network/public/interfaces/cors.mojom-shared.h"
#include "services/network/public/interfaces/fetch_api.mojom-shared.h"

namespace blink {

class HTTPHeaderMap;
class KURL;
class SecurityOrigin;

// Thin wrapper functions to call ::network::cors functions from Blink core.
namespace CORS {

PLATFORM_EXPORT WTF::Optional<network::mojom::CORSError> CheckAccess(
    const KURL&,
    const int response_status_code,
    const HTTPHeaderMap&,
    network::mojom::FetchCredentialsMode,
    const SecurityOrigin&);

PLATFORM_EXPORT bool IsCORSEnabledRequestMode(network::mojom::FetchRequestMode);

}  // namespace CORS

}  // namespace blink

#endif  // CORS_h
