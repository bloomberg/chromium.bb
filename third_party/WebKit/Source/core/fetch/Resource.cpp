/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "core/fetch/Resource.h"

#include "FetchInitiatorTypeNames.h"
#include "core/fetch/CachedMetadata.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourcePtr.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/Logging.h"
#include "platform/PurgeableBuffer.h"
#include "platform/SharedBuffer.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"

using namespace WTF;

namespace WebCore {

// These response headers are not copied from a revalidated response to the
// cached response headers. For compatibility, this list is based on Chromium's
// net/http/http_response_headers.cc.
const char* const headersToIgnoreAfterRevalidation[] = {
    "allow",
    "connection",
    "etag",
    "expires",
    "keep-alive",
    "last-modified"
    "proxy-authenticate",
    "proxy-connection",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "www-authenticate",
    "x-frame-options",
    "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list, also grabbed from Chromium's net/http/http_response_headers.cc.
const char* const headerPrefixesToIgnoreAfterRevalidation[] = {
    "content-",
    "x-content-",
    "x-webkit-"
};

static inline bool shouldUpdateHeaderAfterRevalidation(const AtomicString& header)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(headersToIgnoreAfterRevalidation); i++) {
        if (equalIgnoringCase(header, headersToIgnoreAfterRevalidation[i]))
            return false;
    }
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(headerPrefixesToIgnoreAfterRevalidation); i++) {
        if (header.startsWith(headerPrefixesToIgnoreAfterRevalidation[i], false))
            return false;
    }
    return true;
}

DEFINE_DEBUG_ONLY_GLOBAL(RefCountedLeakCounter, cachedResourceLeakCounter, ("Resource"));

Resource::Resource(const ResourceRequest& request, Type type)
    : m_resourceRequest(request)
    , m_responseTimestamp(currentTime())
    , m_cancelTimer(this, &Resource::cancelTimerFired)
    , m_lastDecodedAccessTime(0)
    , m_loadFinishTime(0)
    , m_identifier(0)
    , m_encodedSize(0)
    , m_decodedSize(0)
    , m_accessCount(0)
    , m_handleCount(0)
    , m_preloadCount(0)
    , m_protectorCount(0)
    , m_preloadResult(PreloadNotReferenced)
    , m_cacheLiveResourcePriority(CacheLiveResourcePriorityLow)
    , m_inLiveDecodedResourcesList(false)
    , m_requestedFromNetworkingLayer(false)
    , m_inCache(false)
    , m_loading(false)
    , m_switchingClientsToRevalidatedResource(false)
    , m_type(type)
    , m_status(Pending)
    , m_wasPurged(false)
    , m_needsSynchronousCacheHit(false)
#ifndef NDEBUG
    , m_deleted(false)
    , m_lruIndex(0)
#endif
    , m_nextInAllResourcesList(0)
    , m_prevInAllResourcesList(0)
    , m_nextInLiveResourcesList(0)
    , m_prevInLiveResourcesList(0)
    , m_resourceToRevalidate(0)
    , m_proxyResource(0)
{
    ASSERT(m_type == unsigned(type)); // m_type is a bitfield, so this tests careless updates of the enum.
#ifndef NDEBUG
    cachedResourceLeakCounter.increment();
#endif

    if (!m_resourceRequest.url().hasFragmentIdentifier())
        return;
    KURL urlForCache = MemoryCache::removeFragmentIdentifierIfNeeded(m_resourceRequest.url());
    if (urlForCache.hasFragmentIdentifier())
        return;
    m_fragmentIdentifierForRequest = m_resourceRequest.url().fragmentIdentifier();
    m_resourceRequest.setURL(urlForCache);
}

Resource::~Resource()
{
    ASSERT(!m_resourceToRevalidate); // Should be true because canDelete() checks this.
    ASSERT(canDelete());
    ASSERT(!inCache());
    ASSERT(!m_deleted);
    ASSERT(url().isNull() || memoryCache()->resourceForURL(KURL(ParsedURLString, url())) != this);

#ifndef NDEBUG
    m_deleted = true;
    cachedResourceLeakCounter.decrement();
#endif
}

