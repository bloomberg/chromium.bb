/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/blob/BlobRegistry.h"

#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginCache.h"
#include "public/platform/Platform.h"
#include "public/platform/WebBlobData.h"
#include "public/platform/WebBlobRegistry.h"
#include "public/platform/WebString.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/MainThread.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BlobOriginCache : public SecurityOriginCache {
public:
    BlobOriginCache();
    virtual SecurityOrigin* cachedOrigin(const KURL&) override;
};

struct BlobRegistryContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BlobRegistryContext(const KURL& url, PassOwnPtr<BlobData> blobData)
        : url(url.copy())
        , blobData(blobData)
    {
        this->blobData->detachFromCurrentThread();
    }

    BlobRegistryContext(const KURL& url, const String& type)
        : url(url.copy())
        , type(type.isolatedCopy())
    {
    }

    BlobRegistryContext(const KURL& url, const KURL& srcURL)
        : url(url.copy())
        , srcURL(srcURL.copy())
    {
    }

    BlobRegistryContext(const KURL& url, PassRefPtr<RawData> streamData)
        : url(url.copy())
        , streamData(streamData)
    {
    }

    BlobRegistryContext(const KURL& url)
        : url(url.copy())
    {
    }

    KURL url;
    KURL srcURL;
    OwnPtr<BlobData> blobData;
    PassRefPtr<RawData> streamData;
    String type;
};

static WebBlobRegistry* blobRegistry()
{
    return Platform::current()->blobRegistry();
}

typedef HashMap<String, RefPtr<SecurityOrigin>> BlobURLOriginMap;
static ThreadSpecific<BlobURLOriginMap>& originMap()
{
    // We want to create the BlobOriginCache exactly once because it is shared by all the threads.
    AtomicallyInitializedStaticReference(BlobOriginCache, cache, new BlobOriginCache);
    (void)cache; // BlobOriginCache's constructor does the interesting work.

    AtomicallyInitializedStaticReference(ThreadSpecific<BlobURLOriginMap>, map, new ThreadSpecific<BlobURLOriginMap>);
    return map;
}

static void saveToOriginMap(SecurityOrigin* origin, const KURL& url)
{
    // If the blob URL contains null origin, as in the context with unique
    // security origin or file URL, save the mapping between url and origin so
    // that the origin can be retrived when doing security origin check.
    if (origin && BlobURL::getOrigin(url) == "null")
        originMap()->add(url.string(), origin);
}

static void removeFromOriginMap(const KURL& url)
{
    if (BlobURL::getOrigin(url) == "null")
        originMap()->remove(url.string());
}

void BlobRegistry::registerBlobData(const String& uuid, PassOwnPtr<BlobData> data)
{
    blobRegistry()->registerBlobData(uuid, WebBlobData(data));
}

void BlobRegistry::addBlobDataRef(const String& uuid)
{
    blobRegistry()->addBlobDataRef(uuid);
}

void BlobRegistry::removeBlobDataRef(const String& uuid)
{
    blobRegistry()->removeBlobDataRef(uuid);
}

void BlobRegistry::registerPublicBlobURL(SecurityOrigin* origin, const KURL& url, PassRefPtr<BlobDataHandle> handle)
{
    saveToOriginMap(origin, url);
    blobRegistry()->registerPublicBlobURL(url, handle->uuid());
}

void BlobRegistry::revokePublicBlobURL(const KURL& url)
{
    removeFromOriginMap(url);
    blobRegistry()->revokePublicBlobURL(url);
}

static void registerStreamURLTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->registerStreamURL(blobRegistryContext->url, blobRegistryContext->type);
}

void BlobRegistry::registerStreamURL(const KURL& url, const String& type)
{
    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->registerStreamURL(url, type);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url, type));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&registerStreamURLTask, context.release()));
    }
}

static void registerStreamURLFromTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->registerStreamURL(blobRegistryContext->url, blobRegistryContext->srcURL);
}

void BlobRegistry::registerStreamURL(SecurityOrigin* origin, const KURL& url, const KURL& srcURL)
{
    saveToOriginMap(origin, url);

    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->registerStreamURL(url, srcURL);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url, srcURL));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&registerStreamURLFromTask, context.release()));
    }
}

static void addDataToStreamTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry()) {
        RefPtr<RawData> data(blobRegistryContext->streamData);
        registry->addDataToStream(blobRegistryContext->url, data->data(), data->length());
    }
}

void BlobRegistry::addDataToStream(const KURL& url, PassRefPtr<RawData> streamData)
{
    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->addDataToStream(url, streamData->data(), streamData->length());
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url, streamData));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&addDataToStreamTask, context.release()));
    }
}

static void flushStreamTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->flushStream(blobRegistryContext->url);
}

void BlobRegistry::flushStream(const KURL& url)
{
    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->flushStream(url);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&flushStreamTask, context.release()));
    }
}

static void finalizeStreamTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->finalizeStream(blobRegistryContext->url);
}

void BlobRegistry::finalizeStream(const KURL& url)
{
    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->finalizeStream(url);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&finalizeStreamTask, context.release()));
    }
}

static void abortStreamTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->abortStream(blobRegistryContext->url);
}

void BlobRegistry::abortStream(const KURL& url)
{
    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->abortStream(url);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&abortStreamTask, context.release()));
    }
}

static void unregisterStreamURLTask(PassOwnPtr<BlobRegistryContext> blobRegistryContext)
{
    if (WebBlobRegistry* registry = blobRegistry())
        registry->unregisterStreamURL(blobRegistryContext->url);
}

void BlobRegistry::unregisterStreamURL(const KURL& url)
{
    removeFromOriginMap(url);

    if (isMainThread()) {
        if (WebBlobRegistry* registry = blobRegistry())
            registry->unregisterStreamURL(url);
    } else {
        OwnPtr<BlobRegistryContext> context = adoptPtr(new BlobRegistryContext(url));
        Platform::current()->mainThread()->postTask(FROM_HERE, bind(&unregisterStreamURLTask, context.release()));
    }
}

BlobOriginCache::BlobOriginCache()
{
    SecurityOrigin::setCache(this);
}

SecurityOrigin* BlobOriginCache::cachedOrigin(const KURL& url)
{
    if (url.protocolIs("blob"))
        return originMap()->get(url.string());
    return 0;
}

} // namespace blink
