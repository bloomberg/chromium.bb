// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

#include <algorithm>
#include <memory>

using blink::protocol::Array;
using blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::DataEntry;

typedef blink::protocol::Backend::CacheStorage::DeleteCacheCallback DeleteCacheCallback;
typedef blink::protocol::Backend::CacheStorage::DeleteEntryCallback DeleteEntryCallback;
typedef blink::protocol::Backend::CacheStorage::RequestCacheNamesCallback RequestCacheNamesCallback;
typedef blink::protocol::Backend::CacheStorage::RequestEntriesCallback RequestEntriesCallback;
typedef blink::protocol::Dispatcher::CallbackBase RequestCallback;
typedef blink::WebServiceWorkerCache::BatchOperation BatchOperation;

namespace blink {

namespace {

String buildCacheId(const String& securityOrigin, const String& cacheName)
{
    String id(securityOrigin);
    id.append("|");
    id.append(cacheName);
    return id;
}

bool parseCacheId(ErrorString* errorString, const String& id, String* securityOrigin, String* cacheName)
{
    size_t pipe = id.find('|');
    if (pipe == WTF::kNotFound) {
        *errorString = "Invalid cache id.";
        return false;
    }
    *securityOrigin = id.substring(0, pipe);
    *cacheName = id.substring(pipe + 1);
    return true;
}

PassOwnPtr<WebServiceWorkerCacheStorage> assertCacheStorage(ErrorString* errorString, const String& securityOrigin)
{
    RefPtr<SecurityOrigin> secOrigin = SecurityOrigin::createFromString(securityOrigin);

    // Cache Storage API is restricted to trustworthy origins.
    if (!secOrigin->isPotentiallyTrustworthy()) {
        *errorString = secOrigin->isPotentiallyTrustworthyErrorMessage();
        return nullptr;
    }

    OwnPtr<WebServiceWorkerCacheStorage> cache = adoptPtr(Platform::current()->cacheStorage(WebSecurityOrigin(secOrigin)));
    if (!cache)
        *errorString = "Could not find cache storage.";
    return cache.release();
}

PassOwnPtr<WebServiceWorkerCacheStorage> assertCacheStorageAndNameForId(ErrorString* errorString, const String& cacheId, String* cacheName)
{
    String securityOrigin;
    if (!parseCacheId(errorString, cacheId, &securityOrigin, cacheName)) {
        return nullptr;
    }
    return assertCacheStorage(errorString, securityOrigin);
}

CString serviceWorkerCacheErrorString(WebServiceWorkerCacheError error)
{
    switch (error) {
    case WebServiceWorkerCacheErrorNotImplemented:
        return CString("not implemented.");
        break;
    case WebServiceWorkerCacheErrorNotFound:
        return CString("not found.");
        break;
    case WebServiceWorkerCacheErrorExists:
        return CString("cache already exists.");
        break;
    default:
        return CString("unknown error.");
        break;
    }
}

class RequestCacheNames
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
    WTF_MAKE_NONCOPYABLE(RequestCacheNames);

public:
    RequestCacheNames(const String& securityOrigin, PassOwnPtr<RequestCacheNamesCallback> callback)
        : m_securityOrigin(securityOrigin)
        , m_callback(std::move(callback))
    {
    }

    ~RequestCacheNames() override { }

