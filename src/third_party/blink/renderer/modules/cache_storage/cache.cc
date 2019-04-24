// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache.h"

#include <memory>
#include <utility>
#include "base/feature_list.h"
#include "base/optional.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/request_init.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {

namespace {

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

enum class CodeCacheGenerateTiming {
  kDontGenerate,
  kGenerateNow,
  kGenerateWhenIdle,
};

CodeCacheGenerateTiming ShouldGenerateV8CodeCache(ScriptState* script_state,
                                                  const Response* response) {
  EagerCodeCacheStrategy strategy =
      ServiceWorkerUtils::GetEagerCodeCacheStrategy();
  if (strategy == EagerCodeCacheStrategy::kDontGenerate)
    return CodeCacheGenerateTiming::kDontGenerate;

  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
  if (!global_scope)
    return CodeCacheGenerateTiming::kDontGenerate;

  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response->InternalMIMEType())) {
    return CodeCacheGenerateTiming::kDontGenerate;
  }

  if (!response->InternalBodyBuffer())
    return CodeCacheGenerateTiming::kDontGenerate;

  if (global_scope->IsInstalling())
    return CodeCacheGenerateTiming::kGenerateNow;

  if (strategy == EagerCodeCacheStrategy::kOnIdleTask) {
    return CodeCacheGenerateTiming::kGenerateWhenIdle;
  }

  return CodeCacheGenerateTiming::kDontGenerate;
}

}  // namespace

// TODO(nhiroki): Unfortunately, we have to go through V8 to wait for the fetch
// promise. It should be better to achieve this only within C++ world.
class Cache::FetchResolvedForAdd final : public ScriptFunction {
 public:
  // |exception_state| is passed so that the context_type, interface_name and
  // property_name can be copied and then used to construct a new ExceptionState
  // object asynchronously later.
  static v8::Local<v8::Function> Create(
      ScriptState* script_state,
      Cache* cache,
      const String& method_name,
      const HeapVector<Member<Request>>& requests,
      const ExceptionState& exception_state) {
    FetchResolvedForAdd* self = MakeGarbageCollected<FetchResolvedForAdd>(
        script_state, cache, method_name, requests, exception_state);
    return self->BindToV8Function();
  }

  FetchResolvedForAdd(ScriptState* script_state,
                      Cache* cache,
                      const String& method_name,
                      const HeapVector<Member<Request>>& requests,
                      const ExceptionState& exception_state)
      : ScriptFunction(script_state),
        cache_(cache),
        method_name_(method_name),
        requests_(requests),
        context_type_(exception_state.Context()),
        property_name_(exception_state.PropertyName()),
        interface_name_(exception_state.InterfaceName()) {}

  ScriptValue Call(ScriptValue value) override {
    ExceptionState exception_state(GetScriptState()->GetIsolate(),
                                   context_type_, property_name_,
                                   interface_name_);
    HeapVector<Member<Response>> responses =
        NativeValueTraits<IDLSequence<Response>>::NativeValue(
            GetScriptState()->GetIsolate(), value.V8Value(), exception_state);
    if (exception_state.HadException()) {
      ScriptPromise rejection =
          ScriptPromise::Reject(GetScriptState(), exception_state);
      return ScriptValue(GetScriptState(), rejection.V8Value());
    }

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

    ScriptPromise put_promise = cache_->PutImpl(
        GetScriptState(), method_name_, requests_, responses, exception_state);
    return ScriptValue(GetScriptState(), put_promise.V8Value());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(cache_);
    visitor->Trace(requests_);
    ScriptFunction::Trace(visitor);
  }

 private:
  Member<Cache> cache_;
  const String method_name_;
  HeapVector<Member<Request>> requests_;
  ExceptionState::ContextType context_type_;
  const char* property_name_;
  const char* interface_name_;
};

