// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/InspectorServiceWorkerCacheAgent.h"

#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorTypeBuilder.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebServiceWorkerCache.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "public/platform/WebServiceWorkerCacheStorage.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

#include <algorithm>

using blink::TypeBuilder::Array;
using blink::TypeBuilder::ServiceWorkerCache::DataEntry;

typedef blink::InspectorBackendDispatcher::ServiceWorkerCacheCommandHandler::DeleteCacheCallback DeleteCacheCallback;
typedef blink::InspectorBackendDispatcher::ServiceWorkerCacheCommandHandler::RequestCacheNamesCallback RequestCacheNamesCallback;
typedef blink::InspectorBackendDispatcher::ServiceWorkerCacheCommandHandler::RequestEntriesCallback RequestEntriesCallback;
typedef blink::InspectorBackendDispatcher::CallbackBase RequestCallback;

namespace blink {

namespace {

WebServiceWorkerCacheStorage* assertCacheStorage(ErrorString* errorString, ServiceWorkerGlobalScope* globalScope)
{
    ServiceWorkerGlobalScopeClient* client = ServiceWorkerGlobalScopeClient::from(globalScope);
    if (!client) {
        *errorString = "Could not find service worker global scope.";
        return nullptr;
    }
    WebServiceWorkerCacheStorage* cache = client->cacheStorage();
    if (!cache) {
        *errorString = "Cache not available on service worker global client.";
        return nullptr;
    }
    return cache;
}

CString serviceWorkerCacheErrorString(WebServiceWorkerCacheError* error)
{
    switch (*error) {
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
    RequestCacheNames(PassRefPtrWillBeRawPtr<RequestCacheNamesCallback> callback)
        : m_callback(callback)
    {
    }

    virtual ~RequestCacheNames() { }

    void onSuccess(WebVector<WebString>* caches)
    {
        RefPtr<TypeBuilder::Array<String>> array = TypeBuilder::Array<String>::create();
        for (size_t i = 0; i < caches->size(); i++) {
            array->addItem(String((*caches)[i]));
        }
        m_callback->sendSuccess(array);
    }

    void onError(WebServiceWorkerCacheError* error)
    {
        m_callback->sendFailure(String::format("Error requesting cache names: %s", serviceWorkerCacheErrorString(error).data()));
    }

private:
    RefPtrWillBePersistent<RequestCacheNamesCallback> m_callback;
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
    ResponsesAccumulator(int numResponses, const DataRequestParams& params, PassRefPtrWillBeRawPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_numResponsesLeft(numResponses)
        , m_responses(static_cast<size_t>(numResponses))
        , m_callback(callback)
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
        RefPtr<TypeBuilder::Array<DataEntry>> array = TypeBuilder::Array<DataEntry>::create();
        for (const auto& requestResponse : m_responses) {
            RefPtr<DataEntry> entry = DataEntry::create()
                .setRequest(JSONString::create(requestResponse.request)->toJSONString())
                .setResponse(JSONString::create(requestResponse.response)->toJSONString());
            array->addItem(entry);
        }
        m_callback->sendSuccess(array, hasMore);
    }

private:
    DataRequestParams m_params;
    int m_numResponsesLeft;
    Vector<RequestResponse> m_responses;
    RefPtrWillBePersistent<RequestEntriesCallback> m_callback;
};

class GetCacheResponsesForRequestData : public WebServiceWorkerCache::CacheMatchCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheResponsesForRequestData);

public:
    GetCacheResponsesForRequestData(
        const DataRequestParams& params, const WebServiceWorkerRequest& request,
        PassRefPtr<ResponsesAccumulator> accum, PassRefPtrWillBeRawPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_request(request)
        , m_accumulator(accum)
        , m_callback(callback)
    {
    }
    virtual ~GetCacheResponsesForRequestData() { }

    void onSuccess(WebServiceWorkerResponse* response)
    {
        m_accumulator->addRequestResponsePair(m_request, *response);
    }

    void onError(WebServiceWorkerCacheError* error)
    {
        m_callback->sendFailure(String::format("Error requesting responses for cache  %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    WebServiceWorkerRequest m_request;
    RefPtr<ResponsesAccumulator> m_accumulator;
    RefPtrWillBePersistent<RequestEntriesCallback> m_callback;
};

class GetCacheKeysForRequestData : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheKeysForRequestData);

public:
    GetCacheKeysForRequestData(const DataRequestParams& params, PassOwnPtr<WebServiceWorkerCache> cache, PassRefPtrWillBeRawPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_cache(cache)
        , m_callback(callback)
    {
    }
    virtual ~GetCacheKeysForRequestData() { }

    void onSuccess(WebVector<WebServiceWorkerRequest>* requests)
    {
        RefPtr<ResponsesAccumulator> accumulator = adoptRef(new ResponsesAccumulator(requests->size(), m_params, m_callback));

        for (size_t i = 0; i < requests->size(); i++) {
            const auto& request = (*requests)[i];
            auto* cacheRequest = new GetCacheResponsesForRequestData(m_params, request, accumulator, m_callback);
            m_cache->dispatchMatch(cacheRequest, request, WebServiceWorkerCache::QueryParams());
        }
    }

    void onError(WebServiceWorkerCacheError* error)
    {
        m_callback->sendFailure(String::format("Error requesting requests for cache %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    OwnPtr<WebServiceWorkerCache> m_cache;
    RefPtrWillBePersistent<RequestEntriesCallback> m_callback;
};

class GetCacheForRequestData
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
    WTF_MAKE_NONCOPYABLE(GetCacheForRequestData);

public:
    GetCacheForRequestData(const DataRequestParams& params, PassRefPtrWillBeRawPtr<RequestEntriesCallback> callback)
        : m_params(params)
        , m_callback(callback)
    {
    }
    virtual ~GetCacheForRequestData() { }

    void onSuccess(WebServiceWorkerCache* cache)
    {
        auto* cacheRequest = new GetCacheKeysForRequestData(m_params, adoptPtr(cache), m_callback);
        cache->dispatchKeys(cacheRequest, nullptr, WebServiceWorkerCache::QueryParams());
    }

    void onError(WebServiceWorkerCacheError* error)
    {
        m_callback->sendFailure(String::format("Error requesting cache %s: %s", m_params.cacheName.utf8().data(), serviceWorkerCacheErrorString(error).data()));
    }

private:
    DataRequestParams m_params;
    RefPtrWillBePersistent<RequestEntriesCallback> m_callback;
};

class DeleteCache : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
    WTF_MAKE_NONCOPYABLE(DeleteCache);

public:
    DeleteCache(PassRefPtrWillBeRawPtr<DeleteCacheCallback> callback)
        : m_callback(callback)
    {
    }
    virtual ~DeleteCache() { }

    void onSuccess()
    {
        m_callback->sendSuccess();
    }

    void onError(WebServiceWorkerCacheError* error)
    {
        m_callback->sendFailure(String::format("Error requesting cache names: %s", serviceWorkerCacheErrorString(error).data()));
    }

private:
    RefPtrWillBePersistent<DeleteCacheCallback> m_callback;
};

} // namespace

InspectorServiceWorkerCacheAgent::InspectorServiceWorkerCacheAgent(ServiceWorkerGlobalScope* scope)
    : InspectorBaseAgent<blink::InspectorServiceWorkerCacheAgent, InspectorFrontend::ServiceWorkerCache>("ServiceWorkerCache")
    , m_globalScope(scope)
{
}

InspectorServiceWorkerCacheAgent::~InspectorServiceWorkerCacheAgent() { }

DEFINE_TRACE(InspectorServiceWorkerCacheAgent)
{
    InspectorBaseAgent::trace(visitor);
}

void InspectorServiceWorkerCacheAgent::requestCacheNames(ErrorString* errorString, PassRefPtrWillBeRawPtr<RequestCacheNamesCallback> callback)
{
    WebServiceWorkerCacheStorage* cache = assertCacheStorage(errorString, m_globalScope);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    cache->dispatchKeys(new RequestCacheNames(callback));
}


void InspectorServiceWorkerCacheAgent::requestEntries(ErrorString* errorString, const String& cacheName, int skipCount, int pageSize, PassRefPtrWillBeRawPtr<RequestEntriesCallback> callback)
{
    WebServiceWorkerCacheStorage* cache = assertCacheStorage(errorString, m_globalScope);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    DataRequestParams params;
    params.cacheName = cacheName;
    params.pageSize = pageSize;
    params.skipCount = skipCount;
    cache->dispatchOpen(new GetCacheForRequestData(params, callback), WebString(cacheName));
}

void InspectorServiceWorkerCacheAgent::deleteCache(ErrorString* errorString, const String& cacheName, PassRefPtrWillBeRawPtr<DeleteCacheCallback> callback)
{
    WebServiceWorkerCacheStorage* cache = assertCacheStorage(errorString, m_globalScope);
    if (!cache) {
        callback->sendFailure(*errorString);
        return;
    }
    cache->dispatchDelete(new DeleteCache(callback), WebString(cacheName));
}

} // namespace blink
