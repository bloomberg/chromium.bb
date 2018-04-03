// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/Cache.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Response.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/fetch/BodyStreamBuffer.h"
#include "core/fetch/FetchDataLoader.h"
#include "core/fetch/Request.h"
#include "core/fetch/Response.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/cachestorage/CacheStorageError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/loader/fetch/CachedMetadata.h"
#include "platform/network/http_names.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/runtime_enabled_features.h"
#include "public/platform/modules/cache_storage/cache_storage.mojom-blink.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"

namespace blink {

namespace {

// FIXME: Consider using CallbackPromiseAdapter.
class CacheMatchCallbacks : public WebServiceWorkerCache::CacheMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(CacheMatchCallbacks);

 public:
  explicit CacheMatchCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(const WebServiceWorkerResponse& web_response) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    ScriptState::Scope scope(resolver_->GetScriptState());
    resolver_->Resolve(
        Response::Create(resolver_->GetScriptState(), web_response));
    resolver_.Clear();
  }

  void OnError(mojom::CacheStorageError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == mojom::CacheStorageError::kErrorNotFound)
      resolver_->Resolve();
    else
      resolver_->Reject(CacheStorageError::CreateException(reason));
    resolver_.Clear();
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithResponsesCallbacks
    : public WebServiceWorkerCache::CacheWithResponsesCallbacks {
  WTF_MAKE_NONCOPYABLE(CacheWithResponsesCallbacks);

 public:
  explicit CacheWithResponsesCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(
      const WebVector<WebServiceWorkerResponse>& web_responses) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    ScriptState::Scope scope(resolver_->GetScriptState());
    HeapVector<Member<Response>> responses;
    for (size_t i = 0; i < web_responses.size(); ++i)
      responses.push_back(
          Response::Create(resolver_->GetScriptState(), web_responses[i]));
    resolver_->Resolve(responses);
    resolver_.Clear();
  }

  void OnError(mojom::CacheStorageError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(CacheStorageError::CreateException(reason));
    resolver_.Clear();
  }

 protected:
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheDeleteCallback : public WebServiceWorkerCache::CacheBatchCallbacks {
  WTF_MAKE_NONCOPYABLE(CacheDeleteCallback);

 public:
  explicit CacheDeleteCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess() override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Resolve(true);
    resolver_.Clear();
  }

  void OnError(mojom::CacheStorageError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == mojom::CacheStorageError::kErrorNotFound)
      resolver_->Resolve(false);
    else
      resolver_->Reject(CacheStorageError::CreateException(reason));
    resolver_.Clear();
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheWithRequestsCallbacks
    : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
  WTF_MAKE_NONCOPYABLE(CacheWithRequestsCallbacks);

 public:
  explicit CacheWithRequestsCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(
      const WebVector<WebServiceWorkerRequest>& web_requests) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    ScriptState::Scope scope(resolver_->GetScriptState());
    HeapVector<Member<Request>> requests;
    for (size_t i = 0; i < web_requests.size(); ++i)
      requests.push_back(
          Request::Create(resolver_->GetScriptState(), web_requests[i]));
    resolver_->Resolve(requests);
    resolver_.Clear();
  }

  void OnError(mojom::CacheStorageError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(CacheStorageError::CreateException(reason));
    resolver_.Clear();
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

void RecordResponseTypeForAdd(const Member<Response>& response) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.Cache.AddResponseType",
                            response->GetResponse()->GetType());
}

bool VaryHeaderContainsAsterisk(const Response* response) {
  const FetchHeaderList* headers = response->headers()->HeaderList();
  String varyHeader;
  if (headers->Get("vary", varyHeader)) {
    Vector<String> fields;
    varyHeader.Split(',', fields);
    return std::any_of(fields.begin(), fields.end(), [](const String& field) {
      return field.StripWhiteSpace() == "*";
    });
  }
  return false;
}