class Cache::BarrierCallbackForPut final
    : public GarbageCollectedFinalized<BarrierCallbackForPut> {
 public:
  BarrierCallbackForPut(wtf_size_t number_of_operations,
                        Cache* cache,
                        const String& method_name,
                        ScriptPromiseResolver* resolver)
      : number_of_remaining_operations_(number_of_operations),
        cache_(cache),
        method_name_(method_name),
        resolver_(resolver) {
    DCHECK_LT(0, number_of_remaining_operations_);
    batch_operations_.resize(number_of_operations);
  }

  void OnSuccess(wtf_size_t index,
                 mojom::blink::BatchOperationPtr batch_operation) {
    DCHECK_LT(index, batch_operations_.size());
    if (!StillActive())
      return;
    batch_operations_[index] = std::move(batch_operation);
    if (--number_of_remaining_operations_ != 0)
      return;
    MaybeReportInstalledScripts();
    int operation_count = batch_operations_.size();
    DCHECK_GE(operation_count, 1);
    // Make sure to bind the Cache object to keep the mojo interface pointer
    // alive during the operation.  Otherwise GC might prevent the callback
    // from ever being executed.
    cache_->cache_ptr_->Batch(
        std::move(batch_operations_),
        RuntimeEnabledFeatures::CacheStorageAddAllRejectsDuplicatesEnabled(),
        WTF::Bind(
            [](const String& method_name, ScriptPromiseResolver* resolver,
               base::TimeTicks start_time, int operation_count, Cache* _,
               mojom::blink::CacheStorageVerboseErrorPtr error) {
              base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
              if (operation_count > 1) {
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.Cache.Renderer.PutMany", elapsed);
              } else {
                DCHECK_EQ(operation_count, 1);
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.Cache.Renderer.PutOne", elapsed);
              }
              ExecutionContext* context = resolver->GetExecutionContext();
              if (!context || context->IsContextDestroyed())
                return;
              String message;
              if (error->message) {
                message.append(method_name);
                message.append(": ");
                message.append(error->message);
              }
              if (error->value == mojom::blink::CacheStorageError::kSuccess) {
                resolver->Resolve();
                if (message) {
                  context->AddConsoleMessage(ConsoleMessage::Create(
                      kJSMessageSource, mojom::ConsoleMessageLevel::kWarning,
                      message));

                  // If the message indicates there were duplicate requests in
                  // the batch argument list, but the operation succeeded
                  // anyway, then record the UseCounter event.  We use string
                  // matching on the message to avoid temporarily plumbing
                  // additional meta data through the operation result.
                  // TODO(crbug.com/877737): Remove this once the
                  // cache.addAll() duplicate rejection finally ships.
                  if (error->message.Contains(
                          blink::cache_storage::
                              kDuplicateOperationBaseMessage)) {
                    Deprecation::CountDeprecation(
                        context,
                        WebFeature::kCacheStorageAddAllSuccessWithDuplicate);
                  }
                }
              } else {
                resolver->Reject(
                    CacheStorageError::CreateException(error->value, message));
              }
            },
            method_name_, WrapPersistent(resolver_.Get()),
            base::TimeTicks::Now(), operation_count,
            WrapPersistent(cache_.Get())));
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
    resolver_->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
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
    auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
    if (!global_scope)
      return;
    if (!global_scope->IsInstalling())
      return;

    for (const auto& operation : batch_operations_) {
      scoped_refptr<BlobDataHandle> blob_data_handle =
          operation->response->blob;
      if (!blob_data_handle)
        continue;
      if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
              blob_data_handle->GetType())) {
        continue;
      }
      uint64_t side_data_blob_size =
          operation->response->side_data_blob
              ? operation->response->side_data_blob->size()
              : 0;
      global_scope->CountCacheStorageInstalledScript(blob_data_handle->size(),
                                                     side_data_blob_size);
    }
  }

  bool completed_ = false;
  int number_of_remaining_operations_;
  Member<Cache> cache_;
  const String method_name_;
  Member<ScriptPromiseResolver> resolver_;
  Vector<mojom::blink::BatchOperationPtr> batch_operations_;
};

class Cache::BlobHandleCallbackForPut final
    : public GarbageCollectedFinalized<BlobHandleCallbackForPut>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BlobHandleCallbackForPut);

 public:
  BlobHandleCallbackForPut(wtf_size_t index,
                           BarrierCallbackForPut* barrier_callback,
                           Request* request,
                           Response* response)
      : index_(index), barrier_callback_(barrier_callback) {
    fetch_api_request_ = request->CreateFetchAPIRequest();
    fetch_api_response_ = response->PopulateFetchAPIResponse();
  }
  ~BlobHandleCallbackForPut() override = default;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> handle) override {
    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = std::move(fetch_api_request_);
    batch_operation->response = std::move(fetch_api_response_);
    batch_operation->response->blob = handle;
    barrier_callback_->OnSuccess(index_, std::move(batch_operation));
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
  const wtf_size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;

  mojom::blink::FetchAPIRequestPtr fetch_api_request_;
  mojom::blink::FetchAPIResponsePtr fetch_api_response_;
};

