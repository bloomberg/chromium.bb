// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/Cache.h"

#include <algorithm>
#include <memory>
#include <string>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/V8Request.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/GlobalFetch.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "services/network/public/interfaces/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char kNotImplementedString[] =
    "NotSupportedError: Method is not implemented.";

class ScopedFetcherForTests final
    : public GarbageCollectedFinalized<ScopedFetcherForTests>,
      public GlobalFetch::ScopedFetcher {
  USING_GARBAGE_COLLECTED_MIXIN(ScopedFetcherForTests);

 public:
  static ScopedFetcherForTests* Create() { return new ScopedFetcherForTests; }

  ScriptPromise Fetch(ScriptState* script_state,
                      const RequestInfo& request_info,
                      const Dictionary&,
                      ExceptionState&) override {
    ++fetch_count_;
    if (expected_url_) {
      String fetched_url;
      if (request_info.IsRequest())
        EXPECT_EQ(*expected_url_, request_info.GetAsRequest()->url());
      else
        EXPECT_EQ(*expected_url_, request_info.GetAsUSVString());
    }

    if (response_) {
      ScriptPromiseResolver* resolver =
          ScriptPromiseResolver::Create(script_state);
      const ScriptPromise promise = resolver->Promise();
      resolver->Resolve(response_);
      response_ = nullptr;
      return promise;
    }
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "Unexpected call to fetch, no response available."));
  }

  // This does not take ownership of its parameter. The provided sample object
  // is used to check the parameter when called.
  void SetExpectedFetchUrl(const String* expected_url) {
    expected_url_ = expected_url;
  }
  void SetResponse(Response* response) { response_ = response; }

  int FetchCount() const { return fetch_count_; }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(response_);
    GlobalFetch::ScopedFetcher::Trace(visitor);
  }

 private:
  ScopedFetcherForTests() : fetch_count_(0), expected_url_(nullptr) {}

  int fetch_count_;
  const String* expected_url_;
  Member<Response> response_;
};

// A test implementation of the WebServiceWorkerCache interface which returns a
// (provided) error for every operation, and optionally checks arguments to
// methods against provided arguments. Also used as a base class for test
// specific caches.
class ErrorWebCacheForTests : public WebServiceWorkerCache {
 public:
  ErrorWebCacheForTests(const WebServiceWorkerCacheError error)
      : error_(error),
        expected_url_(nullptr),
        expected_query_params_(nullptr),
        expected_batch_operations_(nullptr) {}

  std::string GetAndClearLastErrorWebCacheMethodCalled() {
    std::string old = last_error_web_cache_method_called_;
    last_error_web_cache_method_called_.clear();
    return old;
  }

  // These methods do not take ownership of their parameter. They provide an
  // optional sample object to check parameters against.
  void SetExpectedUrl(const String* expected_url) {
    expected_url_ = expected_url;
  }
  void SetExpectedQueryParams(const QueryParams* expected_query_params) {
    expected_query_params_ = expected_query_params;
  }
  void SetExpectedBatchOperations(
      const WebVector<BatchOperation>* expected_batch_operations) {
    expected_batch_operations_ = expected_batch_operations;
  }

  // From WebServiceWorkerCache:
  void DispatchMatch(std::unique_ptr<CacheMatchCallbacks> callbacks,
                     const WebServiceWorkerRequest& web_request,
                     const QueryParams& query_params) override {
    last_error_web_cache_method_called_ = "dispatchMatch";
    CheckUrlIfProvided(web_request.Url());
    CheckQueryParamsIfProvided(query_params);

    return callbacks->OnError(error_);
  }

