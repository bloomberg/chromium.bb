// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Cache_h
#define Cache_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/cachestorage/CacheQueryOptions.h"
#include "modules/fetch/GlobalFetch.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"

namespace blink {

class ExceptionState;
class Response;
class Request;
class ScriptState;

typedef RequestOrUSVString RequestInfo;

class MODULES_EXPORT Cache final : public GarbageCollectedFinalized<Cache>,
                                   public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(Cache);

 public:
  static Cache* Create(GlobalFetch::ScopedFetcher*,
                       std::unique_ptr<WebServiceWorkerCache>);

  // From Cache.idl:
  ScriptPromise match(ScriptState*,
                      const RequestInfo&,
                      const CacheQueryOptions&,
                      ExceptionState&);
  ScriptPromise matchAll(ScriptState*, ExceptionState&);
  ScriptPromise matchAll(ScriptState*,
                         const RequestInfo&,
                         const CacheQueryOptions&,
                         ExceptionState&);
  ScriptPromise add(ScriptState*, const RequestInfo&, ExceptionState&);
  ScriptPromise addAll(ScriptState*,
                       const HeapVector<RequestInfo>&,
                       ExceptionState&);
  ScriptPromise deleteFunction(ScriptState*,
                               const RequestInfo&,
                               const CacheQueryOptions&,
                               ExceptionState&);
  ScriptPromise put(ScriptState*,
                    const RequestInfo&,
                    Response*,
                    ExceptionState&);
  ScriptPromise keys(ScriptState*, ExceptionState&);
  ScriptPromise keys(ScriptState*,
                     const RequestInfo&,
                     const CacheQueryOptions&,
                     ExceptionState&);

  static WebServiceWorkerCache::QueryParams ToWebQueryParams(
      const CacheQueryOptions&);

  void Trace(blink::Visitor*);

 private:
  class BarrierCallbackForPut;
  class BlobHandleCallbackForPut;
  class FetchResolvedForAdd;
  friend class FetchResolvedForAdd;
  Cache(GlobalFetch::ScopedFetcher*, std::unique_ptr<WebServiceWorkerCache>);

  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const CacheQueryOptions&);
  ScriptPromise MatchAllImpl(ScriptState*);
  ScriptPromise MatchAllImpl(ScriptState*,
                             const Request*,
                             const CacheQueryOptions&);
  ScriptPromise AddAllImpl(ScriptState*,
                           const HeapVector<Member<Request>>&,
                           ExceptionState&);
  ScriptPromise DeleteImpl(ScriptState*,
                           const Request*,
                           const CacheQueryOptions&);
  ScriptPromise PutImpl(ScriptState*,
                        const HeapVector<Member<Request>>&,
                        const HeapVector<Member<Response>>&);
  ScriptPromise KeysImpl(ScriptState*);
  ScriptPromise KeysImpl(ScriptState*,
                         const Request*,
                         const CacheQueryOptions&);

  WebServiceWorkerCache* WebCache() const;

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  std::unique_ptr<WebServiceWorkerCache> web_cache_;
};

}  // namespace blink

#endif  // Cache_h
