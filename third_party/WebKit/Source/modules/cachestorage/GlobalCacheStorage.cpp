// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/GlobalCacheStorage.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/cachestorage/CacheStorage.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

namespace {

template <typename T>
class GlobalCacheStorageImpl final
    : public GarbageCollectedFinalized<GlobalCacheStorageImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalCacheStorageImpl);

 public:
  static GlobalCacheStorageImpl& From(T& supplementable,
                                      ExecutionContext* execution_context) {
    GlobalCacheStorageImpl* supplement = static_cast<GlobalCacheStorageImpl*>(
        Supplement<T>::From(supplementable, GetName()));
    if (!supplement) {
      supplement = new GlobalCacheStorageImpl;
      Supplement<T>::ProvideTo(supplementable, GetName(), supplement);
    }
    return *supplement;
  }

  ~GlobalCacheStorageImpl() {
    if (caches_)
      caches_->Dispose();
  }

  CacheStorage* Caches(T& fetching_scope, ExceptionState& exception_state) {
    ExecutionContext* context = fetching_scope.GetExecutionContext();
    if (!context->GetSecurityOrigin()->CanAccessCacheStorage()) {
      if (context->GetSecurityContext().IsSandboxed(kSandboxOrigin))
        exception_state.ThrowSecurityError(
            "Cache storage is disabled because the context is sandboxed and "
            "lacks the 'allow-same-origin' flag.");
      else if (context->Url().ProtocolIs("data"))
        exception_state.ThrowSecurityError(
            "Cache storage is disabled inside 'data:' URLs.");
      else
        exception_state.ThrowSecurityError(
            "Access to cache storage is denied.");
      return nullptr;
    }

    if (!caches_) {
      caches_ = CacheStorage::Create(
          GlobalFetch::ScopedFetcher::From(fetching_scope),
          Platform::Current()->CreateCacheStorage(
              WebSecurityOrigin(context->GetSecurityOrigin())));
    }
    return caches_;
  }

  // Promptly dispose of associated CacheStorage.
  EAGERLY_FINALIZE();
  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(caches_);
    Supplement<T>::Trace(visitor);
  }

 private:
  GlobalCacheStorageImpl() {}

  static const char* GetName() { return "CacheStorage"; }

  Member<CacheStorage> caches_;
};

}  // namespace

CacheStorage* GlobalCacheStorage::caches(LocalDOMWindow& window,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<LocalDOMWindow>::From(
             window, window.GetExecutionContext())
      .Caches(window, exception_state);
}

CacheStorage* GlobalCacheStorage::caches(WorkerGlobalScope& worker,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<WorkerGlobalScope>::From(
             worker, worker.GetExecutionContext())
      .Caches(worker, exception_state);
}

}  // namespace blink