  void DispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
                        const WebServiceWorkerRequest& web_request,
                        const QueryParams& query_params) override {
    last_error_web_cache_method_called_ = "dispatchMatchAll";
    CheckUrlIfProvided(web_request.Url());
    CheckQueryParamsIfProvided(query_params);

    return callbacks->OnError(error_);
  }

  void DispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
                    const WebServiceWorkerRequest& web_request,
                    const QueryParams& query_params) override {
    last_error_web_cache_method_called_ = "dispatchKeys";
    if (!web_request.Url().IsEmpty()) {
      CheckUrlIfProvided(web_request.Url());
      CheckQueryParamsIfProvided(query_params);
    }

    return callbacks->OnError(error_);
  }

  void DispatchBatch(
      std::unique_ptr<CacheBatchCallbacks> callbacks,
      const WebVector<BatchOperation>& batch_operations) override {
    last_error_web_cache_method_called_ = "dispatchBatch";
    CheckBatchOperationsIfProvided(batch_operations);

    return callbacks->OnError(error_);
  }

 protected:
  void CheckUrlIfProvided(const KURL& url) {
    if (!expected_url_)
      return;
    EXPECT_EQ(*expected_url_, url);
  }

  void CheckQueryParamsIfProvided(const QueryParams& query_params) {
    if (!expected_query_params_)
      return;
    CompareQueryParamsForTest(*expected_query_params_, query_params);
  }

  void CheckBatchOperationsIfProvided(
      const WebVector<BatchOperation>& batch_operations) {
    if (!expected_batch_operations_)
      return;
    const WebVector<BatchOperation> expected_batch_operations =
        *expected_batch_operations_;
    EXPECT_EQ(expected_batch_operations.size(), batch_operations.size());
    for (int i = 0, minsize = std::min(expected_batch_operations.size(),
                                       batch_operations.size());
         i < minsize; ++i) {
      EXPECT_EQ(expected_batch_operations[i].operation_type,
                batch_operations[i].operation_type);
      const String expected_request_url =
          KURL(expected_batch_operations[i].request.Url());
      EXPECT_EQ(expected_request_url, KURL(batch_operations[i].request.Url()));
      ASSERT_EQ(expected_batch_operations[i].response.UrlList().size(),
                batch_operations[i].response.UrlList().size());
      for (size_t j = 0;
           j < expected_batch_operations[i].response.UrlList().size(); ++j) {
        EXPECT_EQ(expected_batch_operations[i].response.UrlList()[j],
                  batch_operations[i].response.UrlList()[j]);
      }
      CompareQueryParamsForTest(expected_batch_operations[i].match_params,
                                batch_operations[i].match_params);
    }
  }

 private:
  static void CompareQueryParamsForTest(
      const QueryParams& expected_query_params,
      const QueryParams& query_params) {
    EXPECT_EQ(expected_query_params.ignore_search, query_params.ignore_search);
    EXPECT_EQ(expected_query_params.ignore_method, query_params.ignore_method);
    EXPECT_EQ(expected_query_params.ignore_vary, query_params.ignore_vary);
    EXPECT_EQ(expected_query_params.cache_name, query_params.cache_name);
  }

  const WebServiceWorkerCacheError error_;

  const String* expected_url_;
  const QueryParams* expected_query_params_;
  const WebVector<BatchOperation>* expected_batch_operations_;

  std::string last_error_web_cache_method_called_;
};

class NotImplementedErrorCache : public ErrorWebCacheForTests {
 public:
  NotImplementedErrorCache()
      : ErrorWebCacheForTests(kWebServiceWorkerCacheErrorNotImplemented) {}
};

class CacheStorageTest : public ::testing::Test {
 public:
  CacheStorageTest() : page_(DummyPageHolder::Create(IntSize(1, 1))) {}

  Cache* CreateCache(ScopedFetcherForTests* fetcher,
                     WebServiceWorkerCache* web_cache) {
    return Cache::Create(fetcher, WTF::WrapUnique(web_cache));
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(page_->GetDocument().GetFrame());
  }
  ExecutionContext* GetExecutionContext() {
    return ExecutionContext::From(GetScriptState());
  }
  v8::Isolate* GetIsolate() { return GetScriptState()->GetIsolate(); }
  v8::Local<v8::Context> GetContext() { return GetScriptState()->GetContext(); }

