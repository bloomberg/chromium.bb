// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Cache_h
#define Cache_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/CacheQueryOptions.h"
#include "public/platform/WebServiceWorkerCache.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class Response;
class Request;
class ScriptState;

class Cache final : public GarbageCollectedFinalized<Cache>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(Cache);
public:
    static Cache* create(WebServiceWorkerCache*);

    // From Cache.idl:
    ScriptPromise match(ScriptState*, Request*, const CacheQueryOptions&);
    ScriptPromise match(ScriptState*, const String&, const CacheQueryOptions&, ExceptionState&);
    ScriptPromise matchAll(ScriptState*, Request*, const CacheQueryOptions&);
    ScriptPromise matchAll(ScriptState*, const String&, const CacheQueryOptions&, ExceptionState&);
    ScriptPromise add(ScriptState*, Request*);
    ScriptPromise add(ScriptState*, const String&, ExceptionState&);
    ScriptPromise addAll(ScriptState*, const Vector<ScriptValue>&);
    ScriptPromise deleteFunction(ScriptState*, Request*, const CacheQueryOptions&);
    ScriptPromise deleteFunction(ScriptState*, const String&, const CacheQueryOptions&, ExceptionState&);
    ScriptPromise put(ScriptState*, Request*, Response*);
    ScriptPromise put(ScriptState*, const String&, Response*, ExceptionState&);
    ScriptPromise keys(ScriptState*);
    ScriptPromise keys(ScriptState*, Request*, const CacheQueryOptions&);
    ScriptPromise keys(ScriptState*, const String&, const CacheQueryOptions&, ExceptionState&);

    static PassRefPtrWillBeRawPtr<DOMException> domExceptionForCacheError(WebServiceWorkerCacheError);

    void trace(Visitor*) { }

private:
    explicit Cache(WebServiceWorkerCache*);

    ScriptPromise matchImpl(ScriptState*, const Request*, const CacheQueryOptions&);
    ScriptPromise matchAllImpl(ScriptState*, const Request*, const CacheQueryOptions&);
    ScriptPromise addImpl(ScriptState*, const Request*);
    ScriptPromise addAllImpl(ScriptState*, const Vector<const Request*>);
    ScriptPromise deleteImpl(ScriptState*, const Request*, const CacheQueryOptions&);
    ScriptPromise putImpl(ScriptState*, const Request*, Response*);
    ScriptPromise keysImpl(ScriptState*);
    ScriptPromise keysImpl(ScriptState*, const Request*, const CacheQueryOptions&);

    OwnPtr<WebServiceWorkerCache> m_webCache;
};

} // namespace blink

#endif // Cache_h