void Resource::failBeforeStarting()
{
    WTF_LOG(ResourceLoading, "Cannot start loading '%s'", url().string().latin1().data());
    error(Resource::LoadError);
}

void Resource::load(ResourceFetcher* fetcher, const ResourceLoaderOptions& options)
{
    if (!fetcher->frame()) {
        failBeforeStarting();
        return;
    }

    m_options = options;
    m_loading = true;

    if (!accept().isEmpty())
        m_resourceRequest.setHTTPAccept(accept());

    // FIXME: It's unfortunate that the cache layer and below get to know anything about fragment identifiers.
    // We should look into removing the expectation of that knowledge from the platform network stacks.
    ResourceRequest request(m_resourceRequest);
    if (!m_fragmentIdentifierForRequest.isNull()) {
        KURL url = request.url();
        url.setFragmentIdentifier(m_fragmentIdentifierForRequest);
        request.setURL(url);
        m_fragmentIdentifierForRequest = String();
    }
    m_status = Pending;
    if (m_loader) {
        RELEASE_ASSERT(m_options.synchronousPolicy == RequestSynchronously);
        m_loader->changeToSynchronous();
        return;
    }
    m_loader = ResourceLoader::create(fetcher, this, request, options);
    m_loader->start();
}

void Resource::checkNotify()
{
    if (isLoading())
        return;

    ResourceClientWalker<ResourceClient> w(m_clients);
    while (ResourceClient* c = w.next())
        c->notifyFinished(this);
}

void Resource::appendData(const char* data, int length)
{
    TRACE_EVENT0("webkit", "Resource::appendData");
    ASSERT(!m_resourceToRevalidate);
    ASSERT(!errorOccurred());
    if (m_options.dataBufferingPolicy == DoNotBufferData)
        return;
    if (m_data)
        m_data->append(data, length);
    else
        m_data = SharedBuffer::create(data, length);
    setEncodedSize(m_data->size());
}

void Resource::setResourceBuffer(PassRefPtr<SharedBuffer> resourceBuffer)
{
    ASSERT(!m_resourceToRevalidate);
    ASSERT(!errorOccurred());
    ASSERT(m_options.dataBufferingPolicy == BufferData);
    m_data = resourceBuffer;
    setEncodedSize(m_data->size());
}

void Resource::setDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    m_options.dataBufferingPolicy = dataBufferingPolicy;
    m_data.clear();
    setEncodedSize(0);
}

void Resource::error(Resource::Status status)
{
    if (m_resourceToRevalidate)
        revalidationFailed();

    if (!m_error.isNull() && (m_error.isCancellation() || !isPreloaded()))
        memoryCache()->remove(this);

    setStatus(status);
    ASSERT(errorOccurred());
    m_data.clear();

    setLoading(false);
    checkNotify();
}

void Resource::finishOnePart()
{
    setLoading(false);
    checkNotify();
}

void Resource::finish(double finishTime)
{
    ASSERT(!m_resourceToRevalidate);
    ASSERT(!errorOccurred());
    m_loadFinishTime = finishTime;
    finishOnePart();
    if (!errorOccurred())
        m_status = Cached;
}

bool Resource::passesAccessControlCheck(SecurityOrigin* securityOrigin)
{
    String ignoredErrorDescription;
    return passesAccessControlCheck(securityOrigin, ignoredErrorDescription);
}

bool Resource::passesAccessControlCheck(SecurityOrigin* securityOrigin, String& errorDescription)
{
    return WebCore::passesAccessControlCheck(m_response, resourceRequest().allowCookies() ? AllowStoredCredentials : DoNotAllowStoredCredentials, securityOrigin, errorDescription);
}