bool ShouldGenerateV8CodeCache(ScriptState* script_state,
                               const Response* response) {
  if (!RuntimeEnabledFeatures::PWAFullCodeCacheEnabled())
    return false;
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsServiceWorkerGlobalScope())
    return false;
  if (!ToServiceWorkerGlobalScope(context)->IsInstalling())
    return false;
  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response->InternalMIMEType())) {
    return false;
  }
  if (!response->InternalBodyBuffer())
    return false;
  return true;
}

}  // namespace

// TODO(nhiroki): Unfortunately, we have to go through V8 to wait for the fetch
// promise. It should be better to achieve this only within C++ world.
class Cache::FetchResolvedForAdd final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(
      ScriptState* script_state,
      Cache* cache,
      const HeapVector<Member<Request>>& requests) {
    FetchResolvedForAdd* self =
        new FetchResolvedForAdd(script_state, cache, requests);
    return self->BindToV8Function();
  }

  ScriptValue Call(ScriptValue value) override {
    NonThrowableExceptionState exception_state;
    HeapVector<Member<Response>> responses =
        NativeValueTraits<IDLSequence<Response>>::NativeValue(
            GetScriptState()->GetIsolate(), value.V8Value(), exception_state);

    for (const auto& response : responses) {
      if (!response->ok()) {
        ScriptPromise rejection = ScriptPromise::Reject(
            GetScriptState(),
            V8ThrowException::CreateTypeError(GetScriptState()->GetIsolate(),
                                              "Request failed"));
        return ScriptValue(GetScriptState(), rejection.V8Value());
      }
      if (VaryHeaderContainsAsterisk(response)) {
        ScriptPromise rejection = ScriptPromise::Reject(
            GetScriptState(),
            V8ThrowException::CreateTypeError(GetScriptState()->GetIsolate(),
                                              "Vary header contains *"));
        return ScriptValue(GetScriptState(), rejection.V8Value());
      }
    }

    for (const auto& response : responses)
      RecordResponseTypeForAdd(response);

    ScriptPromise put_promise =
        cache_->PutImpl(GetScriptState(), requests_, responses);
    return ScriptValue(GetScriptState(), put_promise.V8Value());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(cache_);
    visitor->Trace(requests_);
    ScriptFunction::Trace(visitor);
  }

 private:
  FetchResolvedForAdd(ScriptState* script_state,
                      Cache* cache,
                      const HeapVector<Member<Request>>& requests)
      : ScriptFunction(script_state), cache_(cache), requests_(requests) {}

  Member<Cache> cache_;
  HeapVector<Member<Request>> requests_;
};

