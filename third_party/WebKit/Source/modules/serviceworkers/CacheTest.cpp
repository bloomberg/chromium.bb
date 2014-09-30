// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Cache.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Request.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/Response.h"
#include "public/platform/WebServiceWorkerCache.h"
#include "wtf/OwnPtr.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

namespace blink {

namespace {

const char kNotImplementedString[] = "NotSupportedError: Method is not implemented.";

// A test implementation of the WebServiceWorkerCache interface which returns a (provided) error for every operation, and optionally checks arguments
// to methods against provided arguments. Also used as a base class for test specific caches.
class ErrorWebCacheForTests : public WebServiceWorkerCache {
public:
    ErrorWebCacheForTests(const WebServiceWorkerCacheError error)
        : m_error(error)
        , m_expectedUrl(0)
        , m_expectedQueryParams(0)
        , m_expectedBatchOperations(0) { }

    std::string getAndClearLastErrorWebCacheMethodCalled()
    {
        std::string old = m_lastErrorWebCacheMethodCalled;
        m_lastErrorWebCacheMethodCalled.clear();
        return old;
    }

    // These methods do not take ownership of their parameter. They provide an optional sample object to check parameters against.
    void setExpectedUrl(const String* expectedUrl) { m_expectedUrl = expectedUrl; }
    void setExpectedQueryParams(const QueryParams* expectedQueryParams) { m_expectedQueryParams = expectedQueryParams; }
    void setExpectedBatchOperations(const WebVector<BatchOperation>* expectedBatchOperations) { m_expectedBatchOperations = expectedBatchOperations; }

    // From WebServiceWorkerCache:
    virtual void dispatchMatch(CacheMatchCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) OVERRIDE
    {
        m_lastErrorWebCacheMethodCalled = "dispatchMatch";
        checkUrlIfProvided(webRequest.url());
        checkQueryParamsIfProvided(queryParams);

        OwnPtr<CacheMatchCallbacks> ownedCallbacks(adoptPtr(callbacks));
        WebServiceWorkerCacheError error = m_error;
        return callbacks->onError(&error);
    }

    virtual void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) OVERRIDE
    {
        m_lastErrorWebCacheMethodCalled = "dispatchMatchAll";
        checkUrlIfProvided(webRequest.url());
        checkQueryParamsIfProvided(queryParams);

        OwnPtr<CacheWithResponsesCallbacks> ownedCallbacks(adoptPtr(callbacks));
        WebServiceWorkerCacheError error = m_error;
        return callbacks->onError(&error);
    }

    virtual void dispatchKeys(CacheWithRequestsCallbacks* callbacks, const WebServiceWorkerRequest* webRequest, const QueryParams& queryParams) OVERRIDE
    {
        m_lastErrorWebCacheMethodCalled = "dispatchKeys";
        if (webRequest) {
            checkUrlIfProvided(webRequest->url());
            checkQueryParamsIfProvided(queryParams);
        }

        OwnPtr<CacheWithRequestsCallbacks> ownedCallbacks(adoptPtr(callbacks));
        WebServiceWorkerCacheError error = m_error;
        return callbacks->onError(&error);
    }

    virtual void dispatchBatch(CacheWithResponsesCallbacks* callbacks, const WebVector<BatchOperation>& batchOperations) OVERRIDE
    {
        m_lastErrorWebCacheMethodCalled = "dispatchBatch";
        checkBatchOperationsIfProvided(batchOperations);

        OwnPtr<CacheWithResponsesCallbacks> ownedCallbacks(adoptPtr(callbacks));
        WebServiceWorkerCacheError error = m_error;
        return callbacks->onError(&error);
    }

protected:
    void checkUrlIfProvided(const KURL& url)
    {
        if (!m_expectedUrl)
            return;
        EXPECT_EQ(*m_expectedUrl, url);
    }

    void checkQueryParamsIfProvided(const QueryParams& queryParams)
    {
        if (!m_expectedQueryParams)
            return;
        CompareQueryParamsForTest(*m_expectedQueryParams, queryParams);
    }

