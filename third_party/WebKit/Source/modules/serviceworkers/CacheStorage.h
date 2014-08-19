// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CacheStorage_h
#define CacheStorage_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"

namespace blink {

class WebServiceWorkerCacheStorage;

class CacheStorage FINAL : public RefCountedWillBeGarbageCollected<CacheStorage>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CacheStorage);
public:
    static PassRefPtrWillBeRawPtr<CacheStorage> create(WebServiceWorkerCacheStorage*);

    ScriptPromise get(ScriptState*, const String& cacheName);
    ScriptPromise has(ScriptState*, const String& cacheName);
    ScriptPromise createFunction(ScriptState*, const String& cacheName);
    ScriptPromise deleteFunction(ScriptState*, const String& cacheName);
    ScriptPromise keys(ScriptState*);

    void trace(Visitor*) { }

private:
    explicit CacheStorage(WebServiceWorkerCacheStorage*);

    WebServiceWorkerCacheStorage* m_webCacheStorage;
};

} // namespace blink

#endif // CacheStorage_h