class Cache::BarrierCallbackForPut final
    : public GarbageCollectedFinalized<BarrierCallbackForPut> {
 public:
  BarrierCallbackForPut(int number_of_operations,
                        Cache* cache,
                        ScriptPromiseResolver* resolver)
      : number_of_remaining_operations_(number_of_operations),
        cache_(cache),
        resolver_(resolver) {
    DCHECK_LT(0, number_of_remaining_operations_);
    batch_operations_.resize(number_of_operations);
  }

  void OnSuccess(size_t index,
                 const WebServiceWorkerCache::BatchOperation& batch_operation) {
    DCHECK_LT(index, batch_operations_.size());
    if (!StillActive())
      return;
    batch_operations_[index] = batch_operation;
    if (--number_of_remaining_operations_ != 0)
      return;
    MaybeReportInstalledScripts();
    cache_->WebCache()->DispatchBatch(
        std::make_unique<CallbackPromiseAdapter<void, CacheStorageError>>(
            resolver_),
        batch_operations_);
  }

  void OnError(const String& error_message) {
    if (!StillActive())
      return;
    completed_ = true;
    ScriptState* state = resolver_->GetScriptState();
    ScriptState::Scope scope(state);
    resolver_->Reject(
        V8ThrowException::CreateTypeError(state->GetIsolate(), error_message));
  }

  void Abort() {
    if (!StillActive())
      return;
    completed_ = true;
    ScriptState::Scope scope(resolver_->GetScriptState());
    resolver_->Reject(DOMException::Create(kAbortError));
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(cache_);
    visitor->Trace(resolver_);
  }

 private:
  bool StillActive() {
    if (completed_)
      return false;
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return false;

    return true;
  }

  // Report the script stats if this cache storage is for service worker
  // execution context and it's in installation phase.
  void MaybeReportInstalledScripts() {
    ExecutionContext* context = resolver_->GetExecutionContext();
    if (!context || !context->IsServiceWorkerGlobalScope())
      return;
    ServiceWorkerGlobalScope* global_scope =
        ToServiceWorkerGlobalScope(context);
    if (!global_scope->IsInstalling())
      return;

    for (const auto& operation : batch_operations_) {
      scoped_refptr<BlobDataHandle> blob_data_handle =
          operation.response.GetBlobDataHandle();
      if (!blob_data_handle)
        continue;
      if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
              blob_data_handle->GetType())) {
        continue;
      }
      global_scope->CountCacheStorageInstalledScript(
          blob_data_handle->size(), operation.response.SideDataBlobSize());
    }
  }

  bool completed_ = false;
  int number_of_remaining_operations_;
  Member<Cache> cache_;
  Member<ScriptPromiseResolver> resolver_;
  Vector<WebServiceWorkerCache::BatchOperation> batch_operations_;
};