    void checkBatchOperationsIfProvided(const WebVector<BatchOperation>& batchOperations)
    {
        if (!m_expectedBatchOperations)
            return;
        const WebVector<BatchOperation> expectedBatchOperations = *m_expectedBatchOperations;
        EXPECT_EQ(expectedBatchOperations.size(), batchOperations.size());
        for (int i = 0, minsize = std::min(expectedBatchOperations.size(), batchOperations.size()); i < minsize; ++i) {
            EXPECT_EQ(expectedBatchOperations[i].operationType, batchOperations[i].operationType);
            const String expectedRequestUrl = KURL(expectedBatchOperations[i].request.url());
            EXPECT_EQ(expectedRequestUrl, KURL(batchOperations[i].request.url()));
            const String expectedResponseUrl = KURL(expectedBatchOperations[i].response.url());
            EXPECT_EQ(expectedResponseUrl, KURL(batchOperations[i].response.url()));
            CompareQueryParamsForTest(expectedBatchOperations[i].matchParams, batchOperations[i].matchParams);
        }
    }

private:
    static void CompareQueryParamsForTest(const QueryParams& expectedQueryParams, const QueryParams& queryParams)
    {
        EXPECT_EQ(expectedQueryParams.ignoreSearch, queryParams.ignoreSearch);
        EXPECT_EQ(expectedQueryParams.ignoreMethod, queryParams.ignoreMethod);
        EXPECT_EQ(expectedQueryParams.ignoreVary, queryParams.ignoreVary);
        EXPECT_EQ(expectedQueryParams.prefixMatch, queryParams.prefixMatch);
        EXPECT_EQ(expectedQueryParams.cacheName, queryParams.cacheName);
    }

    const WebServiceWorkerCacheError m_error;

    const String* m_expectedUrl;
    const QueryParams* m_expectedQueryParams;
    const WebVector<BatchOperation>* m_expectedBatchOperations;

    std::string m_lastErrorWebCacheMethodCalled;
};

class NotImplementedErrorCache : public ErrorWebCacheForTests {
public:
    NotImplementedErrorCache() : ErrorWebCacheForTests(WebServiceWorkerCacheErrorNotImplemented) { }
};

class ServiceWorkerCacheTest : public ::testing::Test {
public:
    ServiceWorkerCacheTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1))) { }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* executionContext() { return scriptState()->executionContext(); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

    Request* newRequestFromUrl(const String& url)
    {
        TrackExceptionState exceptionState;
        Request* request = Request::create(executionContext(), url, exceptionState);
        EXPECT_FALSE(exceptionState.hadException());
        return exceptionState.hadException() ? 0 : request;
    }

    // Convenience methods for testing the returned promises.
    ScriptValue getRejectValue(ScriptPromise& promise)
    {
        ScriptValue onReject;
        promise.then(UnreachableFunction::create(scriptState()), TestFunction::create(scriptState(), &onReject));
        isolate()->RunMicrotasks();
        return onReject;
    }

    std::string getRejectString(ScriptPromise& promise)
    {
        ScriptValue onReject = getRejectValue(promise);
        return toCoreString(onReject.v8Value()->ToString()).ascii().data();
    }

    ScriptValue getResolveValue(ScriptPromise& promise)
    {
        ScriptValue onResolve;
        promise.then(TestFunction::create(scriptState(), &onResolve), UnreachableFunction::create(scriptState()));
        isolate()->RunMicrotasks();
        return onResolve;
    }

    std::string getResolveString(ScriptPromise& promise)
    {
        ScriptValue onResolve = getResolveValue(promise);
        return toCoreString(onResolve.v8Value()->ToString()).ascii().data();
    }

private:
    // A ScriptFunction that creates a test failure if it is ever called.
    class UnreachableFunction : public ScriptFunction {
    public:
        static v8::Handle<v8::Function> create(ScriptState* scriptState)
        {
            UnreachableFunction* self = new UnreachableFunction(scriptState);
            return self->bindToV8Function();
        }

        virtual ScriptValue call(ScriptValue value) OVERRIDE
        {
            ADD_FAILURE() << "Unexpected call to a null ScriptFunction.";
            return value;
        }
    private:
        UnreachableFunction(ScriptState* scriptState) : ScriptFunction(scriptState) { }
    };

    // A ScriptFunction that saves its parameter; used by tests to assert on correct
    // values being passed.
    class TestFunction : public ScriptFunction {
    public:
        static v8::Handle<v8::Function> create(ScriptState* scriptState, ScriptValue* outValue)
        {
            TestFunction* self = new TestFunction(scriptState, outValue);
            return self->bindToV8Function();
        }

        virtual ScriptValue call(ScriptValue value) OVERRIDE
        {
            ASSERT(!value.isEmpty());
            *m_value = value;
            return value;
        }

    private:
        TestFunction(ScriptState* scriptState, ScriptValue* outValue) : ScriptFunction(scriptState), m_value(outValue) { }

        ScriptValue* m_value;
    };

    // From ::testing::Test:
    virtual void SetUp() OVERRIDE
    {
        EXPECT_FALSE(m_scriptScope);
        m_scriptScope = adoptPtr(new ScriptState::Scope(scriptState()));
    }

    virtual void TearDown() OVERRIDE
    {
        m_scriptScope = 0;
    }

    // Lifetime is that of the text fixture.
    OwnPtr<DummyPageHolder> m_page;

    // Lifetime is per test instance.
    OwnPtr<ScriptState::Scope> m_scriptScope;
};

