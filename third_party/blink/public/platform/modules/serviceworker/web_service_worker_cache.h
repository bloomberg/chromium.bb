// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICEWORKER_WEB_SERVICE_WORKER_CACHE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICEWORKER_WEB_SERVICE_WORKER_CACHE_H_

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom-shared.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_request.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_response.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// The Service Worker Cache API. The embedder provides the implementation of the
// Cache to Blink. Blink uses the interface to operate on entries. This object
// is owned by Blink, and should be destroyed as each Cache instance is no
// longer in use.
class WebServiceWorkerCache {
 public:
  using CacheMatchCallbacks = WebCallbacks<const WebServiceWorkerResponse&,
                                           blink::mojom::CacheStorageError>;
  using CacheWithResponsesCallbacks =
      WebCallbacks<const WebVector<WebServiceWorkerResponse>&,
                   blink::mojom::CacheStorageError>;
  using CacheWithRequestsCallbacks =
      WebCallbacks<const WebVector<WebServiceWorkerRequest>&,
                   blink::mojom::CacheStorageError>;
  using CacheBatchCallbacks =
      WebCallbacks<void, blink::mojom::CacheStorageError>;

  virtual ~WebServiceWorkerCache() = default;

  // Options that affect the scope of searches.
  struct QueryParams {
    QueryParams()
        : ignore_search(false), ignore_method(false), ignore_vary(false) {}

    bool ignore_search;
    bool ignore_method;
    bool ignore_vary;
    WebString cache_name;
  };

  enum OperationType {
    kOperationTypeUndefined,
    kOperationTypePut,
    kOperationTypeDelete,
    kOperationTypeLast = kOperationTypeDelete
  };

  struct BatchOperation {
    BatchOperation() : operation_type(kOperationTypeUndefined) {}

    OperationType operation_type;
    WebServiceWorkerRequest request;
    WebServiceWorkerResponse response;
    QueryParams match_params;
  };

  WebServiceWorkerCache() = default;

  // Ownership of the Cache*Callbacks methods passes to the
  // WebServiceWorkerCache instance, which will delete it after calling
  // onSuccess or onFailure.
  virtual void DispatchMatch(std::unique_ptr<CacheMatchCallbacks>,
                             const WebServiceWorkerRequest&,
                             const QueryParams&) = 0;
  virtual void DispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks>,
                                const WebServiceWorkerRequest&,
                                const QueryParams&) = 0;
  virtual void DispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks>,
                            const WebServiceWorkerRequest&,
                            const QueryParams&) = 0;
  virtual void DispatchBatch(std::unique_ptr<CacheBatchCallbacks>,
                             const WebVector<BatchOperation>&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICEWORKER_WEB_SERVICE_WORKER_CACHE_H_