class Cache::CodeCacheHandleCallbackForPut final
    : public GarbageCollectedFinalized<CodeCacheHandleCallbackForPut>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(CodeCacheHandleCallbackForPut);

 public:
  CodeCacheHandleCallbackForPut(ScriptState* script_state,
                                Cache* cache,
                                wtf_size_t index,
                                BarrierCallbackForPut* barrier_callback,
                                Request* request,
                                Response* response,
                                CodeCacheGenerateTiming timing)
      : script_state_(script_state),
        cache_(cache),
        index_(index),
        barrier_callback_(barrier_callback),
        mime_type_(response->InternalMIMEType()),
        timing_(timing) {
    fetch_api_request_ = request->CreateFetchAPIRequest();
    fetch_api_response_ = response->PopulateFetchAPIResponse();
    url_ = fetch_api_request_->url;
    opaque_mode_ = fetch_api_response_->response_type ==
                           network::mojom::FetchResponseType::kOpaque
                       ? V8CodeCache::OpaqueMode::kOpaque
                       : V8CodeCache::OpaqueMode::kNotOpaque;
  }
  ~CodeCacheHandleCallbackForPut() override = default;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    base::Time response_time = fetch_api_response_->response_time;
    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = std::move(fetch_api_request_);
    batch_operation->response = std::move(fetch_api_response_);

    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->SetContentType(mime_type_);
    blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
    batch_operation->response->blob = BlobDataHandle::Create(
        std::move(blob_data), array_buffer->ByteLength());

    if (timing_ == CodeCacheGenerateTiming::kGenerateNow) {
      scoped_refptr<CachedMetadata> cached_metadata =
          GenerateFullCodeCache(array_buffer);
      if (cached_metadata) {
        const Vector<uint8_t>& serialized_data =
            cached_metadata->SerializedData();
        std::unique_ptr<BlobData> side_data_blob_data = BlobData::Create();
        side_data_blob_data->AppendBytes(serialized_data.data(),
                                         serialized_data.size());

        batch_operation->response->side_data_blob = BlobDataHandle::Create(
            std::move(side_data_blob_data), serialized_data.size());
      }
    } else {
      // Schedule an idle task to generate code cache later.
      ServiceWorkerGlobalScope* global_scope = GetServiceWorkerGlobalScope();
      if (global_scope) {
        auto* thread_scheduler =
            global_scope->GetScheduler()->GetWorkerThreadScheduler();
        DCHECK(thread_scheduler);
        int task_id = global_scope->WillStartTask();
        thread_scheduler->IdleTaskRunner()->PostIdleTask(
            FROM_HERE, WTF::Bind(&Cache::CodeCacheHandleCallbackForPut::
                                     GenerateCodeCacheOnIdleTask,
                                 WrapPersistent(this), task_id,
                                 WrapPersistent(array_buffer), response_time));
      }
    }

    barrier_callback_->OnSuccess(index_, std::move(batch_operation));
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->OnError("network error");
  }

  void Abort() override { barrier_callback_->Abort(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(script_state_);
    visitor->Trace(cache_);
    visitor->Trace(barrier_callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  ServiceWorkerGlobalScope* GetServiceWorkerGlobalScope() {
    ExecutionContext* context = ExecutionContext::From(script_state_);
    if (!context || context->IsContextDestroyed())
      return nullptr;
    // Currently |this| is only created for triggering V8 code caching after
    // Cache#put() is used by a service worker so |script_state_| should be
    // ServiceWorkerGlobalScope.
    auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
    DCHECK(global_scope);
    return global_scope;
  }

  scoped_refptr<CachedMetadata> GenerateFullCodeCache(
      DOMArrayBuffer* array_buffer) {
    // Currently we only support UTF8 encoding.
    // TODO(horo): Use the charset in Content-type header of the response.
    // See crbug.com/743311.
    std::unique_ptr<TextResourceDecoder> text_decoder =
        TextResourceDecoder::Create(
            TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());

    return V8CodeCache::GenerateFullCodeCache(
        script_state_,
        text_decoder->Decode(static_cast<const char*>(array_buffer->Data()),
                             array_buffer->ByteLength()),
        url_, text_decoder->Encoding(), opaque_mode_);
  }

  void GenerateCodeCacheOnIdleTask(int task_id,
                                   DOMArrayBuffer* array_buffer,
                                   base::Time response_time,
                                   base::TimeTicks) {
    ServiceWorkerGlobalScope* global_scope = GetServiceWorkerGlobalScope();
    if (!global_scope)
      return;

    scoped_refptr<CachedMetadata> cached_metadata =
        GenerateFullCodeCache(array_buffer);
    if (!cached_metadata) {
      global_scope->DidEndTask(task_id);
      return;
    }
    cache_->cache_ptr_->SetSideData(
        url_, response_time, cached_metadata->SerializedData(),
        WTF::Bind(
            [](ServiceWorkerGlobalScope* global_scope, int task_id,
               mojom::blink::CacheStorageError error) {
              global_scope->DidEndTask(task_id);
            },
            WrapPersistent(global_scope), task_id));
  }

  const Member<ScriptState> script_state_;
  const Member<Cache> cache_;
  const wtf_size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;
  const String mime_type_;
  KURL url_;
  V8CodeCache::OpaqueMode opaque_mode_;
  CodeCacheGenerateTiming timing_;

  mojom::blink::FetchAPIRequestPtr fetch_api_request_;
  mojom::blink::FetchAPIResponsePtr fetch_api_response_;
};

Cache* Cache::Create(
    GlobalFetch::ScopedFetcher* fetcher,
    CacheStorage* cache_storage,
    mojom::blink::CacheStorageCacheAssociatedPtrInfo cache_ptr_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return MakeGarbageCollected<Cache>(fetcher, cache_storage,
                                     std::move(cache_ptr_info),
                                     std::move(task_runner));
}

ScriptPromise Cache::match(ScriptState* script_state,
                           const RequestInfo& request,
                           const CacheQueryOptions* options,
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
  return MatchAllImpl(script_state, nullptr, CacheQueryOptions::Create());
}

ScriptPromise Cache::matchAll(ScriptState* script_state,
                              const RequestInfo& request,
                              const CacheQueryOptions* options,
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

  return AddAllImpl(script_state, "Cache.add()", requests, exception_state);
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

  return AddAllImpl(script_state, "Cache.addAll()", requests, exception_state);
}

ScriptPromise Cache::Delete(ScriptState* script_state,
                            const RequestInfo& request,
                            const CacheQueryOptions* options,
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
  if (request.IsRequest()) {
    return PutImpl(script_state, "Cache.put()",
                   HeapVector<Member<Request>>(1, request.GetAsRequest()),
                   HeapVector<Member<Response>>(1, response), exception_state);
  }
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return PutImpl(script_state, "Cache.put()",
                 HeapVector<Member<Request>>(1, new_request),
                 HeapVector<Member<Response>>(1, response), exception_state);
}

ScriptPromise Cache::keys(ScriptState* script_state, ExceptionState&) {
  return KeysImpl(script_state, nullptr, CacheQueryOptions::Create());
}

ScriptPromise Cache::keys(ScriptState* script_state,
                          const RequestInfo& request,
                          const CacheQueryOptions* options,
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

Cache::Cache(GlobalFetch::ScopedFetcher* fetcher,
             CacheStorage* cache_storage,
             mojom::blink::CacheStorageCacheAssociatedPtrInfo cache_ptr_info,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : scoped_fetcher_(fetcher), cache_storage_(cache_storage) {
  cache_ptr_.Bind(std::move(cache_ptr_info), std::move(task_runner));
}

void Cache::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  visitor->Trace(cache_storage_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise Cache::MatchImpl(ScriptState* script_state,
                               const Request* request,
                               const CacheQueryOptions* options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  // Make sure to bind the Cache object to keep the mojo interface pointer
  // alive during the operation.  Otherwise GC might prevent the callback
  // from ever being executed.
  cache_ptr_->Match(
      request->CreateFetchAPIRequest(),
      mojom::blink::CacheQueryOptions::From(options),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, Cache* _,
             mojom::blink::MatchResultPtr result) {
            base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Match",
                                     elapsed);
            if (options->hasIgnoreSearch() && options->ignoreSearch()) {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.Cache.Renderer.Match.IgnoreSearch",
                  elapsed);
            }
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              switch (result->get_status()) {
                case mojom::CacheStorageError::kErrorNotFound:
                  UMA_HISTOGRAM_LONG_TIMES(
                      "ServiceWorkerCache.Cache.Renderer.Match.Miss", elapsed);
                  resolver->Resolve();
                  break;
                default:
                  resolver->Reject(
                      CacheStorageError::CreateException(result->get_status()));
                  break;
              }
            } else {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.Cache.Renderer.Match.Hit", elapsed);
              ScriptState::Scope scope(resolver->GetScriptState());
              resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                 *result->get_response()));
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), WrapPersistent(this)));

  return promise;
}