TEST_F(ServiceWorkerCacheTest, Basics)
{
    ErrorWebCacheForTests* testCache;
    Cache* cache = Cache::create(testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    const String url = "http://www.cachetest.org/";

    CacheQueryOptions* options = CacheQueryOptions::create();
    ScriptPromise matchPromise = cache->match(scriptState(), url, *options);
    EXPECT_EQ(kNotImplementedString, getRejectString(matchPromise));

    cache = Cache::create(testCache = new ErrorWebCacheForTests(WebServiceWorkerCacheErrorNotFound));
    matchPromise = cache->match(scriptState(), url, *options);
    EXPECT_EQ("NotFoundError: Entry was not found.", getRejectString(matchPromise));

    cache = Cache::create(testCache = new ErrorWebCacheForTests(WebServiceWorkerCacheErrorExists));
    matchPromise = cache->match(scriptState(), url, *options);
    EXPECT_EQ("InvalidAccessError: Entry already exists.", getRejectString(matchPromise));
}

// Tests that arguments are faithfully passed on calls to Cache methods, except for methods which use batch operations,
// which are tested later.
TEST_F(ServiceWorkerCacheTest, BasicArguments)
{
    ErrorWebCacheForTests* testCache;
    Cache* cache = Cache::create(testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    const String url = "http://www.cache.arguments.test/";
    testCache->setExpectedUrl(&url);

    WebServiceWorkerCache::QueryParams expectedQueryParams;
    expectedQueryParams.ignoreVary = true;
    expectedQueryParams.cacheName = "this is a cache name";
    testCache->setExpectedQueryParams(&expectedQueryParams);

    CacheQueryOptions* options = CacheQueryOptions::create();
    options->setIgnoreVary(1);
    options->setCacheName(expectedQueryParams.cacheName);

    Request* request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise matchResult = cache->match(scriptState(), request, *options);
    EXPECT_EQ("dispatchMatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(matchResult));

    ScriptPromise stringMatchResult = cache->match(scriptState(), url, *options);
    EXPECT_EQ("dispatchMatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringMatchResult));

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise matchAllResult = cache->matchAll(scriptState(), request, *options);
    EXPECT_EQ("dispatchMatchAll", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(matchAllResult));

    ScriptPromise stringMatchAllResult = cache->matchAll(scriptState(), url, *options);
    EXPECT_EQ("dispatchMatchAll", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringMatchAllResult));

    ScriptPromise keysResult1 = cache->keys(scriptState());
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(keysResult1));

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise keysResult2 = cache->keys(scriptState(), request, *options);
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(keysResult2));

    ScriptPromise stringKeysResult2 = cache->keys(scriptState(), url, *options);
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringKeysResult2));
}

