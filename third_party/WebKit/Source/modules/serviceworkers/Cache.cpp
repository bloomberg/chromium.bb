// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Cache.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/Response.h"
#include "public/platform/WebServiceWorkerCache.h"

namespace blink {

namespace {

WebServiceWorkerCache::QueryParams toWebQueryParams(const QueryParams& queryParams)
{
    WebServiceWorkerCache::QueryParams webQueryParams;
    // FIXME: The queryParams.hasXXXX() calls can be removed if defaults are
    // added to the IDL. https://github.com/slightlyoff/ServiceWorker/issues/466
    webQueryParams.ignoreSearch = queryParams.hasIgnoreSearch() && queryParams.ignoreSearch();
    webQueryParams.ignoreMethod = queryParams.hasIgnoreMethod() && queryParams.ignoreMethod();
    webQueryParams.ignoreVary = queryParams.hasIgnoreVary() && queryParams.ignoreVary();
    webQueryParams.prefixMatch = queryParams.hasPrefixMatch() && queryParams.prefixMatch();
    webQueryParams.cacheName = queryParams.cacheName();
    return webQueryParams;
}

// FIXME: Consider using CallbackPromiseAdapter.
class CacheMatchCallbacks : public WebServiceWorkerCache::CacheMatchCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheMatchCallbacks);
public:
    CacheMatchCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebServiceWorkerResponse* webResponse) OVERRIDE
    {
        m_resolver->resolve(Response::create(m_resolver->scriptState()->executionContext(), *webResponse));
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithResponsesCallbacks : public WebServiceWorkerCache::CacheWithResponsesCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheWithResponsesCallbacks);
public:
    CacheWithResponsesCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerResponse>* webResponses) OVERRIDE
    {
        HeapVector<Member<Response> > responses;
        for (size_t i = 0; i < webResponses->size(); ++i)
            responses.append(Response::create(m_resolver->scriptState()->executionContext(), (*webResponses)[i]));
        m_resolver->resolve(responses);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithRequestsCallbacks : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheWithRequestsCallbacks);
public:
    CacheWithRequestsCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerRequest>* webRequests) OVERRIDE
    {
        HeapVector<Member<Request> > requests;
        for (size_t i = 0; i < webRequests->size(); ++i)
            requests.append(Request::create(m_resolver->scriptState()->executionContext(), (*webRequests)[i]));
        m_resolver->resolve(requests);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

ScriptPromise rejectForCacheError(ScriptState* scriptState, WebServiceWorkerCacheError error)
{
    return ScriptPromise::rejectWithDOMException(scriptState, Cache::domExceptionForCacheError(error));
}

ScriptPromise rejectAsNotImplemented(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Cache is not implemented"));
}

} // namespace

Cache* Cache::create(WebServiceWorkerCache* webCache)
{
    return new Cache(webCache);
}

ScriptPromise Cache::match(ScriptState* scriptState, Request* originalRequest, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return matchImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::match(ScriptState* scriptState, const String& requestString, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return matchImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, Request* originalRequest, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return matchAllImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, const String& requestString, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return matchAllImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::add(ScriptState* scriptState, Request* originalRequest)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return addImpl(scriptState, request);
}

ScriptPromise Cache::add(ScriptState* scriptState, const String& requestString)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return addImpl(scriptState, request);
}

ScriptPromise Cache::addAll(ScriptState* scriptState, const Vector<ScriptValue>& rawRequests)
{
    // FIXME: Implement this.
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::deleteFunction(ScriptState* scriptState, Request* originalRequest, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return deleteImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::deleteFunction(ScriptState* scriptState, const String& requestString, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return deleteImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::put(ScriptState* scriptState, Request* originalRequest, Response* response)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return putImpl(scriptState, request, response);
}

ScriptPromise Cache::put(ScriptState* scriptState, const String& requestString, Response* response)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return putImpl(scriptState, request, response);
}

ScriptPromise Cache::keys(ScriptState* scriptState)
{
    return keysImpl(scriptState);
}

ScriptPromise Cache::keys(ScriptState* scriptState, Request* originalRequest, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), originalRequest, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return keysImpl(scriptState, request, queryParams);
}

ScriptPromise Cache::keys(ScriptState* scriptState, const String& requestString, const QueryParams& queryParams)
{
    TrackExceptionState exceptionState;
    Request* request = Request::create(scriptState->executionContext(), requestString, exceptionState);
    if (exceptionState.hadException()) {
        // FIXME: We should throw the caught error.
        return rejectForCacheError(scriptState, WebServiceWorkerCacheErrorNotFound);
    }
    return keysImpl(scriptState, request, queryParams);
}

Cache::Cache(WebServiceWorkerCache* webCache)
    : m_webCache(adoptPtr(webCache)) { }

ScriptPromise Cache::matchImpl(ScriptState* scriptState, Request* request, const QueryParams& queryParams)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchMatch(new CacheMatchCallbacks(resolver), webRequest, toWebQueryParams(queryParams));
    return promise;
}

ScriptPromise Cache::matchAllImpl(ScriptState* scriptState, Request* request, const QueryParams& queryParams)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchMatchAll(new CacheWithResponsesCallbacks(resolver), webRequest, toWebQueryParams(queryParams));
    return promise;
}

ScriptPromise Cache::addImpl(ScriptState* scriptState, Request*)
{
    // FIXME: Implement this.
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::addAllImpl(ScriptState* scriptState, Vector<Request*>)
{
    // FIXME: Implement this.
    return rejectAsNotImplemented(scriptState);
}

PassRefPtrWillBeRawPtr<DOMException> Cache::domExceptionForCacheError(WebServiceWorkerCacheError reason)
{
    switch (reason) {
    case WebServiceWorkerCacheErrorNotImplemented:
        return DOMException::create(NotSupportedError, "Method is not implemented.");
    case WebServiceWorkerCacheErrorNotFound:
        return DOMException::create(NotFoundError, "Entry was not found.");
    case WebServiceWorkerCacheErrorExists:
        return DOMException::create(InvalidAccessError, "Entry already exists.");
    default:
        ASSERT_NOT_REACHED();
        return DOMException::create(NotSupportedError, "Unknown error.");
    }
}

ScriptPromise Cache::deleteImpl(ScriptState* scriptState, Request* request, const QueryParams& queryParams)
{
    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypeDelete;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    batchOperations[0].matchParams = toWebQueryParams(queryParams);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchBatch(new CacheWithResponsesCallbacks(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::putImpl(ScriptState* scriptState, Request* request, Response* response)
{
    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypePut;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    response->populateWebServiceWorkerResponse(batchOperations[0].response);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchBatch(new CacheWithResponsesCallbacks(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, WebServiceWorkerCache::QueryParams());
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState, Request* request, const QueryParams& queryParams)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, toWebQueryParams(queryParams));
    return promise;
}

} // namespace blink
