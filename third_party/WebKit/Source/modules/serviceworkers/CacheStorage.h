// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CacheStorage_h
#define CacheStorage_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Cache;
class WebServiceWorkerCacheStorage;

class CacheStorage final : public GarbageCollected<CacheStorage>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(CacheStorage);
public:
    static CacheStorage* create(WebServiceWorkerCacheStorage*);

    ScriptPromise get(ScriptState*, const String& cacheName);
    ScriptPromise has(ScriptState*, const String& cacheName);
    ScriptPromise createFunction(ScriptState*, const String& cacheName);
    ScriptPromise deleteFunction(ScriptState*, const String& cacheName);
    ScriptPromise keys(ScriptState*);

    void trace(Visitor*);

private:
    class Callbacks;
    class WithCacheCallbacks;
    class DeleteCallbacks;
    class KeysCallbacks;

    friend class WithCacheCallbacks;
    friend class DeleteCallbacks;

    explicit CacheStorage(WebServiceWorkerCacheStorage*);

    WebServiceWorkerCacheStorage* m_webCacheStorage;
    HeapHashMap<String, Member<Cache> > m_nameToCacheMap;
};

} // namespace blink

#endif // CacheStorage_h
