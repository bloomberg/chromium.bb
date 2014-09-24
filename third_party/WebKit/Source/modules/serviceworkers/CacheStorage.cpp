// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/CacheStorage.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/Cache.h"
#include "public/platform/WebServiceWorkerCacheError.h"
#include "public/platform/WebServiceWorkerCacheStorage.h"

namespace blink {

namespace {

PassRefPtrWillBeRawPtr<DOMException> createNoImplementationException()
{
    return DOMException::create(NotSupportedError, "No CacheStorage implementation provided.");
}

}

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::Callbacks FINAL : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
    WTF_MAKE_NONCOPYABLE(Callbacks);
public:
    explicit Callbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }
    virtual ~Callbacks() { }

    virtual void onSuccess() OVERRIDE
    {
        m_resolver->resolve(true);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve(false);
        else
            m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::WithCacheCallbacks FINAL : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
    WTF_MAKE_NONCOPYABLE(WithCacheCallbacks);
public:
    WithCacheCallbacks(const String& cacheName, CacheStorage* cacheStorage, PassRefPtr<ScriptPromiseResolver> resolver)
        : m_cacheName(cacheName), m_cacheStorage(cacheStorage), m_resolver(resolver) { }
    virtual ~WithCacheCallbacks() { }

    virtual void onSuccess(WebServiceWorkerCache* webCache) OVERRIDE
    {
        // FIXME: Remove this once content's WebServiceWorkerCache implementation has landed.
        if (!webCache) {
            m_resolver->reject("not implemented");
            return;
        }
        Cache* cache = Cache::create(webCache);
        m_cacheStorage->m_nameToCacheMap.set(m_cacheName, cache);
        m_resolver->resolve(cache);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve();
        else
            m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    String m_cacheName;
    Persistent<CacheStorage> m_cacheStorage;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::DeleteCallbacks FINAL : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
    WTF_MAKE_NONCOPYABLE(DeleteCallbacks);
public:
    DeleteCallbacks(const String& cacheName, CacheStorage* cacheStorage, PassRefPtr<ScriptPromiseResolver> resolver)
        : m_cacheName(cacheName), m_cacheStorage(cacheStorage), m_resolver(resolver) { }
    virtual ~DeleteCallbacks() { }

    virtual void onSuccess() OVERRIDE
    {
        m_cacheStorage->m_nameToCacheMap.remove(m_cacheName);
        m_resolver->resolve(true);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        if (*reason == WebServiceWorkerCacheErrorNotFound)
            m_resolver->resolve(false);
        else
            m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    String m_cacheName;
    Persistent<CacheStorage> m_cacheStorage;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::KeysCallbacks FINAL : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
    WTF_MAKE_NONCOPYABLE(KeysCallbacks);
public:
    explicit KeysCallbacks(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }
    virtual ~KeysCallbacks() { }

    virtual void onSuccess(WebVector<WebString>* keys) OVERRIDE
    {
        Vector<String> wtfKeys;
        for (size_t i = 0; i < keys->size(); ++i)
            wtfKeys.append((*keys)[i]);
        m_resolver->resolve(wtfKeys);
        m_resolver.clear();
    }

    virtual void onError(WebServiceWorkerCacheError* reason) OVERRIDE
    {
        m_resolver->reject(Cache::domExceptionForCacheError(*reason));
        m_resolver.clear();
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
};

CacheStorage* CacheStorage::create(WebServiceWorkerCacheStorage* webCacheStorage)
{
    return new CacheStorage(webCacheStorage);
}

ScriptPromise CacheStorage::get(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_nameToCacheMap.contains(cacheName)) {
        Cache* cache = m_nameToCacheMap.find(cacheName)->value;
        resolver->resolve(cache);
        return promise;
    }

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchGet(new WithCacheCallbacks(cacheName, this, resolver), cacheName);
    else
        resolver->reject(createNoImplementationException());

    return promise;
}

ScriptPromise CacheStorage::has(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_nameToCacheMap.contains(cacheName)) {
        resolver->resolve(true);
        return promise;
    }

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchHas(new Callbacks(resolver), cacheName);
    else
        resolver->reject(createNoImplementationException());

    return promise;
}

ScriptPromise CacheStorage::createFunction(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchCreate(new WithCacheCallbacks(cacheName, this, resolver), cacheName);
    else
        resolver->reject(createNoImplementationException());

    return promise;
}

ScriptPromise CacheStorage::deleteFunction(ScriptState* scriptState, const String& cacheName)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchDelete(new DeleteCallbacks(cacheName, this, resolver), cacheName);
    else
        resolver->reject(createNoImplementationException());

    return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    const ScriptPromise promise = resolver->promise();

    if (m_webCacheStorage)
        m_webCacheStorage->dispatchKeys(new KeysCallbacks(resolver));
    else
        resolver->reject(createNoImplementationException());

    return promise;
}

void CacheStorage::trace(Visitor* visitor)
{
    visitor->trace(m_nameToCacheMap);
}

CacheStorage::CacheStorage(WebServiceWorkerCacheStorage* webCacheStorage)
    : m_webCacheStorage(webCacheStorage)
{
}

} // namespace blink