  Request* NewRequestFromUrl(const String& url) {
    DummyExceptionStateForTesting exception_state;
    Request* request = Request::Create(GetScriptState(), url, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    return exception_state.HadException() ? nullptr : request;
  }

  // Convenience methods for testing the returned promises.
  ScriptValue GetRejectValue(ScriptPromise& promise) {
    ScriptValue on_reject;
    promise.Then(UnreachableFunction::Create(GetScriptState()),
                 TestFunction::Create(GetScriptState(), &on_reject));
    v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
    return on_reject;
  }

  std::string GetRejectString(ScriptPromise& promise) {
    ScriptValue on_reject = GetRejectValue(promise);
    return ToCoreString(
               on_reject.V8Value()->ToString(GetContext()).ToLocalChecked())
        .Ascii()
        .data();
  }

  ScriptValue GetResolveValue(ScriptPromise& promise) {
    ScriptValue on_resolve;
    promise.Then(TestFunction::Create(GetScriptState(), &on_resolve),
                 UnreachableFunction::Create(GetScriptState()));
    v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
    return on_resolve;
  }

  std::string GetResolveString(ScriptPromise& promise) {
    ScriptValue on_resolve = GetResolveValue(promise);
    return ToCoreString(
               on_resolve.V8Value()->ToString(GetContext()).ToLocalChecked())
        .Ascii()
        .data();
  }

 private:
  // A ScriptFunction that creates a test failure if it is ever called.
  class UnreachableFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state) {
      UnreachableFunction* self = new UnreachableFunction(script_state);
      return self->BindToV8Function();
    }

    ScriptValue Call(ScriptValue value) override {
      ADD_FAILURE() << "Unexpected call to a null ScriptFunction.";
      return value;
    }

   private:
    UnreachableFunction(ScriptState* script_state)
        : ScriptFunction(script_state) {}
  };

  // A ScriptFunction that saves its parameter; used by tests to assert on
  // correct values being passed.
  class TestFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state,
                                          ScriptValue* out_value) {
      TestFunction* self = new TestFunction(script_state, out_value);
      return self->BindToV8Function();
    }

    ScriptValue Call(ScriptValue value) override {
      DCHECK(!value.IsEmpty());
      *value_ = value;
      return value;
    }

   private:
    TestFunction(ScriptState* script_state, ScriptValue* out_value)
        : ScriptFunction(script_state), value_(out_value) {}

    ScriptValue* value_;
  };

  // Lifetime is that of the text fixture.
  std::unique_ptr<DummyPageHolder> page_;
};

RequestInfo StringToRequestInfo(const String& value) {
  RequestInfo info;
  info.SetUSVString(value);
  return info;
}

RequestInfo RequestToRequestInfo(Request* value) {
  RequestInfo info;
  info.SetRequest(value);
  return info;
}

TEST_F(CacheStorageTest, Basics) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  ErrorWebCacheForTests* test_cache;
  Cache* cache =
      CreateCache(fetcher, test_cache = new NotImplementedErrorCache());
  DCHECK(cache);

  const String url = "http://www.cachetest.org/";

  CacheQueryOptions options;
  ScriptPromise match_promise = cache->match(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_promise));

  cache = CreateCache(fetcher, test_cache = new ErrorWebCacheForTests(
                                   kWebServiceWorkerCacheErrorNotFound));
  match_promise = cache->match(GetScriptState(), StringToRequestInfo(url),
                               options, exception_state);
  ScriptValue script_value = GetResolveValue(match_promise);
  EXPECT_TRUE(script_value.IsUndefined());

  cache = CreateCache(fetcher, test_cache = new ErrorWebCacheForTests(
                                   kWebServiceWorkerCacheErrorExists));
  match_promise = cache->match(GetScriptState(), StringToRequestInfo(url),
                               options, exception_state);
  EXPECT_EQ("InvalidAccessError: Entry already exists.",
            GetRejectString(match_promise));
}