static double currentAge(const ResourceResponse& response, double responseTimestamp)
{
    // RFC2616 13.2.3
    // No compensation for latency as that is not terribly important in practice
    double dateValue = response.date();
    double apparentAge = std::isfinite(dateValue) ? std::max(0., responseTimestamp - dateValue) : 0;
    double ageValue = response.age();
    double correctedReceivedAge = std::isfinite(ageValue) ? std::max(apparentAge, ageValue) : apparentAge;
    double residentTime = currentTime() - responseTimestamp;
    return correctedReceivedAge + residentTime;
}

static double freshnessLifetime(const ResourceResponse& response, double responseTimestamp)
{
#if !OS(ANDROID)
    // On desktop, local files should be reloaded in case they change.
    if (response.url().isLocalFile())
        return 0;
#endif

    // Cache other non-http / non-filesystem resources liberally.
    if (!response.url().protocolIsInHTTPFamily()
        && !response.url().protocolIs("filesystem"))
        return std::numeric_limits<double>::max();

    // RFC2616 13.2.4
    double maxAgeValue = response.cacheControlMaxAge();
    if (std::isfinite(maxAgeValue))
        return maxAgeValue;
    double expiresValue = response.expires();
    double dateValue = response.date();
    double creationTime = std::isfinite(dateValue) ? dateValue : responseTimestamp;
    if (std::isfinite(expiresValue))
        return expiresValue - creationTime;
    double lastModifiedValue = response.lastModified();
    if (std::isfinite(lastModifiedValue))
        return (creationTime - lastModifiedValue) * 0.1;
    // If no cache headers are present, the specification leaves the decision to the UA. Other browsers seem to opt for 0.
    return 0;
}

static bool canUseResponse(const ResourceResponse& response, double responseTimestamp)
{
    if (response.isNull())
        return false;

    // FIXME: Why isn't must-revalidate considered a reason we can't use the response?
    if (response.cacheControlContainsNoCache() || response.cacheControlContainsNoStore())
        return false;

    if (response.httpStatusCode() == 303)  {
        // Must not be cached.
        return false;
    }

    if (response.httpStatusCode() == 302 || response.httpStatusCode() == 307) {
        // Default to not cacheable.
        // FIXME: Consider allowing these to be cached if they have headers permitting caching.
        return false;
    }

    return currentAge(response, responseTimestamp) <= freshnessLifetime(response, responseTimestamp);
}

void Resource::willSendRequest(ResourceRequest& request, const ResourceResponse& response)
{
    m_redirectChain.append(RedirectPair(request, response));
    m_requestedFromNetworkingLayer = true;
}

bool Resource::unlock()
{
    if (hasClients() || m_proxyResource || m_resourceToRevalidate || !m_loadFinishTime || !isSafeToUnlock())
        return false;

    if (m_purgeableData) {
        ASSERT(!m_data);
        return true;
    }
    if (!m_data)
        return false;

    // Should not make buffer purgeable if it has refs other than this since we don't want two copies.
    if (!m_data->hasOneRef())
        return false;

    m_data->createPurgeableBuffer();
    if (!m_data->hasPurgeableBuffer())
        return false;

    m_purgeableData = m_data->releasePurgeableBuffer();
    m_purgeableData->unlock();
    m_data.clear();
    return true;
}

void Resource::responseReceived(const ResourceResponse& response)
{
    setResponse(response);
    m_responseTimestamp = currentTime();
    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        setEncoding(encoding);

    if (!m_resourceToRevalidate)
        return;
    if (response.httpStatusCode() == 304)
        revalidationSucceeded(response);
    else
        revalidationFailed();
}

void Resource::setSerializedCachedMetadata(const char* data, size_t size)
{
    // We only expect to receive cached metadata from the platform once.
    // If this triggers, it indicates an efficiency problem which is most
    // likely unexpected in code designed to improve performance.
    ASSERT(!m_cachedMetadata);
    ASSERT(!m_resourceToRevalidate);

    m_cachedMetadata = CachedMetadata::deserialize(data, size);
}