// Tests that arguments are faithfully passed to API calls that degrade to batch operations.
TEST_F(ServiceWorkerCacheTest, BatchOperationArguments)
{
    ErrorWebCacheForTests* testCache;
    Cache* cache = Cache::create(testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    WebServiceWorkerCache::QueryParams expectedQueryParams;
    expectedQueryParams.prefixMatch = true;
    expectedQueryParams.cacheName = "this is another cache name";
    testCache->setExpectedQueryParams(&expectedQueryParams);

    CacheQueryOptions* options = CacheQueryOptions::create();
    options->setPrefixMatch(1);
    options->setCacheName(expectedQueryParams.cacheName);

    const String url = "http://batch.operations.test/";
    Request* request = newRequestFromUrl(url);
    ASSERT(request);

    WebServiceWorkerResponse webResponse;
    webResponse.setURL(KURL(ParsedURLString, url));
    Response* response = Response::create(executionContext(), webResponse);

    WebVector<WebServiceWorkerCache::BatchOperation> expectedDeleteOperations(size_t(1));
    {
        WebServiceWorkerCache::BatchOperation deleteOperation;
        deleteOperation.operationType = WebServiceWorkerCache::OperationTypeDelete;
        request->populateWebServiceWorkerRequest(deleteOperation.request);
        deleteOperation.matchParams = expectedQueryParams;
        expectedDeleteOperations[0] = deleteOperation;
    }
    testCache->setExpectedBatchOperations(&expectedDeleteOperations);

    ScriptPromise deleteResult = cache->deleteFunction(scriptState(), request, *options);
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(deleteResult));

    ScriptPromise stringDeleteResult = cache->deleteFunction(scriptState(), url, *options);
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringDeleteResult));

    WebVector<WebServiceWorkerCache::BatchOperation> expectedPutOperations(size_t(1));
    {
        WebServiceWorkerCache::BatchOperation putOperation;
        putOperation.operationType = WebServiceWorkerCache::OperationTypePut;
        request->populateWebServiceWorkerRequest(putOperation.request);
        response->populateWebServiceWorkerResponse(putOperation.response);
        expectedPutOperations[0] = putOperation;
    }
    testCache->setExpectedBatchOperations(&expectedPutOperations);

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise putResult = cache->put(scriptState(), request, response);
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(putResult));

    ScriptPromise stringPutResult = cache->put(scriptState(), url, response);
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringPutResult));

    // FIXME: test add & addAll.
}

class MatchTestCache : public NotImplementedErrorCache {
public:
    MatchTestCache(WebServiceWorkerResponse& response)
        : m_response(response) { }

    // From WebServiceWorkerCache:
    virtual void dispatchMatch(CacheMatchCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) OVERRIDE
    {
        OwnPtr<CacheMatchCallbacks> ownedCallbacks(adoptPtr(callbacks));
        return callbacks->onSuccess(&m_response);
    }

private:
    WebServiceWorkerResponse& m_response;
};

TEST_F(ServiceWorkerCacheTest, MatchResponseTest)
{
    const String requestUrl = "http://request.url/";
    const String responseUrl = "http://match.response.test/";

    WebServiceWorkerResponse webResponse;
    webResponse.setURL(KURL(ParsedURLString, responseUrl));

    Cache* cache = Cache::create(new MatchTestCache(webResponse));
    CacheQueryOptions* options = CacheQueryOptions::create();

    ScriptPromise result = cache->match(scriptState(), requestUrl, *options);
    ScriptValue scriptValue = getResolveValue(result);
    Response* response = V8Response::toImplWithTypeCheck(isolate(), scriptValue.v8Value());
    ASSERT_TRUE(response);
    EXPECT_EQ(responseUrl, response->url());
}

class KeysTestCache : public NotImplementedErrorCache {
public:
    KeysTestCache(WebVector<WebServiceWorkerRequest>& requests)
        : m_requests(requests) { }

    virtual void dispatchKeys(CacheWithRequestsCallbacks* callbacks, const WebServiceWorkerRequest* webRequest, const QueryParams& queryParams) OVERRIDE
    {
        OwnPtr<CacheWithRequestsCallbacks> ownedCallbacks(adoptPtr(callbacks));
        return callbacks->onSuccess(&m_requests);
    }

private:
    WebVector<WebServiceWorkerRequest>& m_requests;
};

