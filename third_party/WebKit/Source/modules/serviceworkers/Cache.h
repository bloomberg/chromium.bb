// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Cache_h
#define Cache_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/QueryParams.h"
#include "public/platform/WebServiceWorkerCache.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Response;
class Request;
class ScriptState;

class Cache FINAL : public GarbageCollectedFinalized<Cache>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(Cache);
public:
    static Cache* create(WebServiceWorkerCache*);

    // From Cache.idl:
    ScriptPromise match(ScriptState*, Request*, const QueryParams&);
    ScriptPromise match(ScriptState*, const String&, const QueryParams&);
    ScriptPromise matchAll(ScriptState*, Request*, const QueryParams&);
    ScriptPromise matchAll(ScriptState*, const String&, const QueryParams&);
    ScriptPromise add(ScriptState*, Request*);
    ScriptPromise add(ScriptState*, const String&);
    ScriptPromise addAll(ScriptState*, const Vector<ScriptValue>&);
    ScriptPromise deleteFunction(ScriptState*, Request*, const QueryParams&);
    ScriptPromise deleteFunction(ScriptState*, const String&, const QueryParams&);
    ScriptPromise put(ScriptState*, Request*, Response*);
    ScriptPromise put(ScriptState*, const String&, Response*);
    ScriptPromise keys(ScriptState*);
    ScriptPromise keys(ScriptState*, Request*, const QueryParams&);
    ScriptPromise keys(ScriptState*, const String&, const QueryParams&);

    static PassRefPtrWillBeRawPtr<DOMException> domExceptionForCacheError(WebServiceWorkerCacheError);

    void trace(Visitor*) { }

private:
    explicit Cache(WebServiceWorkerCache*);

    ScriptPromise matchImpl(ScriptState*, Request*, const QueryParams&);
    ScriptPromise matchAllImpl(ScriptState*, Request*, const QueryParams&);
    ScriptPromise addImpl(ScriptState*, Request*);
    ScriptPromise addAllImpl(ScriptState*, Vector<Request*>);
    ScriptPromise deleteImpl(ScriptState*, Request*, const QueryParams&);
    ScriptPromise putImpl(ScriptState*, Request*, Response*);
    ScriptPromise keysImpl(ScriptState*);
    ScriptPromise keysImpl(ScriptState*, Request*, const QueryParams&);

    OwnPtr<WebServiceWorkerCache> m_webCache;
};

} // namespace blink

#endif // Cache_h
