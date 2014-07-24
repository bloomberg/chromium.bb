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

namespace WebCore {

// See https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#cache-storage

class CacheStorage FINAL : public RefCountedWillBeGarbageCollected<CacheStorage>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CacheStorage);
public:
    static PassRefPtrWillBeRawPtr<CacheStorage> create();

    ScriptPromise createFunction(ScriptState*, const String& key);
    ScriptPromise rename(ScriptState*, const String& oldKey, const String& newKey);
    ScriptPromise get(ScriptState*, const String& key);
    ScriptPromise deleteFunction(ScriptState*, const String& key);
    ScriptPromise keys(ScriptState*);

    void trace(Visitor*) { }

private:
    CacheStorage();
};

} // namespace WebCore

#endif // CacheStorage_h
