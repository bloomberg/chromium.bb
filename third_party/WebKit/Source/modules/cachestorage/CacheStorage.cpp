// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/CacheStorage.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/cachestorage/CacheStorageError.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "platform/bindings/ScriptState.h"
#include "platform/http_names.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"

namespace blink {

namespace {

DOMException* CreateNoImplementationException() {
  return DOMException::Create(kNotSupportedError,
                              "No CacheStorage implementation provided.");
}

bool CommonChecks(ScriptState* script_state, ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return false;

  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    exception_state.ThrowSecurityError(error_message);
    return false;
  }
  return true;
}

}  // namespace

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::Callbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(Callbacks);

 public:
  explicit Callbacks(ScriptPromiseResolver* resolver) : resolver_(resolver) {}
  ~Callbacks() override {}

  void OnSuccess() override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Resolve(true);
    resolver_.Clear();
  }

  void OnError(WebServiceWorkerCacheError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == kWebServiceWorkerCacheErrorNotFound)
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
  ~WithCacheCallbacks() override {}

  void OnSuccess(std::unique_ptr<WebServiceWorkerCache> web_cache) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    Cache* cache = Cache::Create(cache_storage_->scoped_fetcher_,
                                 WTF::WrapUnique(web_cache.release()));
    resolver_->Resolve(cache);
    resolver_.Clear();
  }

  void OnError(WebServiceWorkerCacheError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == kWebServiceWorkerCacheErrorNotFound)
      resolver_->Resolve();
    else
      resolver_->Reject(CacheStorageError::CreateException(reason));
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

  void OnError(WebServiceWorkerCacheError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == kWebServiceWorkerCacheErrorNotFound ||
        reason == kWebServiceWorkerCacheErrorCacheNameNotFound)
      resolver_->Resolve();
    else
      resolver_->Reject(CacheStorageError::CreateException(reason));
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
  ~DeleteCallbacks() override {}

  void OnSuccess() override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Resolve(true);
    resolver_.Clear();
  }

  void OnError(WebServiceWorkerCacheError reason) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (reason == kWebServiceWorkerCacheErrorNotFound)
      resolver_->Resolve(false);
    else
      resolver_->Reject(CacheStorageError::CreateException(reason));
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
  ~KeysCallbacks() override {}

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

  void OnError(WebServiceWorkerCacheError reason) override {
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
                                 const String& cache_name,
                                 ExceptionState& exception_state) {
  if (!CommonChecks(script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchOpen(
        WTF::MakeUnique<WithCacheCallbacks>(cache_name, this, resolver),
        cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::has(ScriptState* script_state,
                                const String& cache_name,
                                ExceptionState& exception_state) {
  if (!CommonChecks(script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchHas(WTF::MakeUnique<Callbacks>(resolver),
                                    cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::deleteFunction(ScriptState* script_state,
                                           const String& cache_name,
                                           ExceptionState& exception_state) {
  if (!CommonChecks(script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_) {
    web_cache_storage_->DispatchDelete(
        WTF::MakeUnique<DeleteCallbacks>(cache_name, this, resolver),
        cache_name);
  } else {
    resolver->Reject(CreateNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* script_state,
                                 ExceptionState& exception_state) {
  if (!CommonChecks(script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (web_cache_storage_)
    web_cache_storage_->DispatchKeys(WTF::MakeUnique<KeysCallbacks>(resolver));
  else
    resolver->Reject(CreateNoImplementationException());

  return promise;
}

ScriptPromise CacheStorage::match(ScriptState* script_state,
                                  const RequestInfo& request,
                                  const CacheQueryOptions& options,
                                  ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (!CommonChecks(script_state, exception_state))
    return ScriptPromise();

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

  if (web_cache_storage_)
    web_cache_storage_->DispatchMatch(WTF::MakeUnique<MatchCallbacks>(resolver),
                                      web_request,
                                      Cache::ToWebQueryParams(options));
  else
    resolver->Reject(CreateNoImplementationException());

  return promise;
}

CacheStorage::CacheStorage(
    GlobalFetch::ScopedFetcher* fetcher,
    std::unique_ptr<WebServiceWorkerCacheStorage> web_cache_storage)
    : scoped_fetcher_(fetcher),
      web_cache_storage_(std::move(web_cache_storage)) {}

CacheStorage::~CacheStorage() {}

void CacheStorage::Dispose() {
  web_cache_storage_.reset();
}

DEFINE_TRACE(CacheStorage) {
  visitor->Trace(scoped_fetcher_);
}

}  // namespace blink