void Resource::setCachedMetadata(unsigned dataTypeID, const char* data, size_t size)
{
    // Currently, only one type of cached metadata per resource is supported.
    // If the need arises for multiple types of metadata per resource this could
    // be enhanced to store types of metadata in a map.
    ASSERT(!m_cachedMetadata);

    m_cachedMetadata = CachedMetadata::create(dataTypeID, data, size);
    const Vector<char>& serializedData = m_cachedMetadata->serialize();
    blink::Platform::current()->cacheMetadata(m_response.url(), m_response.responseTime(), serializedData.data(), serializedData.size());
}

CachedMetadata* Resource::cachedMetadata(unsigned dataTypeID) const
{
    if (!m_cachedMetadata || m_cachedMetadata->dataTypeID() != dataTypeID)
        return 0;
    return m_cachedMetadata.get();
}

void Resource::setCacheLiveResourcePriority(CacheLiveResourcePriority priority)
{
    if (inCache() && m_inLiveDecodedResourcesList && cacheLiveResourcePriority() != static_cast<unsigned>(priority)) {
        memoryCache()->removeFromLiveDecodedResourcesList(this);
        m_cacheLiveResourcePriority = priority;
        memoryCache()->insertInLiveDecodedResourcesList(this);
        memoryCache()->prune();
    }
}

void Resource::clearLoader()
{
    m_loader = 0;
}

void Resource::addClient(ResourceClient* client)
{
    if (addClientToSet(client))
        didAddClient(client);
}

void Resource::didAddClient(ResourceClient* c)
{
    if (!isLoading() && !stillNeedsLoad())
        c->notifyFinished(this);
}

static bool shouldSendCachedDataSynchronouslyForType(Resource::Type type)
{
    // Some resources types default to return data synchronously.
    // For most of these, it's because there are layout tests that
    // expect data to return synchronously in case of cache hit. In
    // the case of fonts, there was a performance regression.
    // FIXME: Get to the point where we don't need to special-case sync/async
    // behavior for different resource types.
    if (type == Resource::Image)
        return true;
    if (type == Resource::CSSStyleSheet)
        return true;
    if (type == Resource::Script)
        return true;
    if (type == Resource::Font)
        return true;
    return false;
}

bool Resource::addClientToSet(ResourceClient* client)
{
    ASSERT(!isPurgeable());

    if (m_preloadResult == PreloadNotReferenced) {
        if (isLoaded())
            m_preloadResult = PreloadReferencedWhileComplete;
        else if (m_requestedFromNetworkingLayer)
            m_preloadResult = PreloadReferencedWhileLoading;
        else
            m_preloadResult = PreloadReferenced;
    }
    if (!hasClients() && inCache())
        memoryCache()->addToLiveResourcesSize(this);

    // If we have existing data to send to the new client and the resource type supprts it, send it asynchronously.
    if (!m_response.isNull() && !m_proxyResource && !shouldSendCachedDataSynchronouslyForType(type()) && !m_needsSynchronousCacheHit) {
        m_clientsAwaitingCallback.add(client);
        ResourceCallback::callbackHandler()->schedule(this);
        return false;
    }

    m_clients.add(client);
    return true;
}

void Resource::removeClient(ResourceClient* client)
{
    if (m_clientsAwaitingCallback.contains(client)) {
        ASSERT(!m_clients.contains(client));
        m_clientsAwaitingCallback.remove(client);
        if (m_clientsAwaitingCallback.isEmpty())
            ResourceCallback::callbackHandler()->cancel(this);
    } else {
        ASSERT(m_clients.contains(client));
        m_clients.remove(client);
        didRemoveClient(client);
    }

    bool deleted = deleteIfPossible();
    if (!deleted && !hasClients()) {
        if (inCache()) {
            memoryCache()->removeFromLiveResourcesSize(this);
            memoryCache()->removeFromLiveDecodedResourcesList(this);
        }
        if (!m_switchingClientsToRevalidatedResource)
            allClientsRemoved();
        if (response().cacheControlContainsNoStore()) {
            // RFC2616 14.9.2:
            // "no-store: ... MUST make a best-effort attempt to remove the information from volatile storage as promptly as possible"
            // "... History buffers MAY store such responses as part of their normal operation."
            // We allow non-secure content to be reused in history, but we do not allow secure content to be reused.
            if (url().protocolIs("https"))
                memoryCache()->remove(this);
        } else {
            memoryCache()->prune(this);
        }
    }
    // This object may be dead here.
}