// Tests that arguments are faithfully passed on calls to Cache methods, except
// for methods which use batch operations, which are tested later.
TEST_F(CacheStorageTest, BasicArguments) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  ErrorWebCacheForTests* test_cache;
  Cache* cache =
      CreateCache(fetcher, test_cache = new NotImplementedErrorCache());
  DCHECK(cache);

  const String url = "http://www.cache.arguments.test/";
  test_cache->SetExpectedUrl(&url);

  WebServiceWorkerCache::QueryParams expected_query_params;
  expected_query_params.ignore_vary = true;
  expected_query_params.cache_name = "this is a cache name";
  test_cache->SetExpectedQueryParams(&expected_query_params);

  CacheQueryOptions options;
  options.setIgnoreVary(1);
  options.setCacheName(expected_query_params.cache_name);

  Request* request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise match_result =
      cache->match(GetScriptState(), RequestToRequestInfo(request), options,
                   exception_state);
  EXPECT_EQ("dispatchMatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_result));

  ScriptPromise string_match_result = cache->match(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchMatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_match_result));

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise match_all_result =
      cache->matchAll(GetScriptState(), RequestToRequestInfo(request), options,
                      exception_state);
  EXPECT_EQ("dispatchMatchAll",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_all_result));

  ScriptPromise string_match_all_result = cache->matchAll(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchMatchAll",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_match_all_result));

  ScriptPromise keys_result1 = cache->keys(GetScriptState(), exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(keys_result1));

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise keys_result2 =
      cache->keys(GetScriptState(), RequestToRequestInfo(request), options,
                  exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(keys_result2));

  ScriptPromise string_keys_result2 = cache->keys(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_keys_result2));
}

// Tests that arguments are faithfully passed to API calls that degrade to batch
// operations.
TEST_F(CacheStorageTest, BatchOperationArguments) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  ErrorWebCacheForTests* test_cache;
  Cache* cache =
      CreateCache(fetcher, test_cache = new NotImplementedErrorCache());
  DCHECK(cache);

  WebServiceWorkerCache::QueryParams expected_query_params;
  expected_query_params.cache_name = "this is another cache name";
  test_cache->SetExpectedQueryParams(&expected_query_params);

  CacheQueryOptions options;
  options.setCacheName(expected_query_params.cache_name);

  const String url = "http://batch.operations.test/";
  Request* request = NewRequestFromUrl(url);
  DCHECK(request);

  WebServiceWorkerResponse web_response;
  std::vector<KURL> url_list;
  url_list.push_back(KURL(url));
  web_response.SetURLList(url_list);
  Response* response = Response::Create(GetScriptState(), web_response);

  WebVector<WebServiceWorkerCache::BatchOperation> expected_delete_operations(
      size_t(1));
  {
    WebServiceWorkerCache::BatchOperation delete_operation;
    delete_operation.operation_type =
        WebServiceWorkerCache::kOperationTypeDelete;
    request->PopulateWebServiceWorkerRequest(delete_operation.request);
    delete_operation.match_params = expected_query_params;
    expected_delete_operations[0] = delete_operation;
  }
  test_cache->SetExpectedBatchOperations(&expected_delete_operations);

  ScriptPromise delete_result =
      cache->deleteFunction(GetScriptState(), RequestToRequestInfo(request),
                            options, exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(delete_result));

  ScriptPromise string_delete_result = cache->deleteFunction(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_delete_result));

  WebVector<WebServiceWorkerCache::BatchOperation> expected_put_operations(
      size_t(1));
  {
    WebServiceWorkerCache::BatchOperation put_operation;
    put_operation.operation_type = WebServiceWorkerCache::kOperationTypePut;
    request->PopulateWebServiceWorkerRequest(put_operation.request);
    response->PopulateWebServiceWorkerResponse(put_operation.response);
    expected_put_operations[0] = put_operation;
  }
  test_cache->SetExpectedBatchOperations(&expected_put_operations);

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise put_result = cache->put(
      GetScriptState(), RequestToRequestInfo(request),
      response->clone(GetScriptState(), exception_state), exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(put_result));

  ScriptPromise string_put_result = cache->put(
      GetScriptState(), StringToRequestInfo(url), response, exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_put_result));

  // FIXME: test add & addAll.
}

