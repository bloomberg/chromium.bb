// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/CacheStorage.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/fetch/Request.h"
#include "core/fetch/Response.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/cachestorage/CacheStorageError.h"
#include "platform/bindings/ScriptState.h"
#include "platform/network/http_names.h"
#include "public/platform/modules/cache_storage/cache_storage.mojom-blink.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"

namespace blink {

namespace {

DOMException* CreateNoImplementationException() {
  return DOMException::Create(kNotSupportedError,
                              "No CacheStorage implementation provided.");
}

}  // namespace

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::Callbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(Callbacks);

 public:
  explicit Callbacks(ScriptPromiseResolver* resolver) : resolver_(resolver) {}
  ~Callbacks() override = default;

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
class CacheStorage::WithCacheCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(WithCacheCallbacks);

 public:
  WithCacheCallbacks(const String& cache_name,
                     CacheStorage* cache_storage,
                     ScriptPromiseResolver* resolver)
      : cache_name_(cache_name),
        cache_storage_(cache_storage),
        resolver_(resolver) {}
  ~WithCacheCallbacks() override = default;

  void OnSuccess(std::unique_ptr<WebServiceWorkerCache> web_cache) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    Cache* cache = Cache::Create(cache_storage_->scoped_fetcher_,
                                 base::WrapUnique(web_cache.release()));
    resolver_->Resolve(cache);
    resolver_.Clear();
  }

  void OnError(mojom::CacheStorageError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == mojom::CacheStorageError::kErrorNotFound ||
        reason == mojom::CacheStorageError::kErrorStorage) {
      resolver_->Resolve();
    } else {
      resolver_->Reject(CacheStorageError::CreateException(reason));
    }
    resolver_.Clear();
  }

 private:
  String cache_name_;
  Persistent<CacheStorage> cache_storage_;
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::MatchCallbacks
    : public WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(MatchCallbacks);

 public:
  explicit MatchCallbacks(ScriptPromiseResolver* resolver)
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
    if (reason == mojom::CacheStorageError::kErrorNotFound ||
        reason == mojom::CacheStorageError::kErrorStorage ||
        reason == mojom::CacheStorageError::kErrorCacheNameNotFound) {
      resolver_->Resolve();
    } else {
      resolver_->Reject(CacheStorageError::CreateException(reason));
    }
    resolver_.Clear();
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::DeleteCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCallbacks);

 public:
  DeleteCallbacks(const String& cache_name,
                  CacheStorage* cache_storage,
                  ScriptPromiseResolver* resolver)
      : cache_name_(cache_name),
        cache_storage_(cache_storage),
        resolver_(resolver) {}
  ~DeleteCallbacks() override = default;

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
    if (reason == mojom::CacheStorageError::kErrorNotFound ||
        reason == mojom::CacheStorageError::kErrorStorage) {
      resolver_->Resolve(false);
    } else {
      resolver_->Reject(CacheStorageError::CreateException(reason));
    }
    resolver_.Clear();
  }

 private:
  String cache_name_;
  Persistent<CacheStorage> cache_storage_;
  Persistent<ScriptPromiseResolver> resolver_;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::KeysCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
  WTF_MAKE_NONCOPYABLE(KeysCallbacks);

 public:
  explicit KeysCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~KeysCallbacks() override = default;

  void OnSuccess(const WebVector<WebString>& keys) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    Vector<String> wtf_keys;
    for (size_t i = 0; i < keys.size(); ++i)
      wtf_keys.push_back(keys[i]);
    resolver_->Resolve(wtf_keys);
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

CacheStorage* CacheStorage::Create(
    GlobalFetch::ScopedFetcher* fetcher,
    std::unique_ptr<WebServiceWorkerCacheStorage> web_cache_storage) {
  return new CacheStorage(fetcher, std::move(web_cache_storage));
}

ScriptPromise CacheStorage::open(ScriptState* script_state,
                                 const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchOpen(
        std::make_unique<WithCacheCallbacks>(cache_name, this, resolver),
        cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::has(ScriptState* script_state,
                                const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchHas(std::make_unique<Callbacks>(resolver),
                                    cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::Delete(ScriptState* script_state,
                                   const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchDelete(
        std::make_unique<DeleteCallbacks>(cache_name, this, resolver),
        cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_)
    web_cache_storage_->DispatchKeys(std::make_unique<KeysCallbacks>(resolver));
  else
    resolver->Reject(CreateNoImplementationException());

  return promise;
}

ScriptPromise CacheStorage::match(ScriptState* script_state,
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

ScriptPromise CacheStorage::MatchImpl(ScriptState* script_state,
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

  if (web_cache_storage_) {
    web_cache_storage_->DispatchMatch(
        std::make_unique<MatchCallbacks>(resolver), web_request,
        Cache::ToWebQueryParams(options));
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

CacheStorage::CacheStorage(
    GlobalFetch::ScopedFetcher* fetcher,
    std::unique_ptr<WebServiceWorkerCacheStorage> web_cache_storage)
    : scoped_fetcher_(fetcher),
      web_cache_storage_(std::move(web_cache_storage)) {}

CacheStorage::~CacheStorage() = default;

void CacheStorage::Dispose() {
  web_cache_storage_.reset();
}

void CacheStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