void Resource::allClientsRemoved()
{
    if (!m_loader)
        return;
    if (m_type == MainResource || m_type == Raw)
        cancelTimerFired(&m_cancelTimer);
    else if (!m_cancelTimer.isActive())
        m_cancelTimer.startOneShot(0);
}

void Resource::cancelTimerFired(Timer<Resource>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_cancelTimer);
    if (hasClients() || !m_loader)
        return;
    ResourcePtr<Resource> protect(this);
    m_loader->cancelIfNotFinishing();
    if (m_status != Cached)
        memoryCache()->remove(this);
}

bool Resource::deleteIfPossible()
{
    if (canDelete() && !inCache()) {
        InspectorInstrumentation::willDestroyResource(this);
        delete this;
        return true;
    }
    return false;
}

void Resource::setDecodedSize(size_t size)
{
    if (size == m_decodedSize)
        return;

    ptrdiff_t delta = size - m_decodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_decodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        memoryCache()->removeFromLRUList(this);

    m_decodedSize = size;

    if (inCache()) {
        // Now insert into the new LRU list.
        memoryCache()->insertInLRUList(this);

        // Insert into or remove from the live decoded list if necessary.
        // When inserting into the LiveDecodedResourcesList it is possible
        // that the m_lastDecodedAccessTime is still zero or smaller than
        // the m_lastDecodedAccessTime of the current list head. This is a
        // violation of the invariant that the list is to be kept sorted
        // by access time. The weakening of the invariant does not pose
        // a problem. For more details please see: https://bugs.webkit.org/show_bug.cgi?id=30209
        if (m_decodedSize && !m_inLiveDecodedResourcesList && hasClients())
            memoryCache()->insertInLiveDecodedResourcesList(this);
        else if (!m_decodedSize && m_inLiveDecodedResourcesList)
            memoryCache()->removeFromLiveDecodedResourcesList(this);

        // Update the cache's size totals.
        memoryCache()->adjustSize(hasClients(), delta);
    }
}

void Resource::setEncodedSize(size_t size)
{
    if (size == m_encodedSize)
        return;

    ptrdiff_t delta = size - m_encodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_encodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        memoryCache()->removeFromLRUList(this);

    m_encodedSize = size;

    if (inCache()) {
        // Now insert into the new LRU list.
        memoryCache()->insertInLRUList(this);

        // Update the cache's size totals.
        memoryCache()->adjustSize(hasClients(), delta);
    }
}

void Resource::didAccessDecodedData(double timeStamp)
{
    m_lastDecodedAccessTime = timeStamp;
    if (inCache()) {
        if (m_inLiveDecodedResourcesList) {
            memoryCache()->removeFromLiveDecodedResourcesList(this);
            memoryCache()->insertInLiveDecodedResourcesList(this);
        }
        memoryCache()->prune();
    }
}

void Resource::finishPendingClients()
{
    while (!m_clientsAwaitingCallback.isEmpty()) {
        ResourceClient* client = m_clientsAwaitingCallback.begin()->key;
        m_clientsAwaitingCallback.remove(client);
        m_clients.add(client);
        didAddClient(client);
    }
}

void Resource::prune()
{
    destroyDecodedDataIfPossible();
    unlock();
}