class MatchTestCache : public NotImplementedErrorCache {
 public:
  MatchTestCache(WebServiceWorkerResponse& response) : response_(response) {}

  // From WebServiceWorkerCache:
  void DispatchMatch(std::unique_ptr<CacheMatchCallbacks> callbacks,
                     const WebServiceWorkerRequest& web_request,
                     const QueryParams& query_params) override {
    return callbacks->OnSuccess(response_);
  }

 private:
  WebServiceWorkerResponse& response_;
};

TEST_F(CacheStorageTest, MatchResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  const String request_url = "http://request.url/";
  const String response_url = "http://match.response.test/";

  WebServiceWorkerResponse web_response;
  std::vector<KURL> url_list;
  url_list.push_back(KURL(response_url));
  web_response.SetURLList(url_list);
  web_response.SetResponseType(network::mojom::FetchResponseType::kDefault);

  Cache* cache = CreateCache(fetcher, new MatchTestCache(web_response));
  CacheQueryOptions options;

  ScriptPromise result =
      cache->match(GetScriptState(), StringToRequestInfo(request_url), options,
                   exception_state);
  ScriptValue script_value = GetResolveValue(result);
  Response* response =
      V8Response::ToImplWithTypeCheck(GetIsolate(), script_value.V8Value());
  ASSERT_TRUE(response);
  EXPECT_EQ(response_url, response->url());
}

class KeysTestCache : public NotImplementedErrorCache {
 public:
  KeysTestCache(WebVector<WebServiceWorkerRequest>& requests)
      : requests_(requests) {}

  void DispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
                    const WebServiceWorkerRequest& web_request,
                    const QueryParams& query_params) override {
    return callbacks->OnSuccess(requests_);
  }

 private:
  WebVector<WebServiceWorkerRequest>& requests_;
};

TEST_F(CacheStorageTest, KeysResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  const String url1 = "http://first.request/";
  const String url2 = "http://second.request/";

  Vector<String> expected_urls(size_t(2));
  expected_urls[0] = url1;
  expected_urls[1] = url2;

  WebVector<WebServiceWorkerRequest> web_requests(size_t(2));
  web_requests[0].SetURL(KURL(url1));
  web_requests[1].SetURL(KURL(url2));

  Cache* cache = CreateCache(fetcher, new KeysTestCache(web_requests));

  ScriptPromise result = cache->keys(GetScriptState(), exception_state);
  ScriptValue script_value = GetResolveValue(result);

  HeapVector<Member<Request>> requests =
      NativeValueTraits<IDLSequence<Request>>::NativeValue(
          GetIsolate(), script_value.V8Value(), exception_state);
  EXPECT_EQ(expected_urls.size(), requests.size());
  for (int i = 0, minsize = std::min(expected_urls.size(), requests.size());
       i < minsize; ++i) {
    Request* request = requests[i];
    EXPECT_TRUE(request);
    if (request)
      EXPECT_EQ(expected_urls[i], request->url());
  }
}

class MatchAllAndBatchTestCache : public NotImplementedErrorCache {
 public:
  MatchAllAndBatchTestCache(WebVector<WebServiceWorkerResponse>& responses)
      : responses_(responses) {}