class Cache::BlobHandleCallbackForPut final
    : public GarbageCollectedFinalized<BlobHandleCallbackForPut>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BlobHandleCallbackForPut);

 public:
  BlobHandleCallbackForPut(size_t index,
                           BarrierCallbackForPut* barrier_callback,
                           Request* request,
                           Response* response)
      : index_(index), barrier_callback_(barrier_callback) {
    request->PopulateWebServiceWorkerRequest(web_request_);
    response->PopulateWebServiceWorkerResponse(web_response_);
  }
  ~BlobHandleCallbackForPut() override = default;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> handle) override {
    WebServiceWorkerCache::BatchOperation batch_operation;
    batch_operation.operation_type = WebServiceWorkerCache::kOperationTypePut;
    batch_operation.request = web_request_;
    batch_operation.response = web_response_;
    batch_operation.response.SetBlobDataHandle(std::move(handle));
    barrier_callback_->OnSuccess(index_, batch_operation);
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->OnError("network error");
  }

  void Abort() override { barrier_callback_->Abort(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(barrier_callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  const size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;

  WebServiceWorkerRequest web_request_;
  WebServiceWorkerResponse web_response_;
};

class Cache::CodeCacheHandleCallbackForPut final
    : public GarbageCollectedFinalized<CodeCacheHandleCallbackForPut>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(CodeCacheHandleCallbackForPut);

 public:
  CodeCacheHandleCallbackForPut(ScriptState* script_state,
                                size_t index,
                                BarrierCallbackForPut* barrier_callback,
                                Request* request,
                                Response* response)
      : script_state_(script_state),
        index_(index),
        barrier_callback_(barrier_callback),
        mime_type_(response->InternalMIMEType()) {
    request->PopulateWebServiceWorkerRequest(web_request_);
    response->PopulateWebServiceWorkerResponse(web_response_);
  }
  ~CodeCacheHandleCallbackForPut() override = default;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    WebServiceWorkerCache::BatchOperation batch_operation;
    batch_operation.operation_type = WebServiceWorkerCache::kOperationTypePut;
    batch_operation.request = web_request_;
    batch_operation.response = web_response_;

    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->SetContentType(mime_type_);
    blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
    batch_operation.response.SetBlobDataHandle(BlobDataHandle::Create(
        std::move(blob_data), array_buffer->ByteLength()));

    // Currently we only support UTF8 encoding.
    // TODO(horo): Use the charset in Content-type header of the response.
    // See crbug.com/743311.
    std::unique_ptr<TextResourceDecoder> text_decoder =
        TextResourceDecoder::Create(
            TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());

    scoped_refptr<CachedMetadata> cached_metadata =
        V8ScriptRunner::GenerateFullCodeCache(
            script_state_.get(),
            text_decoder->Decode(static_cast<const char*>(array_buffer->Data()),
                                 array_buffer->ByteLength()),
            web_request_.Url().GetString(), text_decoder->Encoding(),
            web_response_.ResponseType() ==
                    network::mojom::FetchResponseType::kOpaque
                ? V8ScriptRunner::OpaqueMode::kOpaque
                : V8ScriptRunner::OpaqueMode::kNotOpaque);
    if (!cached_metadata) {
      barrier_callback_->OnSuccess(index_, batch_operation);
      return;
    }
    const Vector<char>& serialized_data = cached_metadata->SerializedData();
    std::unique_ptr<BlobData> side_data_blob_data = BlobData::Create();
    side_data_blob_data->AppendBytes(serialized_data.data(),
                                     serialized_data.size());

    batch_operation.response.SetSideDataBlobDataHandle(BlobDataHandle::Create(
        std::move(side_data_blob_data), serialized_data.size()));
    barrier_callback_->OnSuccess(index_, batch_operation);
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->OnError("network error");
  }

  void Abort() override { barrier_callback_->Abort(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(barrier_callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  const scoped_refptr<ScriptState> script_state_;
  const size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;
  const String mime_type_;

  WebServiceWorkerRequest web_request_;
  WebServiceWorkerResponse web_response_;
};

Cache* Cache::Create(GlobalFetch::ScopedFetcher* fetcher,
                     std::unique_ptr<WebServiceWorkerCache> web_cache) {
  return new Cache(fetcher, std::move(web_cache));
}

ScriptPromise Cache::match(ScriptState* script_state,
                           const RequestInfo& request,
                           const CacheQueryOptions& options,
                           ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return MatchImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return MatchImpl(script_state, new_request, options);
}

ScriptPromise Cache::matchAll(ScriptState* script_state,
                              ExceptionState& exception_state) {
  return MatchAllImpl(script_state);
}

ScriptPromise Cache::matchAll(ScriptState* script_state,
                              const RequestInfo& request,
                              const CacheQueryOptions& options,
                              ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return MatchAllImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return MatchAllImpl(script_state, new_request, options);
}

ScriptPromise Cache::add(ScriptState* script_state,
                         const RequestInfo& request,
                         ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  HeapVector<Member<Request>> requests;
  if (request.IsRequest()) {
    requests.push_back(request.GetAsRequest());
  } else {
    requests.push_back(Request::Create(script_state, request.GetAsUSVString(),
                                       exception_state));
    if (exception_state.HadException())
      return ScriptPromise();
  }

  return AddAllImpl(script_state, requests, exception_state);
}

ScriptPromise Cache::addAll(ScriptState* script_state,
                            const HeapVector<RequestInfo>& raw_requests,
                            ExceptionState& exception_state) {
  HeapVector<Member<Request>> requests;
  for (RequestInfo request : raw_requests) {
    if (request.IsRequest()) {
      requests.push_back(request.GetAsRequest());
    } else {
      requests.push_back(Request::Create(script_state, request.GetAsUSVString(),
                                         exception_state));
      if (exception_state.HadException())
        return ScriptPromise();
    }
  }

  return AddAllImpl(script_state, requests, exception_state);
}

ScriptPromise Cache::Delete(ScriptState* script_state,
                            const RequestInfo& request,
                            const CacheQueryOptions& options,
                            ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return DeleteImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return DeleteImpl(script_state, new_request, options);
}

ScriptPromise Cache::put(ScriptState* script_state,
                         const RequestInfo& request,
                         Response* response,
                         ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return PutImpl(script_state,
                   HeapVector<Member<Request>>(1, request.GetAsRequest()),
                   HeapVector<Member<Response>>(1, response));
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return PutImpl(script_state, HeapVector<Member<Request>>(1, new_request),
                 HeapVector<Member<Response>>(1, response));
}

ScriptPromise Cache::keys(ScriptState* script_state, ExceptionState&) {
  return KeysImpl(script_state);
}

ScriptPromise Cache::keys(ScriptState* script_state,
                          const RequestInfo& request,
                          const CacheQueryOptions& options,
                          ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return KeysImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return KeysImpl(script_state, new_request, options);
}

// static
WebServiceWorkerCache::QueryParams Cache::ToWebQueryParams(
    const CacheQueryOptions& options) {
  WebServiceWorkerCache::QueryParams web_query_params;
  web_query_params.ignore_search = options.ignoreSearch();
  web_query_params.ignore_method = options.ignoreMethod();
  web_query_params.ignore_vary = options.ignoreVary();
  web_query_params.cache_name = options.cacheName();
  return web_query_params;
}

Cache::Cache(GlobalFetch::ScopedFetcher* fetcher,
             std::unique_ptr<WebServiceWorkerCache> web_cache)
    : scoped_fetcher_(fetcher), web_cache_(std::move(web_cache)) {}

void Cache::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise Cache::MatchImpl(ScriptState* script_state,
                               const Request* request,
                               const CacheQueryOptions& options) {
  WebServiceWorkerRequest web_request;
  request->PopulateWebServiceWorkerRequest(web_request);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }
  web_cache_->DispatchMatch(std::make_unique<CacheMatchCallbacks>(resolver),
                            web_request, ToWebQueryParams(options));
  return promise;
}

ScriptPromise Cache::MatchAllImpl(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  web_cache_->DispatchMatchAll(
      std::make_unique<CacheWithResponsesCallbacks>(resolver),
      WebServiceWorkerRequest(), WebServiceWorkerCache::QueryParams());
  return promise;
}

ScriptPromise Cache::MatchAllImpl(ScriptState* script_state,
                                  const Request* request,
                                  const CacheQueryOptions& options) {
  WebServiceWorkerRequest web_request;
  request->PopulateWebServiceWorkerRequest(web_request);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->Resolve(HeapVector<Member<Response>>());
    return promise;
  }
  web_cache_->DispatchMatchAll(
      std::make_unique<CacheWithResponsesCallbacks>(resolver), web_request,
      ToWebQueryParams(options));
  return promise;
}

ScriptPromise Cache::AddAllImpl(ScriptState* script_state,
                                const HeapVector<Member<Request>>& requests,
                                ExceptionState& exception_state) {
  if (requests.IsEmpty())
    return ScriptPromise::CastUndefined(script_state);

  HeapVector<RequestInfo> request_infos;
  request_infos.resize(requests.size());
  Vector<ScriptPromise> promises;
  promises.resize(requests.size());
  for (size_t i = 0; i < requests.size(); ++i) {
    if (!requests[i]->url().ProtocolIsInHTTPFamily())
      return ScriptPromise::Reject(script_state,
                                   V8ThrowException::CreateTypeError(
                                       script_state->GetIsolate(),
                                       "Add/AddAll does not support schemes "
                                       "other than \"http\" or \"https\""));
    if (requests[i]->method() != HTTPNames::GET)
      return ScriptPromise::Reject(
          script_state,
          V8ThrowException::CreateTypeError(
              script_state->GetIsolate(),
              "Add/AddAll only supports the GET request method."));
    request_infos[i].SetRequest(requests[i]);

    promises[i] = scoped_fetcher_->Fetch(script_state, request_infos[i],
                                         Dictionary(), exception_state);
  }

  return ScriptPromise::All(script_state, promises)
      .Then(FetchResolvedForAdd::Create(script_state, this, requests));
}

ScriptPromise Cache::DeleteImpl(ScriptState* script_state,
                                const Request* request,
                                const CacheQueryOptions& options) {
  WebVector<WebServiceWorkerCache::BatchOperation> batch_operations(size_t(1));
  batch_operations[0].operation_type =
      WebServiceWorkerCache::kOperationTypeDelete;
  request->PopulateWebServiceWorkerRequest(batch_operations[0].request);
  batch_operations[0].match_params = ToWebQueryParams(options);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->Resolve(false);
    return promise;
  }
  web_cache_->DispatchBatch(std::make_unique<CacheDeleteCallback>(resolver),
                            batch_operations);
  return promise;
}