void Resource::setResourceToRevalidate(Resource* resource)
{
    ASSERT(resource);
    ASSERT(!m_resourceToRevalidate);
    ASSERT(resource != this);
    ASSERT(m_handlesToRevalidate.isEmpty());
    ASSERT(resource->type() == type());

    WTF_LOG(ResourceLoading, "Resource %p setResourceToRevalidate %p", this, resource);

    // The following assert should be investigated whenever it occurs. Although it should never fire, it currently does in rare circumstances.
    // https://bugs.webkit.org/show_bug.cgi?id=28604.
    // So the code needs to be robust to this assert failing thus the "if (m_resourceToRevalidate->m_proxyResource == this)" in Resource::clearResourceToRevalidate.
    ASSERT(!resource->m_proxyResource);

    resource->m_proxyResource = this;
    m_resourceToRevalidate = resource;
}

void Resource::clearResourceToRevalidate()
{
    ASSERT(m_resourceToRevalidate);
    if (m_switchingClientsToRevalidatedResource)
        return;

    // A resource may start revalidation before this method has been called, so check that this resource is still the proxy resource before clearing it out.
    if (m_resourceToRevalidate->m_proxyResource == this) {
        m_resourceToRevalidate->m_proxyResource = 0;
        m_resourceToRevalidate->deleteIfPossible();
    }
    m_handlesToRevalidate.clear();
    m_resourceToRevalidate = 0;
    deleteIfPossible();
}

void Resource::switchClientsToRevalidatedResource()
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(m_resourceToRevalidate->inCache());
    ASSERT(!inCache());

    WTF_LOG(ResourceLoading, "Resource %p switchClientsToRevalidatedResource %p", this, m_resourceToRevalidate);

    m_resourceToRevalidate->m_identifier = m_identifier;

    m_switchingClientsToRevalidatedResource = true;
    HashSet<ResourcePtrBase*>::iterator end = m_handlesToRevalidate.end();
    for (HashSet<ResourcePtrBase*>::iterator it = m_handlesToRevalidate.begin(); it != end; ++it) {
        ResourcePtrBase* handle = *it;
        handle->m_resource = m_resourceToRevalidate;
        m_resourceToRevalidate->registerHandle(handle);
        --m_handleCount;
    }
    ASSERT(!m_handleCount);
    m_handlesToRevalidate.clear();

    Vector<ResourceClient*> clientsToMove;
    HashCountedSet<ResourceClient*>::iterator end2 = m_clients.end();
    for (HashCountedSet<ResourceClient*>::iterator it = m_clients.begin(); it != end2; ++it) {
        ResourceClient* client = it->key;
        unsigned count = it->value;
        while (count) {
            clientsToMove.append(client);
            --count;
        }
    }

    unsigned moveCount = clientsToMove.size();
    for (unsigned n = 0; n < moveCount; ++n)
        removeClient(clientsToMove[n]);
    ASSERT(m_clients.isEmpty());

    for (unsigned n = 0; n < moveCount; ++n)
        m_resourceToRevalidate->addClientToSet(clientsToMove[n]);
    for (unsigned n = 0; n < moveCount; ++n) {
        // Calling didAddClient may do anything, including trying to cancel revalidation.
        // Assert that it didn't succeed.
        ASSERT(m_resourceToRevalidate);
        // Calling didAddClient for a client may end up removing another client. In that case it won't be in the set anymore.
        if (m_resourceToRevalidate->m_clients.contains(clientsToMove[n]))
            m_resourceToRevalidate->didAddClient(clientsToMove[n]);
    }
    m_switchingClientsToRevalidatedResource = false;
}

void Resource::updateResponseAfterRevalidation(const ResourceResponse& validatingResponse)
{
    m_responseTimestamp = currentTime();

    // RFC2616 10.3.5
    // Update cached headers from the 304 response
    const HTTPHeaderMap& newHeaders = validatingResponse.httpHeaderFields();
    HTTPHeaderMap::const_iterator end = newHeaders.end();
    for (HTTPHeaderMap::const_iterator it = newHeaders.begin(); it != end; ++it) {
        // Entity headers should not be sent by servers when generating a 304
        // response; misconfigured servers send them anyway. We shouldn't allow
        // such headers to update the original request. We'll base this on the
        // list defined by RFC2616 7.1, with a few additions for extension headers
        // we care about.
        if (!shouldUpdateHeaderAfterRevalidation(it->key))
            continue;
        m_response.setHTTPHeaderField(it->key, it->value);
    }
}

