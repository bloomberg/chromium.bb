// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/CacheStorage.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<CacheStorage> CacheStorage::create()
{
    return adoptRefWillBeNoop(new CacheStorage());
}

// FIXME: Implement every one of these methods.
ScriptPromise CacheStorage::createFunction(ScriptState* scriptState, const String& key)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    resolver->reject("not implemented");

    return promise;
}

ScriptPromise CacheStorage::rename(ScriptState* scriptState, const String& oldKey, const String& newKey)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    resolver->reject("not implemented");

    return promise;
}

ScriptPromise CacheStorage::get(ScriptState* scriptState, const String& key)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    resolver->reject("not implemented");

    return promise;
}

ScriptPromise CacheStorage::deleteFunction(ScriptState* scriptState, const String& key)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    resolver->reject("not implemented");

    return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    resolver->reject("not implemented");

    return promise;
}

CacheStorage::CacheStorage()
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
