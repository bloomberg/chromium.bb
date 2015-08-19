// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerCache_h
#define WebServiceWorkerCache_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// The Service Worker Cache API. The embedder provides the implementation of the Cache to Blink. Blink uses the interface
// to operate on entries.
// This object is owned by Blink, and should be destroyed as each Cache instance is no longer in use.
class WebServiceWorkerCache {
public:
    class CacheMatchCallbacks : public WebCallbacks<const WebServiceWorkerResponse&, WebServiceWorkerCacheError> {
    public:
        void onSuccess(WebServiceWorkerResponse* r) { onSuccess(*r); }
        void onError(WebServiceWorkerCacheError* e)
        {
            onError(*e);
            delete e;
        }
        void onSuccess(const WebServiceWorkerResponse&) override {}
        void onError(WebServiceWorkerCacheError) override {}
    };
    class CacheWithResponsesCallbacks : public WebCallbacks<const WebVector<WebServiceWorkerResponse>&, WebServiceWorkerCacheError> {
    public:
        void onSuccess(WebVector<WebServiceWorkerResponse>* r) { onSuccess(*r); }
        void onError(WebServiceWorkerCacheError* e)
        {
            onError(*e);
            delete e;
        }
        void onSuccess(const WebVector<WebServiceWorkerResponse>&) override {}
        void onError(WebServiceWorkerCacheError) override {}
    };
    class CacheWithRequestsCallbacks : public WebCallbacks<const WebVector<WebServiceWorkerRequest>&, WebServiceWorkerCacheError> {
    public:
        void onSuccess(WebVector<WebServiceWorkerRequest>* r) { onSuccess(*r); }
        void onError(WebServiceWorkerCacheError* e)
        {
            onError(*e);
            delete e;
        }
        void onSuccess(const WebVector<WebServiceWorkerRequest>&) override {}
        void onError(WebServiceWorkerCacheError) override {}
    };
    using CacheBatchCallbacks = WebCallbacks<void, WebServiceWorkerCacheError>;

    virtual ~WebServiceWorkerCache() { }

    // Options that affect the scope of searches.
    struct QueryParams {
        QueryParams() : ignoreSearch(false), ignoreMethod(false), ignoreVary(false) { }

        bool ignoreSearch;
        bool ignoreMethod;
        bool ignoreVary;
        WebString cacheName;
    };

    enum OperationType {
        OperationTypeUndefined,
        OperationTypePut,
        OperationTypeDelete,
        OperationTypeLast = OperationTypeDelete
    };

    struct BatchOperation {
        BatchOperation() : operationType(OperationTypeUndefined) { }

        OperationType operationType;
        WebServiceWorkerRequest request;
        WebServiceWorkerResponse response;
        QueryParams matchParams;
    };

    WebServiceWorkerCache() { }

    // Ownership of the Cache*Callbacks methods passes to the WebServiceWorkerCache instance, which will delete it after
    // calling onSuccess or onFailure.
    virtual void dispatchMatch(CacheMatchCallbacks*, const WebServiceWorkerRequest&, const QueryParams&) = 0;
    virtual void dispatchMatchAll(CacheWithResponsesCallbacks*, const WebServiceWorkerRequest&, const QueryParams&) = 0;
    virtual void dispatchKeys(CacheWithRequestsCallbacks*, const WebServiceWorkerRequest*, const QueryParams&) = 0;
    virtual void dispatchBatch(CacheBatchCallbacks*, const WebVector<BatchOperation>&) = 0;
};

} // namespace blink

#endif // WebServiceWorkerCache_h