ScriptPromise Cache::MatchAllImpl(ScriptState* script_state,
                                  const Request* request,
                                  const CacheQueryOptions* options) {
  mojom::blink::FetchAPIRequestPtr fetch_api_request;
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (request) {
    fetch_api_request = request->CreateFetchAPIRequest();

    if (request->method() != http_names::kGET && !options->ignoreMethod()) {
      resolver->Resolve(HeapVector<Member<Response>>());
      return promise;
    }
  }

  // Make sure to bind the Cache object to keep the mojo interface pointer
  // alive during the operation.  Otherwise GC might prevent the callback
  // from ever being executed.
  cache_ptr_->MatchAll(
      std::move(fetch_api_request),
      mojom::blink::CacheQueryOptions::From(options),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, Cache* _,
             mojom::blink::MatchAllResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.MatchAll",
                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              resolver->Reject(
                  CacheStorageError::CreateException(result->get_status()));
            } else {
              ScriptState::Scope scope(resolver->GetScriptState());
              HeapVector<Member<Response>> responses;
              responses.ReserveInitialCapacity(result->get_responses().size());
              for (auto& response : result->get_responses()) {
                responses.push_back(
                    Response::Create(resolver->GetScriptState(), *response));
              }
              resolver->Resolve(responses);
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), WrapPersistent(this)));
  return promise;
}