  void DispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
                        const WebServiceWorkerRequest& web_request,
                        const QueryParams& query_params) override {
    return callbacks->OnSuccess(responses_);
  }

  void DispatchBatch(
      std::unique_ptr<CacheBatchCallbacks> callbacks,
      const WebVector<BatchOperation>& batch_operations) override {
    return callbacks->OnSuccess();
  }

 private:
  WebVector<WebServiceWorkerResponse>& responses_;
};

TEST_F(CacheStorageTest, MatchAllAndBatchResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  const String url1 = "http://first.response/";
  const String url2 = "http://second.response/";

  Vector<String> expected_urls(size_t(2));
  expected_urls[0] = url1;
  expected_urls[1] = url2;

  WebVector<WebServiceWorkerResponse> web_responses(size_t(2));
  web_responses[0].SetURLList(std::vector<KURL>({KURL(url1)}));
  web_responses[0].SetResponseType(network::mojom::FetchResponseType::kDefault);
  web_responses[1].SetURLList(std::vector<KURL>({KURL(url2)}));
  web_responses[1].SetResponseType(network::mojom::FetchResponseType::kDefault);

  Cache* cache =
      CreateCache(fetcher, new MatchAllAndBatchTestCache(web_responses));

  CacheQueryOptions options;
  ScriptPromise result =
      cache->matchAll(GetScriptState(), StringToRequestInfo("http://some.url/"),
                      options, exception_state);
  ScriptValue script_value = GetResolveValue(result);

  HeapVector<Member<Response>> responses =
      NativeValueTraits<IDLSequence<Response>>::NativeValue(
          GetIsolate(), script_value.V8Value(), exception_state);
  EXPECT_EQ(expected_urls.size(), responses.size());
  for (int i = 0, minsize = std::min(expected_urls.size(), responses.size());
       i < minsize; ++i) {
    Response* response = responses[i];
    EXPECT_TRUE(response);
    if (response)
      EXPECT_EQ(expected_urls[i], response->url());
  }

  result = cache->deleteFunction(GetScriptState(),
                                 StringToRequestInfo("http://some.url/"),
                                 options, exception_state);
  script_value = GetResolveValue(result);
  EXPECT_TRUE(script_value.V8Value()->IsBoolean());
  EXPECT_EQ(true, script_value.V8Value().As<v8::Boolean>()->Value());
}

TEST_F(CacheStorageTest, Add) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  ScopedFetcherForTests* fetcher = ScopedFetcherForTests::Create();
  const String url = "http://www.cacheadd.test/";
  const String content_type = "text/plain";
  const String content = "hello cache";

  ErrorWebCacheForTests* test_cache;
  Cache* cache =
      CreateCache(fetcher, test_cache = new NotImplementedErrorCache());

  fetcher->SetExpectedFetchUrl(&url);

  Request* request = NewRequestFromUrl(url);
  Response* response =
      Response::Create(GetScriptState(),
                       new BodyStreamBuffer(GetScriptState(),
                                            new FormDataBytesConsumer(content)),
                       content_type, ResponseInit(), exception_state);
  fetcher->SetResponse(response);

  WebVector<WebServiceWorkerCache::BatchOperation> expected_put_operations(
      size_t(1));
  {
    WebServiceWorkerCache::BatchOperation put_operation;
    put_operation.operation_type = WebServiceWorkerCache::kOperationTypePut;
    request->PopulateWebServiceWorkerRequest(put_operation.request);
    response->PopulateWebServiceWorkerResponse(put_operation.response);
    expected_put_operations[0] = put_operation;
  }
  test_cache->SetExpectedBatchOperations(&expected_put_operations);

  ScriptPromise add_result = cache->add(
      GetScriptState(), RequestToRequestInfo(request), exception_state);

  EXPECT_EQ(kNotImplementedString, GetRejectString(add_result));
  EXPECT_EQ(1, fetcher->FetchCount());
  EXPECT_EQ("dispatchBatch",
            test_cache->GetAndClearLastErrorWebCacheMethodCalled());
}

}  // namespace

}  // namespace blink
