// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cache_query_options.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

using blink::mojom::blink::CacheQueryOptions;
using blink::mojom::blink::CacheQueryOptionsPtr;

template <>
struct TypeConverter<CacheQueryOptionsPtr, const blink::CacheQueryOptions*> {
  static CacheQueryOptionsPtr Convert(const blink::CacheQueryOptions* input) {
    CacheQueryOptionsPtr output = CacheQueryOptions::New();
    output->ignore_search = input->ignoreSearch();
    output->ignore_method = input->ignoreMethod();
    output->ignore_vary = input->ignoreVary();
    return output;
  }
};

}  // namespace mojo

namespace blink {

class AbortController;
class CacheStorageBlobClientList;
class ExceptionState;
class Response;
class Request;
class ScriptPromiseResolver;
class ScriptState;

typedef RequestOrUSVString RequestInfo;

class MODULES_EXPORT Cache : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Cache(GlobalFetch::ScopedFetcher*,
        CacheStorageBlobClientList* blob_client_list,
        mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>,
        scoped_refptr<base::SingleThreadTaskRunner>);

  // From Cache.idl:
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise match(ScriptState* script_state,
                      const V8RequestInfo* request,
                      const CacheQueryOptions* options,
                      ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise match(ScriptState*,
                      const RequestInfo&,
                      const CacheQueryOptions*,
                      ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise matchAll(ScriptState*, ExceptionState&);
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise matchAll(ScriptState* script_state,
                         const V8RequestInfo* request,
                         const CacheQueryOptions* options,
                         ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise matchAll(ScriptState*,
                         const RequestInfo&,
                         const CacheQueryOptions*,
                         ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise add(ScriptState* script_state,
                    const V8RequestInfo* request,
                    ExceptionState& exception_state);
  ScriptPromise addAll(ScriptState* script_state,
                       const HeapVector<Member<V8RequestInfo>>& requests,
                       ExceptionState& exception_state);
  ScriptPromise Delete(ScriptState* script_state,
                       const V8RequestInfo* request,
                       const CacheQueryOptions* options,
                       ExceptionState& exception_state);
  ScriptPromise put(ScriptState* script_state,
                    const V8RequestInfo* request,
                    Response* response,
                    ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise add(ScriptState*, const RequestInfo&, ExceptionState&);
  ScriptPromise addAll(ScriptState*,
                       const HeapVector<RequestInfo>&,
                       ExceptionState&);
  ScriptPromise Delete(ScriptState*,
                       const RequestInfo&,
                       const CacheQueryOptions*,
                       ExceptionState&);
  ScriptPromise put(ScriptState*,
                    const RequestInfo&,
                    Response*,
                    ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise keys(ScriptState*, ExceptionState&);
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise keys(ScriptState* script_state,
                     const V8RequestInfo* request,
                     const CacheQueryOptions* options,
                     ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise keys(ScriptState*,
                     const RequestInfo&,
                     const CacheQueryOptions*,
                     ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  void Trace(Visitor*) const override;

 protected:
  // Virtual for testing.
  virtual AbortController* CreateAbortController(ExecutionContext* context);

 private:
  class BarrierCallbackForPutResponse;
  class BarrierCallbackForPutComplete;
  class CodeCacheHandleCallbackForPut;
  class ResponseBodyLoader;
  class FetchHandler;

  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const CacheQueryOptions*);
  ScriptPromise MatchAllImpl(ScriptState*,
                             const Request*,
                             const CacheQueryOptions*);
  ScriptPromise AddAllImpl(ScriptState*,
                           const String& method_name,
                           const HeapVector<Member<Request>>&,
                           ExceptionState&);
  ScriptPromise DeleteImpl(ScriptState*,
                           const Request*,
                           const CacheQueryOptions*);
  void PutImpl(ScriptPromiseResolver*,
               const String& method_name,
               const HeapVector<Member<Request>>&,
               const HeapVector<Member<Response>>&,
               const WTF::Vector<scoped_refptr<BlobDataHandle>>& blob_list,
               ExceptionState&,
               int64_t trace_id);
  ScriptPromise KeysImpl(ScriptState*,
                         const Request*,
                         const CacheQueryOptions*);

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  Member<CacheStorageBlobClientList> blob_client_list_;

  mojo::AssociatedRemote<mojom::blink::CacheStorageCache> cache_remote_;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_H_