    void onSuccess(const WebVector<WebString>& caches) override
    {
        OwnPtr<Array<Cache>> array = Array<Cache>::create();
        for (size_t i = 0; i < caches.size(); i++) {
            String name = String(caches[i]);
            OwnPtr<Cache> entry = Cache::create()
                .setSecurityOrigin(m_securityOrigin)
                .setCacheName(name)
                .setCacheId(buildCacheId(m_securityOrigin, name)).build();
            array->addItem(entry.release());
        }
        m_callback->sendSuccess(array.release());
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting cache names: %s", serviceWorkerCacheErrorString(error).data()));
    }

private:
    String m_securityOrigin;
    OwnPtr<RequestCacheNamesCallback> m_callback;
};

struct DataRequestParams {
    String cacheName;
    int skipCount;
    int pageSize;
};

struct RequestResponse {
    RequestResponse() { }
    RequestResponse(const String& request, const String& response)
        : request(request)
        , response(response)
    {
    }
    String request;
    String response;
};

class ResponsesAccumulator : public RefCounted<ResponsesAccumulator> {
    WTF_MAKE_NONCOPYABLE(ResponsesAccumulator);

public:
    ResponsesAccumulator(int numResponses, const DataRequestParams& params, PassOwnPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_numResponsesLeft(numResponses)
        , m_responses(static_cast<size_t>(numResponses))
        , m_callback(std::move(callback))
    {
    }

    void addRequestResponsePair(const WebServiceWorkerRequest& request, const WebServiceWorkerResponse& response)
    {
        ASSERT(m_numResponsesLeft > 0);
        RequestResponse& requestResponse = m_responses.at(m_responses.size() - m_numResponsesLeft);
        requestResponse.request = request.url().string();
        requestResponse.response = response.statusText();

        if (--m_numResponsesLeft != 0)
            return;

        std::sort(m_responses.begin(), m_responses.end(),
            [](const RequestResponse& a, const RequestResponse& b)
            {
                return WTF::codePointCompareLessThan(a.request, b.request);
            });
        if (m_params.skipCount > 0)
            m_responses.remove(0, m_params.skipCount);
        bool hasMore = false;
        if (static_cast<size_t>(m_params.pageSize) < m_responses.size()) {
            m_responses.remove(m_params.pageSize, m_responses.size() - m_params.pageSize);
            hasMore = true;
        }
        OwnPtr<Array<DataEntry>> array = Array<DataEntry>::create();
        for (const auto& requestResponse : m_responses) {
            OwnPtr<DataEntry> entry = DataEntry::create()
                .setRequest(requestResponse.request)
                .setResponse(requestResponse.response).build();
            array->addItem(entry.release());
        }
        m_callback->sendSuccess(array.release(), hasMore);
    }

    void sendFailure(const String& error)
    {
        m_callback->sendFailure(error);
    }

private:
    DataRequestParams m_params;
    int m_numResponsesLeft;
    Vector<RequestResponse> m_responses;
    OwnPtr<RequestEntriesCallback> m_callback;
};

class GetCacheResponsesForRequestData : public WebServiceWorkerCache::CacheMatchCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheResponsesForRequestData);

public:
    GetCacheResponsesForRequestData(const DataRequestParams& params, const WebServiceWorkerRequest& request, PassRefPtr<ResponsesAccumulator> accum)
        : m_params(params)
        , m_request(request)
        , m_accumulator(accum)
    {
    }
    ~GetCacheResponsesForRequestData() override { }

    void onSuccess(const WebServiceWorkerResponse& response) override
    {
        m_accumulator->addRequestResponsePair(m_request, response);
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_accumulator->sendFailure(String::format("Error requesting responses for cache  %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    WebServiceWorkerRequest m_request;
    RefPtr<ResponsesAccumulator> m_accumulator;
};

class GetCacheKeysForRequestData : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheKeysForRequestData);

public:
    GetCacheKeysForRequestData(const DataRequestParams& params, PassOwnPtr<WebServiceWorkerCache> cache, PassOwnPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_cache(std::move(cache))
        , m_callback(std::move(callback))
    {
    }
    ~GetCacheKeysForRequestData() override { }

    WebServiceWorkerCache* cache() { return m_cache.get(); }
    void onSuccess(const WebVector<WebServiceWorkerRequest>& requests) override
    {
        if (requests.isEmpty()) {
            OwnPtr<Array<DataEntry>> array = Array<DataEntry>::create();
            m_callback->sendSuccess(array.release(), false);
            return;
        }
        RefPtr<ResponsesAccumulator> accumulator = adoptRef(new ResponsesAccumulator(requests.size(), m_params, m_callback.release()));

        for (size_t i = 0; i < requests.size(); i++) {
            const auto& request = requests[i];
            auto* cacheRequest = new GetCacheResponsesForRequestData(m_params, request, accumulator);
            m_cache->dispatchMatch(cacheRequest, request, WebServiceWorkerCache::QueryParams());
        }
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting requests for cache %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    OwnPtr<WebServiceWorkerCache> m_cache;
    OwnPtr<RequestEntriesCallback> m_callback;
};

class GetCacheForRequestData
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheForRequestData);

public:
    GetCacheForRequestData(const DataRequestParams& params, PassOwnPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_callback(std::move(callback))
    {
    }
    ~GetCacheForRequestData() override { }

    void onSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override
    {
        auto* cacheRequest = new GetCacheKeysForRequestData(m_params, adoptPtr(cache.release()), m_callback.release());
        cacheRequest->cache()->dispatchKeys(cacheRequest, nullptr, WebServiceWorkerCache::QueryParams());
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting cache %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    OwnPtr<RequestEntriesCallback> m_callback;
};

class DeleteCache : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
    WTF_MAKE_NONCOPYABLE(DeleteCache);

public:
    DeleteCache(PassOwnPtr<DeleteCacheCallback> callback)
        : m_callback(std::move(callback))
    {
    }
    ~DeleteCache() override { }

    void onSuccess() override
    {
        m_callback->sendSuccess();
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting cache names: %s", serviceWorkerCacheErrorString(error).data()));
    }

private:
    OwnPtr<DeleteCacheCallback> m_callback;
};

class DeleteCacheEntry : public WebServiceWorkerCache::CacheBatchCallbacks {
    WTF_MAKE_NONCOPYABLE(DeleteCacheEntry);
public:

    DeleteCacheEntry(PassOwnPtr<DeleteEntryCallback> callback)
        : m_callback(std::move(callback))
    {
    }
    ~DeleteCacheEntry() override { }

    void onSuccess() override
    {
        m_callback->sendSuccess();
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting cache names: %s", serviceWorkerCacheErrorString(error).data()));
    }

private:
    OwnPtr<DeleteEntryCallback> m_callback;
};

class GetCacheForDeleteEntry
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheForDeleteEntry);

public:
    GetCacheForDeleteEntry(const String& requestSpec, const String& cacheName, PassOwnPtr<DeleteEntryCallback> callback)
        : m_requestSpec(requestSpec)
        , m_cacheName(cacheName)
        , m_callback(std::move(callback))
    {
    }
    ~GetCacheForDeleteEntry() override { }