void Resource::revalidationSucceeded(const ResourceResponse& response)
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(!m_resourceToRevalidate->inCache());
    ASSERT(m_resourceToRevalidate->isLoaded());
    ASSERT(inCache());

    // Calling evict() can potentially delete revalidatingResource, which we use
    // below. This mustn't be the case since revalidation means it is loaded
    // and so canDelete() is false.
    ASSERT(!canDelete());

    m_resourceToRevalidate->updateResponseAfterRevalidation(response);
    memoryCache()->replace(m_resourceToRevalidate, this);

    switchClientsToRevalidatedResource();
    ASSERT(!m_deleted);
    // clearResourceToRevalidate deletes this.
    clearResourceToRevalidate();
}

void Resource::revalidationFailed()
{
    ASSERT(WTF::isMainThread());
    WTF_LOG(ResourceLoading, "Revalidation failed for %p", this);
    ASSERT(resourceToRevalidate());
    clearResourceToRevalidate();
}

void Resource::updateForAccess()
{
    ASSERT(inCache());

    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    memoryCache()->removeFromLRUList(this);

    // If this is the first time the resource has been accessed, adjust the size of the cache to account for its initial size.
    if (!m_accessCount)
        memoryCache()->adjustSize(hasClients(), size());

    m_accessCount++;
    memoryCache()->insertInLRUList(this);
}

void Resource::registerHandle(ResourcePtrBase* h)
{
    ++m_handleCount;
    if (m_resourceToRevalidate)
        m_handlesToRevalidate.add(h);
}

void Resource::unregisterHandle(ResourcePtrBase* h)
{
    ASSERT(m_handleCount > 0);
    --m_handleCount;

    if (m_resourceToRevalidate)
        m_handlesToRevalidate.remove(h);

    if (!m_handleCount)
        deleteIfPossible();
}

bool Resource::canReuseRedirectChain() const
{
    for (size_t i = 0; i < m_redirectChain.size(); ++i) {
        if (!canUseResponse(m_redirectChain[i].m_redirectResponse, m_responseTimestamp))
            return false;
    }
    return true;
}

bool Resource::mustRevalidateDueToCacheHeaders() const
{
    return !canUseResponse(m_response, m_responseTimestamp);
}

bool Resource::canUseCacheValidator() const
{
    if (m_loading || errorOccurred())
        return false;

    if (m_response.cacheControlContainsNoStore())
        return false;
    return m_response.hasCacheValidatorFields();
}

bool Resource::isPurgeable() const
{
    return m_purgeableData && !m_purgeableData->isLocked();
}

bool Resource::wasPurged() const
{
    return m_wasPurged;
}

bool Resource::lock()
{
    if (!m_purgeableData)
        return true;

    ASSERT(!m_data);
    ASSERT(!hasClients());

    if (!m_purgeableData->lock()) {
        m_wasPurged = true;
        return false;
    }

    m_data = SharedBuffer::adoptPurgeableBuffer(m_purgeableData.release());
    return true;
}

size_t Resource::overheadSize() const
{
    static const int kAverageClientsHashMapSize = 384;
    return sizeof(Resource) + m_response.memoryUsage() + kAverageClientsHashMapSize + m_resourceRequest.url().string().length() * 2;
}

void Resource::didChangePriority(ResourceLoadPriority loadPriority)
{
    if (m_loader)
        m_loader->didChangePriority(loadPriority);
}

