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
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "public/platform/WebServiceWorkerCache.h"

namespace blink {

namespace {

// FIXME: Consider using CallbackPromiseAdapter.
class CacheMatchCallbacks : public WebServiceWorkerCache::CacheMatchCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheMatchCallbacks);
public:
    CacheMatchCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
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
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithResponsesCallbacks : public WebServiceWorkerCache::CacheWithResponsesCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheWithResponsesCallbacks);
public:
    CacheWithResponsesCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerResponse>* webResponses) override
    {
        HeapVector<Member<Response>> responses;
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
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheAddOrPutCallbacks : public CacheWithResponsesCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheAddOrPutCallbacks);
public:
    CacheAddOrPutCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
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
    CacheDeleteCallback(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
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
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithRequestsCallbacks : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheWithRequestsCallbacks);
public:
    CacheWithRequestsCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    virtual void onSuccess(WebVector<WebServiceWorkerRequest>* webRequests) override
    {
        HeapVector<Member<Request>> requests;
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
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise rejectAsNotImplemented(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Cache is not implemented"));
}

} // namespace

class Cache::AsyncPutBatch final : public BodyStreamBuffer::BlobHandleCreatorClient {
public:
    AsyncPutBatch(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, Cache* cache, Request* request, Response* response)
        : m_resolver(resolver)
        , m_cache(cache)
    {
        request->populateWebServiceWorkerRequest(m_webRequest);
        response->populateWebServiceWorkerResponse(m_webResponse);
    }
    ~AsyncPutBatch() override { }
    void didCreateBlobHandle(PassRefPtr<BlobDataHandle> handle) override
    {
        WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
        batchOperations[0].operationType = WebServiceWorkerCache::OperationTypePut;
        batchOperations[0].request = m_webRequest;
        batchOperations[0].response = m_webResponse;
        batchOperations[0].response.setBlobDataHandle(handle);
        m_cache->webCache()->dispatchBatch(new CacheAddOrPutCallbacks(m_resolver.get()), batchOperations);
        cleanup();
    }
    void didFail(PassRefPtrWillBeRawPtr<DOMException> exception) override
    {
        ScriptState* state = m_resolver->scriptState();
        ScriptState::Scope scope(state);
        m_resolver->reject(V8ThrowException::createTypeError(state->isolate(), exception->toString()));
        cleanup();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_resolver);
        visitor->trace(m_cache);
        BlobHandleCreatorClient::trace(visitor);
    }

private:
    void cleanup()
    {
        m_resolver = nullptr;
        m_cache = nullptr;
    }
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
    Member<Cache> m_cache;
    WebServiceWorkerRequest m_webRequest;
    WebServiceWorkerResponse m_webResponse;
};

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

// static
WebServiceWorkerCache::QueryParams Cache::toWebQueryParams(const CacheQueryOptions& options)
{
    WebServiceWorkerCache::QueryParams webQueryParams;
    webQueryParams.ignoreSearch = options.ignoreSearch();
    webQueryParams.ignoreMethod = options.ignoreMethod();
    webQueryParams.ignoreVary = options.ignoreVary();
    webQueryParams.prefixMatch = options.prefixMatch();
    webQueryParams.cacheName = options.cacheName();
    return webQueryParams;
}

Cache::Cache(WebServiceWorkerCache* webCache)
    : m_webCache(adoptPtr(webCache)) { }

ScriptPromise Cache::matchImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchMatch(new CacheMatchCallbacks(resolver), webRequest, toWebQueryParams(options));
    return promise;
}

ScriptPromise Cache::matchAllImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
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

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchBatch(new CacheDeleteCallback(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::putImpl(ScriptState* scriptState, Request* request, Response* response)
{
    KURL url(KURL(), request->url());
    if (!url.protocolIsInHTTPFamily())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Request scheme '" + url.protocol() + "' is unsupported"));
    if (request->method() != "GET")
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Request method '" + request->method() + "' is unsupported"));
    if (request->hasBody() && request->bodyUsed())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Request body is already used"));
    if (response->hasBody() && response->bodyUsed())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Response body is already used"));

    if (request->hasBody())
        request->setBodyUsed();
    if (response->hasBody())
        response->setBodyUsed();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    if (BodyStreamBuffer* buffer = response->internalBuffer()) {
        if (buffer == response->buffer() && response->streamAccessed()) {
            bool dataLost = false;
            buffer = response->createDrainingStream(&dataLost);
            if (dataLost) {
                resolver->reject(DOMException::create(NotSupportedError, "Storing the Response which .body is partially read is not supported."));
                return promise;
            }
        }
        // If the response body type is stream, read the all data and create the
        // blob handle and dispatch the put batch asynchronously.
        buffer->readAllAndCreateBlobHandle(response->internalContentTypeForBuffer(), new AsyncPutBatch(resolver, this, request, response));
        return promise;
    }
    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypePut;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    response->populateWebServiceWorkerResponse(batchOperations[0].response);

    m_webCache->dispatchBatch(new CacheAddOrPutCallbacks(resolver), batchOperations);
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, WebServiceWorkerCache::QueryParams());
    return promise;
}

ScriptPromise Cache::keysImpl(ScriptState* scriptState, const Request* request, const CacheQueryOptions& options)
{
    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    m_webCache->dispatchKeys(new CacheWithRequestsCallbacks(resolver), 0, toWebQueryParams(options));
    return promise;
}

WebServiceWorkerCache* Cache::webCache() const
{
    return m_webCache.get();
}

} // namespace blink
