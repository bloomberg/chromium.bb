// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/CacheStorage.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "public/platform/WebServiceWorkerCacheStorage.h"

namespace blink {

namespace {

const char* CacheErrorToString(WebServiceWorkerCacheError reason)
{
    // FIXME: Construct correct DOM error objects rather than returning strings.
    switch (reason) {
    case WebServiceWorkerCacheError::WebServiceWorkerCacheErrorNotImplemented:
        return "not implemented";
    case WebServiceWorkerCacheError::WebServiceWorkerCacheErrorNotFound:
        return "not found";
    case WebServiceWorkerCacheError::WebServiceWorkerCacheErrorExists:
        return "entry already exists";
    default:
        ASSERT_NOT_REACHED();
        return "unknown error";
    }
}

class CacheStorageCallbacks : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheStorageCallbacks);
public:
    explicit CacheStorageCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~CacheStorageCallbacks() { }

    virtual void onSuccess() OVERRIDE
    {
        m_resolver->resolve(true);
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(CacheErrorToString(*reason));
    }

private:
    const RefPtr<ScriptPromiseResolver> m_resolver;
};

class CacheStorageWithCacheCallbacks : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheStorageWithCacheCallbacks);
public:
    explicit CacheStorageWithCacheCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~CacheStorageWithCacheCallbacks() { }

    virtual void onSuccess(WebServiceWorkerCache* cache) OVERRIDE
    {
        // FIXME: There should be a blink side of the Cache object implementation here, rather than
        // this nonsensical return.
        m_resolver->resolve("succesfully returned a cache");
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(CacheErrorToString(*reason));
    }

private:
    const RefPtr<ScriptPromiseResolver> m_resolver;
};

class CacheStorageKeysCallbacks : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
    WTF_MAKE_NONCOPYABLE(CacheStorageKeysCallbacks);
public:
    explicit CacheStorageKeysCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~CacheStorageKeysCallbacks() { }

    virtual void onSuccess(blink::WebVector<blink::WebString>* keys) OVERRIDE
    {
        Vector<String> wtfKeys;
        for (size_t i = 0; i < keys->size(); ++i)
            wtfKeys.append((*keys)[i]);
        m_resolver->resolve(wtfKeys);
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(CacheErrorToString(*reason));
    }

private:
    const RefPtr<ScriptPromiseResolver> m_resolver;
};

}

PassRefPtrWillBeRawPtr<CacheStorage> CacheStorage::create(WebServiceWorkerCacheStorage* webCacheStorage)
{
    return adoptRefWillBeNoop(new CacheStorage(webCacheStorage));
}

ScriptPromise CacheStorage::get(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchGet(new CacheStorageWithCacheCallbacks(resolver), cacheName);
    else
        resolver->reject("no implementation provided");

    return promise;
}

ScriptPromise CacheStorage::has(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchHas(new CacheStorageCallbacks(resolver), cacheName);
    else
        resolver->reject("no implementation provided");

    return promise;
}

ScriptPromise CacheStorage::createFunction(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchCreate(new CacheStorageWithCacheCallbacks(resolver), cacheName);
    else
        resolver->reject("no implementation provided");

    return promise;
}

ScriptPromise CacheStorage::deleteFunction(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchDelete(new CacheStorageCallbacks(resolver), cacheName);
    else
        resolver->reject("no implementation provided");

    return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchKeys(new CacheStorageKeysCallbacks(resolver));
    else
        resolver->reject("no implementation provided");

    return promise;
}

CacheStorage::CacheStorage(WebServiceWorkerCacheStorage* webCacheStorage) : m_webCacheStorage(webCacheStorage)
{
    ScriptWrappable::init(this);
}

} // namespace blink
