// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_trace_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
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

namespace {

bool IsCacheStorageAllowed(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (auto* document = DynamicTo<Document>(context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return false;
    if (auto* settings_client = frame->GetContentSettingsClient()) {
      // This triggers a sync IPC.
      return settings_client->AllowCacheStorage(
          WebSecurityOrigin(context->GetSecurityOrigin()));
    }
    return true;
  }

  WorkerGlobalScope& worker_global = *To<WorkerGlobalScope>(context);
  // This triggers a sync IPC.
  return WorkerContentSettingsClient::From(worker_global)->AllowCacheStorage();
}

}  // namespace

ScriptPromise CacheStorage::open(ScriptState* script_state,
                                 const String& cache_name) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Open",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsAllowed(script_state)) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
    return promise;
  }

  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Open(
      cache_name, trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             GlobalFetch::ScopedFetcher* fetcher, base::TimeTicks start_time,
             int64_t trace_id, mojom::blink::OpenResultPtr result) {
            UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Open",
                                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed()) {
              return;
            }
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::Open::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
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
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::Open::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  "success");
              // See https://bit.ly/2S0zRAS for task types.
              resolver->Resolve(MakeGarbageCollected<Cache>(
                  fetcher, std::move(result->get_cache()),
                  resolver->GetExecutionContext()->GetTaskRunner(
                      blink::TaskType::kMiscPlatformAPI)));
            }
          },
          WrapPersistent(resolver), WrapPersistent(scoped_fetcher_.Get()),
          base::TimeTicks::Now(), trace_id));

  return promise;
}

ScriptPromise CacheStorage::has(ScriptState* script_state,
                                const String& cache_name) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Has",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsAllowed(script_state)) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
    return promise;
  }

  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Has(
      cache_name, trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             int64_t trace_id, mojom::blink::CacheStorageError result) {
            UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Has",
                                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Has::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(result));
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
          WrapPersistent(resolver), base::TimeTicks::Now(), trace_id));

  return promise;
}

ScriptPromise CacheStorage::Delete(ScriptState* script_state,
                                   const String& cache_name) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Delete",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsAllowed(script_state)) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
    return promise;
  }

  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Delete(
      cache_name, trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             int64_t trace_id, mojom::blink::CacheStorageError result) {
            UMA_HISTOGRAM_TIMES(
                "ServiceWorkerCache.CacheStorage.Renderer.Delete",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Delete::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(result));
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
          WrapPersistent(resolver), base::TimeTicks::Now(), trace_id));

  return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* script_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "CacheStorage::Keys",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsAllowed(script_state)) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
    return promise;
  }

  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Keys(
      trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             int64_t trace_id, const Vector<String>& keys) {
            UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Renderer.Keys",
                                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Keys::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "key_list",
                CacheStorageTracedValue(keys));
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            resolver->Resolve(keys);
          },
          WrapPersistent(resolver), base::TimeTicks::Now(), trace_id));

  return promise;
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
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  mojom::blink::FetchAPIRequestPtr mojo_request =
      request->CreateFetchAPIRequest();
  mojom::blink::MultiCacheQueryOptionsPtr mojo_options =
      mojom::blink::MultiCacheQueryOptions::From(options);
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "CacheStorage::MatchImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(mojo_request),
                         "options", CacheStorageTracedValue(mojo_options));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (!IsAllowed(script_state)) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
    return promise;
  }

  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_ptr_->Match(
      std::move(mojo_request), std::move(mojo_options), trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const MultiCacheQueryOptions* options, int64_t trace_id,
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
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::MatchImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
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
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::MatchImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                  "response", CacheStorageTracedValue(result->get_response()));
              ScriptState::Scope scope(resolver->GetScriptState());
              resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                 *result->get_response()));
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), trace_id));

  return promise;
}

CacheStorage::CacheStorage(ExecutionContext* context,
                           GlobalFetch::ScopedFetcher* fetcher)
    : ContextClient(context), scoped_fetcher_(fetcher), ever_used_(false) {
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

bool CacheStorage::HasPendingActivity() const {
  // Once the CacheStorage has been used once we keep it alive until the
  // context goes away.  This allows us to use the existence of this
  // context as a hint to optimizations such as keeping backend disk_caches
  // open in the browser process.
  //
  // Note, this also keeps the CacheStorage alive during active Cache and
  // CacheStorage operations.
  return ever_used_;
}

void CacheStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

bool CacheStorage::IsAllowed(ScriptState* script_state) {
  if (!allowed_.has_value()) {
    // Cache the IsCacheStorageAllowed() because it triggers a sync IPC.
    allowed_.emplace(IsCacheStorageAllowed(script_state));
  }
  return allowed_.value();
}

}  // namespace blink
