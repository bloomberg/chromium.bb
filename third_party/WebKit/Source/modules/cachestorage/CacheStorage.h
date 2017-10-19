// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CacheStorage_h
#define CacheStorage_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "modules/cachestorage/Cache.h"
#include "modules/cachestorage/CacheQueryOptions.h"
#include "modules/fetch/GlobalFetch.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"

namespace blink {

class Cache;
class WebServiceWorkerCacheStorage;

class CacheStorage final : public GarbageCollectedFinalized<CacheStorage>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(CacheStorage);

 public:
  static CacheStorage* Create(GlobalFetch::ScopedFetcher*,
                              std::unique_ptr<WebServiceWorkerCacheStorage>);
  ~CacheStorage();
  void Dispose();

  ScriptPromise open(ScriptState*, const String& cache_name, ExceptionState&);
  ScriptPromise has(ScriptState*, const String& cache_name, ExceptionState&);
  ScriptPromise deleteFunction(ScriptState*,
                               const String& cache_name,
                               ExceptionState&);
  ScriptPromise keys(ScriptState*, ExceptionState&);
  ScriptPromise match(ScriptState*,
                      const RequestInfo&,
                      const CacheQueryOptions&,
                      ExceptionState&);

  void Trace(blink::Visitor*);

 private:
  class Callbacks;
  class WithCacheCallbacks;
  class DeleteCallbacks;
  class KeysCallbacks;
  class MatchCallbacks;

  friend class WithCacheCallbacks;
  friend class DeleteCallbacks;

  CacheStorage(GlobalFetch::ScopedFetcher*,
               std::unique_ptr<WebServiceWorkerCacheStorage>);
  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const CacheQueryOptions&);

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  std::unique_ptr<WebServiceWorkerCacheStorage> web_cache_storage_;
};

}  // namespace blink

#endif  // CacheStorage_h
