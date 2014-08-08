// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Cache_h
#define Cache_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class Dictionary;
class Response;
class Request;
class ScriptState;
class WebServiceWorkerCache;

class Cache FINAL : public RefCountedWillBeGarbageCollected<Cache>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(Cache);
public:
    static PassRefPtrWillBeRawPtr<Cache> fromWebServiceWorkerCache(WebServiceWorkerCache*);

    // From Cache.idl:
    ScriptPromise match(ScriptState*, Request*, const Dictionary& queryParams);
    ScriptPromise match(ScriptState*, const String&, const Dictionary& queryParams);
    ScriptPromise matchAll(ScriptState*, Request*, const Dictionary& queryParams);
    ScriptPromise matchAll(ScriptState*, const String&, const Dictionary& queryParams);
    ScriptPromise add(ScriptState*, Request*);
    ScriptPromise add(ScriptState*, const String&);
    ScriptPromise addAll(ScriptState*, const WillBeHeapVector<ScriptValue>&);
    ScriptPromise deleteFunction(ScriptState*, Request*, const Dictionary& queryParams);
    ScriptPromise deleteFunction(ScriptState*, const String&, const Dictionary& queryParams);
    ScriptPromise put(ScriptState*, Request*, Response*);
    ScriptPromise put(ScriptState*, const String&, Response*);
    ScriptPromise keys(ScriptState*);
    ScriptPromise keys(ScriptState*, Request*, const Dictionary& queryParams);
    ScriptPromise keys(ScriptState*, const String&, const Dictionary& queryParams);

    void trace(Visitor*) { }

private:
    explicit Cache(WebServiceWorkerCache* webCache);

    WebServiceWorkerCache const* ALLOW_UNUSED m_webCache;
};

} // namespace blink

#endif // Cache_h
