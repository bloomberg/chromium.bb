// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Cache.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"

namespace blink {

namespace {

ScriptPromise rejectAsNotImplemented(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();
    resolver->reject("not implemented");

    return promise;
}

}

PassRefPtrWillBeRawPtr<Cache> Cache::fromWebServiceWorkerCache(WebServiceWorkerCache* webCache)
{
    return adoptRefWillBeNoop(new Cache(webCache));
}

// FIXME: Implement these methods.
ScriptPromise Cache::match(ScriptState* scriptState, Request* request, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::match(ScriptState* scriptState, const String& requestString, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, Request* request, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::matchAll(ScriptState* scriptState, const String& requestString, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::add(ScriptState* scriptState, Request* request)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::add(ScriptState* scriptState, const String& requestString)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::addAll(ScriptState* scriptState, const WillBeHeapVector<ScriptValue>& rawRequests)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::deleteFunction(ScriptState* scriptState, Request* request, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::deleteFunction(ScriptState* scriptState, const String& requestString, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::put(ScriptState* scriptState, Request* request, Response*)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::put(ScriptState* scriptState, const String& requestString, Response*)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::keys(ScriptState* scriptState)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::keys(ScriptState* scriptState, Request* request, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

ScriptPromise Cache::keys(ScriptState* scriptState, const String& requestString, const Dictionary& queryParams)
{
    return rejectAsNotImplemented(scriptState);
}

Cache::Cache(WebServiceWorkerCache* webCache) : m_webCache(webCache)
{
    ScriptWrappable::init(this);
}

} // namespace blink
