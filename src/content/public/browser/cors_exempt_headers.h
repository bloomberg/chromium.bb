// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CORS_EXEMPT_HEADERS_H_
#define CONTENT_PUBLIC_BROWSER_CORS_EXEMPT_HEADERS_H_

#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

// Updates |cors_exempt_header_list| field of the given |param| to register
// headers that are used in content for special purpose and should not be
// blocked by CORS checks.
CONTENT_EXPORT void UpdateCorsExemptHeader(
    network::mojom::NetworkContextParams* params);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CORS_EXEMPT_HEADERS_H_