ScriptPromise Cache::AddAllImpl(ScriptState* script_state,
                                const String& method_name,
                                const HeapVector<Member<Request>>& requests,
                                ExceptionState& exception_state) {
  if (requests.IsEmpty())
    return ScriptPromise::CastUndefined(script_state);

  HeapVector<RequestInfo> request_infos;
  request_infos.resize(requests.size());
  Vector<ScriptPromise> promises;
  promises.resize(requests.size());
  for (wtf_size_t i = 0; i < requests.size(); ++i) {
    if (!requests[i]->url().ProtocolIsInHTTPFamily()) {
      return ScriptPromise::Reject(script_state,
                                   V8ThrowException::CreateTypeError(
                                       script_state->GetIsolate(),
                                       "Add/AddAll does not support schemes "
                                       "other than \"http\" or \"https\""));
    }
    if (requests[i]->method() != http_names::kGET) {
      return ScriptPromise::Reject(
          script_state,
          V8ThrowException::CreateTypeError(
              script_state->GetIsolate(),
              "Add/AddAll only supports the GET request method."));
    }
    request_infos[i].SetRequest(requests[i]);

    promises[i] = scoped_fetcher_->Fetch(
        script_state, request_infos[i], RequestInit::Create(), exception_state);
  }

  return ScriptPromise::All(script_state, promises)
      .Then(FetchResolvedForAdd::Create(script_state, this, method_name,
                                        requests, exception_state));
}

ScriptPromise Cache::DeleteImpl(ScriptState* script_state,
                                const Request* request,
                                const CacheQueryOptions* options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve(false);
    return promise;
  }

  Vector<mojom::blink::BatchOperationPtr> batch_operations;
  batch_operations.push_back(mojom::blink::BatchOperation::New());
  auto& operation = batch_operations.back();
  operation->operation_type = mojom::blink::OperationType::kDelete;
  operation->request = request->CreateFetchAPIRequest();
  operation->match_options = mojom::blink::CacheQueryOptions::From(options);

  // Make sure to bind the Cache object to keep the mojo interface pointer
  // alive during the operation.  Otherwise GC might prevent the callback
  // from ever being executed.
  cache_ptr_->Batch(
      std::move(batch_operations),
      RuntimeEnabledFeatures::CacheStorageAddAllRejectsDuplicatesEnabled(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, Cache* _,
             mojom::blink::CacheStorageVerboseErrorPtr error) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.DeleteOne",
                base::TimeTicks::Now() - start_time);
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context || context->IsContextDestroyed())
              return;
            String message;
            if (error->message) {
              message.append("Cache.delete(): ");
              message.append(error->message);
            }
            bool report_to_console = false;
            if (error->value != mojom::blink::CacheStorageError::kSuccess) {
              switch (error->value) {
                case mojom::blink::CacheStorageError::kErrorNotFound:
                  report_to_console = true;
                  resolver->Resolve(false);
                  break;
                default:
                  resolver->Reject(CacheStorageError::CreateException(
                      error->value, message));
                  break;
              }
            } else {
              report_to_console = true;
              resolver->Resolve(true);
            }
            if (report_to_console && message) {
              context->AddConsoleMessage(ConsoleMessage::Create(
                  kJSMessageSource, mojom::ConsoleMessageLevel::kWarning,
                  message));
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), WrapPersistent(this)));
  return promise;
}