ScriptPromise Cache::PutImpl(ScriptState* script_state,
                             const HeapVector<Member<Request>>& requests,
                             const HeapVector<Member<Response>>& responses) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  BarrierCallbackForPut* barrier_callback =
      new BarrierCallbackForPut(requests.size(), this, resolver);

  for (size_t i = 0; i < requests.size(); ++i) {
    KURL url(NullURL(), requests[i]->url());
    if (!url.ProtocolIsInHTTPFamily()) {
      barrier_callback->OnError("Request scheme '" + url.Protocol() +
                                "' is unsupported");
      return promise;
    }
    if (requests[i]->method() != HTTPNames::GET) {
      barrier_callback->OnError("Request method '" + requests[i]->method() +
                                "' is unsupported");
      return promise;
    }
    DCHECK(!requests[i]->HasBody());

    if (VaryHeaderContainsAsterisk(responses[i])) {
      barrier_callback->OnError("Vary header contains *");
      return promise;
    }
    if (responses[i]->status() == 206) {
      barrier_callback->OnError(
          "Partial response (status code 206) is unsupported");
      return promise;
    }
    if (responses[i]->IsBodyLocked() || responses[i]->bodyUsed()) {
      barrier_callback->OnError("Response body is already used");
      return promise;
    }

    BodyStreamBuffer* buffer = responses[i]->InternalBodyBuffer();

    if (ShouldGenerateV8CodeCache(script_state, responses[i])) {
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsArrayBuffer();
      buffer->StartLoading(loader, new CodeCacheHandleCallbackForPut(
                                       script_state, i, barrier_callback,
                                       requests[i], responses[i]));
      continue;
    }

    if (buffer) {
      // If the response has body, read the all data and create
      // the blob handle and dispatch the put batch asynchronously.
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsBlobHandle(
          responses[i]->InternalMIMEType());
      buffer->StartLoading(
          loader, new BlobHandleCallbackForPut(i, barrier_callback, requests[i],
                                               responses[i]));
      continue;
    }

    WebServiceWorkerCache::BatchOperation batch_operation;
    batch_operation.operation_type = WebServiceWorkerCache::kOperationTypePut;
    requests[i]->PopulateWebServiceWorkerRequest(batch_operation.request);
    responses[i]->PopulateWebServiceWorkerResponse(batch_operation.response);
    barrier_callback->OnSuccess(i, batch_operation);
  }

  return promise;
}

ScriptPromise Cache::KeysImpl(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  web_cache_->DispatchKeys(
      std::make_unique<CacheWithRequestsCallbacks>(resolver),
      WebServiceWorkerRequest(), WebServiceWorkerCache::QueryParams());
  return promise;
}

ScriptPromise Cache::KeysImpl(ScriptState* script_state,
                              const Request* request,
                              const CacheQueryOptions& options) {
  WebServiceWorkerRequest web_request;
  request->PopulateWebServiceWorkerRequest(web_request);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->Resolve(HeapVector<Member<Request>>());
    return promise;
  }
  web_cache_->DispatchKeys(
      std::make_unique<CacheWithRequestsCallbacks>(resolver), web_request,
      ToWebQueryParams(options));
  return promise;
}

WebServiceWorkerCache* Cache::WebCache() const {
  return web_cache_.get();
}

}  // namespace blink
