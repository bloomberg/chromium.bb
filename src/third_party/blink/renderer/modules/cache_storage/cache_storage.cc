// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace mojo {

using blink::mojom::blink::CacheQueryOptions;
using blink::mojom::blink::CacheQueryOptionsPtr;
using blink::mojom::blink::MultiCacheQueryOptions;
using blink::mojom::blink::MultiCacheQueryOptionsPtr;

template <>
struct TypeConverter<MultiCacheQueryOptionsPtr,
                     const blink::MultiCacheQueryOptions*> {
  static MultiCacheQueryOptionsPtr Convert(
      const blink::MultiCacheQueryOptions* input) {
    CacheQueryOptionsPtr query_options = CacheQueryOptions::New();
    query_options->ignore_search = input->ignoreSearch();
    query_options->ignore_method = input->ignoreMethod();
    query_options->ignore_vary = input->ignoreVary();

    MultiCacheQueryOptionsPtr output = MultiCacheQueryOptions::New();
    output->query_options = std::move(query_options);
    output->cache_name = input->cacheName();
    return output;
  }
};

}  // namespace mojo

namespace blink {

CacheStorage* CacheStorage::Create(ExecutionContext* context,
                                   GlobalFetch::ScopedFetcher* fetcher) {
  return MakeGarbageCollected<CacheStorage>(context, fetcher);
}

ScriptPromise CacheStorage::open(ScriptState* script_state,
                                 const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Open(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             GlobalFetch::ScopedFetcher* fetcher, base::TimeTicks start_time,
             CacheStorage* cache_storage, mojom::blink::OpenResultPtr result) {
            UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Open",
                                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed()) {
              return;
            }
            if (result->is_status()) {
              switch (result->get_status()) {
                case mojom::blink::CacheStorageError::kErrorNotFound:
                case mojom::blink::CacheStorageError::kErrorStorage:
                  resolver->Resolve();
                  break;
                default:
                  resolver->Reject(
                      CacheStorageError::CreateException(result->get_status()));
                  break;
              }
            } else {
              // See https://bit.ly/2S0zRAS for task types.
              resolver->Resolve(Cache::Create(
                  fetcher, cache_storage, std::move(result->get_cache()),
                  resolver->GetExecutionContext()->GetTaskRunner(
                      blink::TaskType::kMiscPlatformAPI)));
            }
          },
          WrapPersistent(resolver), WrapPersistent(scoped_fetcher_.Get()),
          TimeTicks::Now(), WrapPersistent(this)));

  return resolver->Promise();
}

ScriptPromise CacheStorage::has(ScriptState* script_state,
                                const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Has(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             CacheStorage* _, mojom::blink::CacheStorageError result) {
            UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Has",
                                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            switch (result) {
              case mojom::blink::CacheStorageError::kSuccess:
                resolver->Resolve(true);
                break;
              case mojom::blink::CacheStorageError::kErrorNotFound:
                resolver->Resolve(false);
                break;
              default:
                resolver->Reject(CacheStorageError::CreateException(result));
                break;
            }
          },
          WrapPersistent(resolver), TimeTicks::Now(), WrapPersistent(this)));

  return resolver->Promise();
}

ScriptPromise CacheStorage::Delete(ScriptState* script_state,
                                   const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Delete(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             CacheStorage* _, mojom::blink::CacheStorageError result) {
            UMA_HISTOGRAM_TIMES(
                "ServiceWorkerCache.CacheStorage.Renderer.Delete",
                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            switch (result) {
              case mojom::blink::CacheStorageError::kSuccess:
                resolver->Resolve(true);
                break;
              case mojom::blink::CacheStorageError::kErrorStorage:
              case mojom::blink::CacheStorageError::kErrorNotFound:
                resolver->Resolve(false);
                break;
              default:
                resolver->Reject(CacheStorageError::CreateException(result));
                break;
            }
          },
          WrapPersistent(resolver), TimeTicks::Now(), WrapPersistent(this)));

  return resolver->Promise();
}

ScriptPromise CacheStorage::keys(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Keys(WTF::Bind(
      [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
         CacheStorage* _, const Vector<String>& keys) {
        UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Keys",
                            base::TimeTicks::Now() - start_time);
        if (!resolver->GetExecutionContext() ||
            resolver->GetExecutionContext()->IsContextDestroyed())
          return;
        resolver->Resolve(keys);
      },
      WrapPersistent(resolver), TimeTicks::Now(), WrapPersistent(this)));

  return resolver->Promise();
}

ScriptPromise CacheStorage::match(ScriptState* script_state,
                                  const RequestInfo& request,
                                  const MultiCacheQueryOptions* options,
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

ScriptPromise CacheStorage::MatchImpl(ScriptState* script_state,
                                      const Request* request,
                                      const MultiCacheQueryOptions* options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Match(
      request->CreateFetchAPIRequest(),
      mojom::blink::MultiCacheQueryOptions::From(options),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const MultiCacheQueryOptions* options, CacheStorage* _,
             mojom::blink::MatchResultPtr result) {
            base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
            if (!options->hasCacheName() || options->cacheName().IsEmpty()) {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.CacheStorage.Renderer.MatchAllCaches",
                  elapsed);
            } else {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.CacheStorage.Renderer.MatchOneCache",
                  elapsed);
            }
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              switch (result->get_status()) {
                case mojom::CacheStorageError::kErrorNotFound:
                case mojom::CacheStorageError::kErrorStorage:
                case mojom::CacheStorageError::kErrorCacheNameNotFound:
                  resolver->Resolve();
                  break;
                default:
                  resolver->Reject(
                      CacheStorageError::CreateException(result->get_status()));
                  break;
              }
            } else {
              ScriptState::Scope scope(resolver->GetScriptState());
              resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                 *result->get_response()));
            }
          },
          WrapPersistent(resolver), TimeTicks::Now(), WrapPersistent(options),
          WrapPersistent(this)));

  return promise;
}

CacheStorage::CacheStorage(ExecutionContext* context,
                           GlobalFetch::ScopedFetcher* fetcher)
    : scoped_fetcher_(fetcher) {
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI);

  // Service workers may already have a CacheStoragePtr provided as an
  // optimization.
  if (auto* service_worker = DynamicTo<ServiceWorkerGlobalScope>(context)) {
    mojom::blink::CacheStoragePtrInfo info = service_worker->TakeCacheStorage();
    if (info) {
      cache_storage_ptr_ = RevocableInterfacePtr<mojom::blink::CacheStorage>(
          std::move(info), context->GetInterfaceInvalidator(), task_runner);
      return;
    }
  }

  context->GetInterfaceProvider()->GetInterface(MakeRequest(
      &cache_storage_ptr_, context->GetInterfaceInvalidator(), task_runner));
}

CacheStorage::~CacheStorage() = default;

void CacheStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
