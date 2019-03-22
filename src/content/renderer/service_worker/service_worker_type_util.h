// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_UTIL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_UTIL_H_

#include "content/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace blink {
class WebServiceWorkerRequest;
class WebServiceWorkerResponse;
}

namespace content {

void CONTENT_EXPORT GetServiceWorkerHeaderMapFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request,
    ServiceWorkerHeaderMap* headers);

blink::mojom::FetchAPIResponsePtr GetFetchAPIResponseFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response);

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_UTIL_H_