ScriptPromise Cache::PutImpl(ScriptState* script_state,
                             const String& method_name,
                             const HeapVector<Member<Request>>& requests,
                             const HeapVector<Member<Response>>& responses,
                             ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();
  BarrierCallbackForPut* barrier_callback =
      MakeGarbageCollected<BarrierCallbackForPut>(requests.size(), this,
                                                  method_name, resolver);

  for (wtf_size_t i = 0; i < requests.size(); ++i) {
    KURL url(NullURL(), requests[i]->url());
    if (!url.ProtocolIsInHTTPFamily()) {
      barrier_callback->OnError("Request scheme '" + url.Protocol() +
                                "' is unsupported");
      return promise;
    }
    if (requests[i]->method() != http_names::kGET) {
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
    if (responses[i]->IsBodyLocked(exception_state) ==
            Body::BodyLocked::kLocked ||
        responses[i]->IsBodyUsed(exception_state) == Body::BodyUsed::kUsed) {
      DCHECK(!exception_state.HadException());
      barrier_callback->OnError("Response body is already used");
      return promise;
    }
    if (exception_state.HadException()) {
      // TODO(ricea): Reject the promise with the actual exception.
      barrier_callback->OnError("Could not inspect response body state");
      return promise;
    }

    BodyStreamBuffer* buffer = responses[i]->InternalBodyBuffer();

    CodeCacheGenerateTiming cache_generate_timing =
        ShouldGenerateV8CodeCache(script_state, responses[i]);
    if (cache_generate_timing != CodeCacheGenerateTiming::kDontGenerate) {
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsArrayBuffer();
      buffer->StartLoading(
          loader,
          MakeGarbageCollected<CodeCacheHandleCallbackForPut>(
              script_state, this, i, barrier_callback, requests[i],
              responses[i], cache_generate_timing),
          exception_state);
      if (exception_state.HadException()) {
        barrier_callback->OnError("Could not inspect response body state");
        return promise;
      }
      continue;
    }

    if (buffer) {
      // If the response has body, read the all data and create
      // the blob handle and dispatch the put batch asynchronously.
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsBlobHandle(
          responses[i]->InternalMIMEType());
      buffer->StartLoading(loader,
                           MakeGarbageCollected<BlobHandleCallbackForPut>(
                               i, barrier_callback, requests[i], responses[i]),
                           exception_state);
      if (exception_state.HadException()) {
        barrier_callback->OnError("Could not inspect response body state");
        return promise;
      }
      continue;
    }

    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = requests[i]->CreateFetchAPIRequest();
    batch_operation->response = responses[i]->PopulateFetchAPIResponse();
    barrier_callback->OnSuccess(i, std::move(batch_operation));
  }

  return promise;
}

ScriptPromise Cache::KeysImpl(ScriptState* script_state,
                              const Request* request,
                              const CacheQueryOptions* options) {
  mojom::blink::FetchAPIRequestPtr fetch_api_request;
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (request) {
    fetch_api_request = request->CreateFetchAPIRequest();

    if (request->method() != http_names::kGET && !options->ignoreMethod()) {
      resolver->Resolve(HeapVector<Member<Response>>());
      return promise;
    }
  }

  // Make sure to bind the Cache object to keep the mojo interface pointer
  // alive during the operation.  Otherwise GC might prevent the callback
  // from ever being executed.
  cache_ptr_->Keys(
      std::move(fetch_api_request),
      mojom::blink::CacheQueryOptions::From(options),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, Cache* _,
             mojom::blink::CacheKeysResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Keys",
                                     base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              resolver->Reject(
                  CacheStorageError::CreateException(result->get_status()));
            } else {
              ScriptState::Scope scope(resolver->GetScriptState());
              HeapVector<Member<Request>> requests;
              requests.ReserveInitialCapacity(result->get_keys().size());
              for (auto& request : result->get_keys()) {
                requests.push_back(
                    Request::Create(resolver->GetScriptState(), *request));
              }
              resolver->Resolve(requests);
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), WrapPersistent(this)));
  return promise;
}

}  // namespace blink