Resource::ResourceCallback* Resource::ResourceCallback::callbackHandler()
{
    DEFINE_STATIC_LOCAL(ResourceCallback, callbackHandler, ());
    return &callbackHandler;
}

Resource::ResourceCallback::ResourceCallback()
    : m_callbackTimer(this, &ResourceCallback::timerFired)
{
}

void Resource::ResourceCallback::schedule(Resource* resource)
{
    if (!m_callbackTimer.isActive())
        m_callbackTimer.startOneShot(0);
    m_resourcesWithPendingClients.add(resource);
}

void Resource::ResourceCallback::cancel(Resource* resource)
{
    m_resourcesWithPendingClients.remove(resource);
    if (m_callbackTimer.isActive() && m_resourcesWithPendingClients.isEmpty())
        m_callbackTimer.stop();
}

void Resource::ResourceCallback::timerFired(Timer<ResourceCallback>*)
{
    HashSet<Resource*>::iterator end = m_resourcesWithPendingClients.end();
    Vector<ResourcePtr<Resource> > resources;
    for (HashSet<Resource*>::iterator it = m_resourcesWithPendingClients.begin(); it != end; ++it)
        resources.append(*it);
    m_resourcesWithPendingClients.clear();
    for (size_t i = 0; i < resources.size(); i++)
        resources[i]->finishPendingClients();
}

static const char* initatorTypeNameToString(const AtomicString& initiatorTypeName)
{
    if (initiatorTypeName == FetchInitiatorTypeNames::css)
        return "CSS resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::document)
        return "Document";
    if (initiatorTypeName == FetchInitiatorTypeNames::icon)
        return "Icon";
    if (initiatorTypeName == FetchInitiatorTypeNames::internal)
        return "Internal resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::link)
        return "Link element resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::processinginstruction)
        return "Processing instruction";
    if (initiatorTypeName == FetchInitiatorTypeNames::texttrack)
        return "Text track";
    if (initiatorTypeName == FetchInitiatorTypeNames::xml)
        return "XML resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::xmlhttprequest)
        return "XMLHttpRequest";

    return "Resource";
}

const char* Resource::resourceTypeToString(Type type, const FetchInitiatorInfo& initiatorInfo)
{
    switch (type) {
    case Resource::MainResource:
        return "Main resource";
    case Resource::Image:
        return "Image";
    case Resource::CSSStyleSheet:
        return "CSS stylesheet";
    case Resource::Script:
        return "Script";
    case Resource::Font:
        return "Font";
    case Resource::Raw:
        return initatorTypeNameToString(initiatorInfo.name);
    case Resource::SVGDocument:
        return "SVG document";
    case Resource::XSLStyleSheet:
        return "XSL stylesheet";
    case Resource::LinkPrefetch:
        return "Link prefetch resource";
    case Resource::LinkSubresource:
        return "Link subresource";
    case Resource::TextTrack:
        return "Text track";
    case Resource::Shader:
        return "Shader";
    case Resource::ImportResource:
        return "Imported resource";
    }
    ASSERT_NOT_REACHED();
    return initatorTypeNameToString(initiatorInfo.name);
}

#if !LOG_DISABLED
const char* ResourceTypeName(Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        return "MainResource";
    case Resource::Image:
        return "Image";
    case Resource::CSSStyleSheet:
        return "CSSStyleSheet";
    case Resource::Script:
        return "Script";
    case Resource::Font:
        return "Font";
    case Resource::Raw:
        return "Raw";
    case Resource::SVGDocument:
        return "SVGDocument";
    case Resource::XSLStyleSheet:
        return "XSLStyleSheet";
    case Resource::LinkPrefetch:
        return "LinkPrefetch";
    case Resource::LinkSubresource:
        return "LinkSubresource";
    case Resource::TextTrack:
        return "TextTrack";
    case Resource::Shader:
        return "Shader";
    case Resource::ImportResource:
        return "ImportResource";
    }
    ASSERT_NOT_REACHED();
    return "Unknown";
}
#endif // !LOG_DISABLED

}
