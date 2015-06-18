// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/cachestorage/Cache.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/DOMException.h"
#include "modules/cachestorage/CacheStorageError.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/GlobalFetch.h"
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

    // Ownership of |rawReason| must be passed.
    virtual void onError(WebServiceWorkerCacheError* rawReason) override
    {
        OwnPtr<WebServiceWorkerCacheError> reason = adoptPtr(rawReason);
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve();
        else
            m_resolver->reject(CacheStorageError::createException(*reason));
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

    // Ownership of |rawReason| must be passed.
    virtual void onError(WebServiceWorkerCacheError* rawReason) override
    {
        OwnPtr<WebServiceWorkerCacheError> reason = adoptPtr(rawReason);
        m_resolver->reject(CacheStorageError::createException(*reason));
        m_resolver.clear();
    }

protected:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheDeleteCallback : public WebServiceWorkerCache::CacheBatchCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheDeleteCallback);
public:
    CacheDeleteCallback(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }

    void onSuccess() override
    {
        m_resolver->resolve(true);
        m_resolver.clear();
    }

    // Ownership of |rawReason| must be passed.
    virtual void onError(WebServiceWorkerCacheError* rawReason) override
    {
        OwnPtr<WebServiceWorkerCacheError> reason = adoptPtr(rawReason);
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve(false);
        else
            m_resolver->reject(CacheStorageError::createException(*reason));
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

    // Ownership of |rawReason| must be passed.
    virtual void onError(WebServiceWorkerCacheError* rawReason) override
    {
        OwnPtr<WebServiceWorkerCacheError> reason = adoptPtr(rawReason);
        m_resolver->reject(CacheStorageError::createException(*reason));
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

class Cache::FetchResolvedForAdd final : public ScriptFunction {
public:
    static v8::Local<v8::Function> create(ScriptState* scriptState, Cache* cache, Request* request)
    {
        FetchResolvedForAdd* self = new FetchResolvedForAdd(scriptState, cache, request);
        return self->bindToV8Function();
    }

    ScriptValue call(ScriptValue value) override
    {
        Response* response = V8Response::toImplWithTypeCheck(scriptState()->isolate(), value.v8Value());
        ScriptPromise putPromise = m_cache->putImpl(scriptState(), m_request, response);
        return ScriptValue(scriptState(), putPromise.v8Value());
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_cache);
        visitor->trace(m_request);
        ScriptFunction::trace(visitor);
    }

private:
    FetchResolvedForAdd(ScriptState* scriptState, Cache* cache, Request* request)
        : ScriptFunction(scriptState)
        , m_cache(cache)
        , m_request(request)
    {
    }

    Member<Cache> m_cache;
    Member<Request> m_request;
};

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
        m_cache->webCache()->dispatchBatch(new CallbackPromiseAdapter<void, CacheStorageError>(m_resolver.get()), batchOperations);
        cleanup();
    }
    void didFail(DOMException* exception) override
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

Cache* Cache::create(WeakPtr<GlobalFetch::ScopedFetcher> fetcher, WebServiceWorkerCache* webCache)
{
    return new Cache(fetcher, webCache);
}

ScriptPromise Cache::match(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return matchImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return matchImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, const RequestInfo& request, const CacheQueryOptions& options, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return matchAllImpl(scriptState, request.getAsRequest(), options);
    Request* newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return matchAllImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::add(ScriptState* scriptState, const RequestInfo& request, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    Request* newRequest;
    if (request.isRequest()) {
        newRequest = request.getAsRequest();
    } else {
        newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);

        if (exceptionState.hadException())
            return ScriptPromise();
    }

    Vector<Request*> requestVector;
    requestVector.append(newRequest);
    return addAllImpl(scriptState, requestVector, exceptionState);
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
    Request* newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();
    return deleteImpl(scriptState, newRequest, options);
}

ScriptPromise Cache::put(ScriptState* scriptState, const RequestInfo& request, Response* response, ExceptionState& exceptionState)
{
    ASSERT(!request.isNull());
    if (request.isRequest())
        return putImpl(scriptState, request.getAsRequest(), response);
    Request* newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);
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
    Request* newRequest = Request::create(scriptState, request.getAsUSVString(), exceptionState);
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
    webQueryParams.cacheName = options.cacheName();
    return webQueryParams;
}

Cache::Cache(WeakPtr<GlobalFetch::ScopedFetcher> fetcher, WebServiceWorkerCache* webCache)
    : m_scopedFetcher(fetcher)
    , m_webCache(adoptPtr(webCache)) { }

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

ScriptPromise Cache::addAllImpl(ScriptState* scriptState, const Vector<Request*>& requests, ExceptionState& exceptionState)
{
    // TODO(gavinp,nhiroki): Implement addAll for more than one element.
    ASSERT(requests.size() == 1);

    Vector<RequestInfo> requestInfos;
    requestInfos.resize(requests.size());
    for (size_t i = 0; i < requests.size(); ++i) {
        if (!requests[i]->url().protocolIsInHTTPFamily())
            return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Add/AddAll does not support schemes other than \"http\" or \"https\""));
        if (requests[i]->method() != "GET")
            return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Add/AddAll only supports the GET request method."));
        requestInfos[i].setRequest(requests[i]);
    }

    ScriptPromise fetchPromise = m_scopedFetcher->fetch(scriptState, requestInfos[0], Dictionary(), exceptionState);
    return fetchPromise.then(FetchResolvedForAdd::create(scriptState, this, requests[0]));
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
        request->lockBody(Body::PassBody);
    if (response->hasBody())
        response->lockBody(Body::PassBody);

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    if (BodyStreamBuffer* buffer = response->internalBuffer()) {
        if (buffer == response->buffer() && response->isBodyConsumed())
            buffer = response->createDrainingStream();
        // If the response body type is stream, read the all data and create the
        // blob handle and dispatch the put batch asynchronously.
        buffer->readAllAndCreateBlobHandle(response->internalMIMEType(), new AsyncPutBatch(resolver, this, request, response));
        return promise;
    }
    WebVector<WebServiceWorkerCache::BatchOperation> batchOperations(size_t(1));
    batchOperations[0].operationType = WebServiceWorkerCache::OperationTypePut;
    request->populateWebServiceWorkerRequest(batchOperations[0].request);
    response->populateWebServiceWorkerResponse(batchOperations[0].response);

    m_webCache->dispatchBatch(new CallbackPromiseAdapter<void, CacheStorageError>(resolver), batchOperations);
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