TEST_F(ServiceWorkerCacheTest, KeysResponseTest)
{
    const String url1 = "http://first.request/";
    const String url2 = "http://second.request/";

    Vector<String> expectedUrls(size_t(2));
    expectedUrls[0] = url1;
    expectedUrls[1] = url2;

    WebVector<WebServiceWorkerRequest> webRequests(size_t(2));
    webRequests[0].setURL(KURL(ParsedURLString, url1));
    webRequests[1].setURL(KURL(ParsedURLString, url2));

    Cache* cache = Cache::create(new KeysTestCache(webRequests));

    ScriptPromise result = cache->keys(scriptState());
    ScriptValue scriptValue = getResolveValue(result);

    NonThrowableExceptionState exceptionState;
    Vector<v8::Handle<v8::Value> > requests = toImplArray<v8::Handle<v8::Value> >(scriptValue.v8Value(), 0, isolate(), exceptionState);
    EXPECT_EQ(expectedUrls.size(), requests.size());
    for (int i = 0, minsize = std::min(expectedUrls.size(), requests.size()); i < minsize; ++i) {
        Request* request = V8Request::toImplWithTypeCheck(isolate(), requests[i]);
        EXPECT_TRUE(request);
        if (request)
            EXPECT_EQ(expectedUrls[i], request->url());
    }
}

class MatchAllAndBatchTestCache : public NotImplementedErrorCache {
public:
    MatchAllAndBatchTestCache(WebVector<WebServiceWorkerResponse>& responses)
        : m_responses(responses) { }

    virtual void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) OVERRIDE
    {
        OwnPtr<CacheWithResponsesCallbacks> ownedCallbacks(adoptPtr(callbacks));
        return callbacks->onSuccess(&m_responses);
    }

    virtual void dispatchBatch(CacheWithResponsesCallbacks* callbacks, const WebVector<BatchOperation>& batchOperations) OVERRIDE
    {
        OwnPtr<CacheWithResponsesCallbacks> ownedCallbacks(adoptPtr(callbacks));
        return callbacks->onSuccess(&m_responses);
    }

private:
    WebVector<WebServiceWorkerResponse>& m_responses;
};

TEST_F(ServiceWorkerCacheTest, MatchAllAndBatchResponseTest)
{
    const String url1 = "http://first.response/";
    const String url2 = "http://second.response/";

    Vector<String> expectedUrls(size_t(2));
    expectedUrls[0] = url1;
    expectedUrls[1] = url2;

    WebVector<WebServiceWorkerResponse> webResponses(size_t(2));
    webResponses[0].setURL(KURL(ParsedURLString, url1));
    webResponses[1].setURL(KURL(ParsedURLString, url2));

    Cache* cache = Cache::create(new MatchAllAndBatchTestCache(webResponses));

    CacheQueryOptions* options = CacheQueryOptions::create();
    ScriptPromise result = cache->matchAll(scriptState(), "http://some.url/", *options);
    ScriptValue scriptValue = getResolveValue(result);

    NonThrowableExceptionState exceptionState;
    Vector<v8::Handle<v8::Value> > responses = toImplArray<v8::Handle<v8::Value> >(scriptValue.v8Value(), 0, isolate(), exceptionState);
    EXPECT_EQ(expectedUrls.size(), responses.size());
    for (int i = 0, minsize = std::min(expectedUrls.size(), responses.size()); i < minsize; ++i) {
        Response* response = V8Response::toImplWithTypeCheck(isolate(), responses[i]);
        EXPECT_TRUE(response);
        if (response)
            EXPECT_EQ(expectedUrls[i], response->url());
    }

    result = cache->deleteFunction(scriptState(), "http://some.url/", *options);
    scriptValue = getResolveValue(result);
    responses = toImplArray<v8::Handle<v8::Value> >(scriptValue.v8Value(), 0, isolate(), exceptionState);
    EXPECT_EQ(expectedUrls.size(), responses.size());
    for (int i = 0, minsize = std::min(expectedUrls.size(), responses.size()); i < minsize; ++i) {
        Response* response = V8Response::toImplWithTypeCheck(isolate(), responses[i]);
        EXPECT_TRUE(response);
        if (response)
            EXPECT_EQ(expectedUrls[i], response->url());
    }
}

} // namespace
} // namespace blink
