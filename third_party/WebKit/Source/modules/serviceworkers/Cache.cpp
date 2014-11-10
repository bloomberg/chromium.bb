// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Cache.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/Response.h"
#include "public/platform/WebServiceWorkerCache.h"

namespace blink {

namespace {

WebServiceWorkerCache::QueryParams toWebQueryParams(const CacheQueryOptions& options)
{
    WebServiceWorkerCache::QueryParams webQueryParams;
    webQueryParams.ignoreSearch = options.ignoreSearch();
    webQueryParams.ignoreMethod = options.ignoreMethod();
    webQueryParams.ignoreVary = options.ignoreVary();
    webQueryParams.prefixMatch = options.prefixMatch();
    webQueryParams.cacheName = options.cacheName();
    return webQueryParams;
}

// FIXME: Consider using CallbackPromiseAdapter.
class CacheMatchCallbacks : public WebServiceWorkerCache::CacheMatchCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheMatchCallbacks);
public:
    CacheMatchCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebServiceWorkerResponse* webResponse) override
    {
        m_resolver->resolve(Response::create(m_resolver->scriptState()->executionContext(), *webResponse));
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) override
    {
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve();
        else
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

    virtual void onSuccess(WebVector<WebServiceWorkerResponse>* webResponses) override
    {
        HeapVector<Member<Response> > responses;
        for (size_t i = 0; i < webResponses->size(); ++i)
            responses.append(Response::create(m_resolver->scriptState()->executionContext(), (*webResponses)[i]));
        m_resolver->resolve(responses);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) override
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

protected:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheAddOrPutCallbacks : public CacheWithResponsesCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheAddOrPutCallbacks);
public:
    CacheAddOrPutCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : CacheWithResponsesCallbacks(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerResponse>* webResponses) override
    {
        // FIXME: Since response is ignored, consider simplifying public API.
        m_resolver->resolve();
        m_resolver.clear();
    }
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheDeleteCallback : public WebServiceWorkerCache::CacheWithResponsesCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheDeleteCallback);
public:
    CacheDeleteCallback(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerResponse>* webResponses) override
    {
        // FIXME: Since response is ignored, consider simplifying public API.
        m_resolver->resolve(true);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) override
    {
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve(false);
        else
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

    virtual void onSuccess(WebVector<WebServiceWorkerRequest>* webRequests) override
    {
        HeapVector<Member<Request> > requests;
        for (size_t i = 0; i < webRequests->size(); ++i)
            requests.append(Request::create(m_resolver->scriptState()->executionContext(), (*webRequests)[i]));
        m_resolver->resolve(requests);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) override
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

ScriptPromise rejectAsNotImplemented(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Cache is not implemented"));
}

} // namespace

Cache* Cache::create(WebServiceWorkerCache* webCache)
{
    return new Cache(webCache);
}

ScriptPromise Cache::match(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return matchImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return matchImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return matchAllImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return matchAllImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::add(ScriptState* scriptState, const RequestInfo& request, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return addImpl(scriptState, request.getAsRequest());
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return addImpl(scriptState, newRequest);
}

ScriptPromise Cache::addAll(ScriptState* scriptState, const Vector<ScriptValue>& rawRequests)
{
    // FIXME: Implement this.
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::deleteFunction(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return deleteImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return deleteImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::put(ScriptState* scriptState, const RequestInfo& request, Response* response, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return putImpl(scriptState, request.getAsRequest(), response);
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return putImpl(scriptState, newRequest, response);
}

ScriptPromise Cache::keys(ScriptState* scriptState, ExceptionState&)
{
    return keysImpl(scriptState);
}

ScriptPromise Cache::keys(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return keysImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState->executionContext(), request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return keysImpl(scriptState, newRequest, options);
}

Cache::Cache(WebServiceWorkerCache* webCache)
    : m_webCache(adoptPtr(webCache)) { }

ScriptPromise Cache::matchImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchMatch(new CacheMatchCallbacks(resolver), webRequest, toWebQueryParams(options));
    return promise;
}

ScriptPromise Cache::matchAllImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchMatchAll(new CacheWithResponsesCallbacks(resolver), webRequest, toWebQueryParams(options));
    return promise;
}

ScriptPromise Cache::addImpl(ScriptState* scriptState, const Request*)
{
    // FIXME: Implement this.
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::addAllImpl(ScriptState* scriptState, const Vector<const Request*>)
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

ScriptPromise Cache::deleteImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypeDelete;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    batchOperations[0].matchParams = toWebQueryParams(options);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchBatch(new CacheDeleteCallback(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::putImpl(ScriptState* scriptState, const Request* request, Response* response)
{
    KURL url(KURL(), request->url());
    if (!url.protocolIsInHTTPFamily())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Request scheme '" + url.protocol() + "' is unsupported"));
    if (request->method() != "GET")
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Request method '" + request->method() + "' is unsupported"));

    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypePut;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    response->populateWebServiceWorkerResponse(batchOperations[0].response);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchBatch(new CacheAddOrPutCallbacks(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, WebServiceWorkerCache::QueryParams());
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, toWebQueryParams(options));
    return promise;
}

} // namespace blink