    void onSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override
    {
        auto* deleteRequest = new DeleteCacheEntry(m_callback.release());
        BatchOperation deleteOperation;
        deleteOperation.operationType = WebServiceWorkerCache::OperationTypeDelete;
        deleteOperation.request.setURL(KURL(ParsedURLString, m_requestSpec));
        Vector<BatchOperation> operations;
        operations.append(deleteOperation);
        cache.release()->dispatchBatch(deleteRequest, WebVector<BatchOperation>(operations));
    }

    void onError(WebServiceWorkerCacheError error) override
    {
        m_callback->sendFailure(String::format("Error requesting cache %s: %s", m_cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    String m_requestSpec;
    String m_cacheName;
    OwnPtr<DeleteEntryCallback> m_callback;
};

} // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent()
    : InspectorBaseAgent<InspectorCacheStorageAgent, protocol::Frontend::CacheStorage>("CacheStorage")
{
}

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() { }

DEFINE_TRACE(InspectorCacheStorageAgent)
{
    InspectorBaseAgent::trace(visitor);
}

void InspectorCacheStorageAgent::requestCacheNames(ErrorString* errorString, const String& securityOrigin, PassOwnPtr<RequestCacheNamesCallback> callback)
{
    RefPtr<SecurityOrigin> secOrigin = SecurityOrigin::createFromString(securityOrigin);

    // Cache Storage API is restricted to trustworthy origins.
    if (!secOrigin->isPotentiallyTrustworthy()) {
        // Don't treat this as an error, just don't attempt to open and enumerate the caches.
        callback->sendSuccess(Array<protocol::CacheStorage::Cache>::create());
        return;
    }

    OwnPtr<WebServiceWorkerCacheStorage> cache = assertCacheStorage(errorString, securityOrigin);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    cache->dispatchKeys(new RequestCacheNames(securityOrigin, std::move(callback)));
}

void InspectorCacheStorageAgent::requestEntries(ErrorString* errorString, const String& cacheId, int skipCount, int pageSize, PassOwnPtr<RequestEntriesCallback> callback)
{
    String cacheName;
    OwnPtr<WebServiceWorkerCacheStorage> cache = assertCacheStorageAndNameForId(errorString, cacheId, &cacheName);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    DataRequestParams params;
    params.cacheName = cacheName;
    params.pageSize = pageSize;
    params.skipCount = skipCount;
    cache->dispatchOpen(new GetCacheForRequestData(params, std::move(callback)), WebString(cacheName));
}

void InspectorCacheStorageAgent::deleteCache(ErrorString* errorString, const String& cacheId, PassOwnPtr<DeleteCacheCallback> callback)
{
    String cacheName;
    OwnPtr<WebServiceWorkerCacheStorage> cache = assertCacheStorageAndNameForId(errorString, cacheId, &cacheName);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    cache->dispatchDelete(new DeleteCache(std::move(callback)), WebString(cacheName));
}

void InspectorCacheStorageAgent::deleteEntry(ErrorString* errorString, const String& cacheId, const String& request, PassOwnPtr<DeleteEntryCallback> callback)
{
    String cacheName;
    OwnPtr<WebServiceWorkerCacheStorage> cache = assertCacheStorageAndNameForId(errorString, cacheId, &cacheName);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    cache->dispatchOpen(new GetCacheForDeleteEntry(request, cacheName, std::move(callback)), WebString(cacheName));
}


} // namespace blink
