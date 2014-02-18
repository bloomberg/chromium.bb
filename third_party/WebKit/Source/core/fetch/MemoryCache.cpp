/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
#include "core/fetch/MemoryCache.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/fetch/ResourcePtr.h"
#include "core/frame/FrameView.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/CString.h"

namespace WebCore {

static MemoryCache* gMemoryCache;

static const unsigned cDefaultCacheCapacity = 8192 * 1024;
static const unsigned cDeferredPruneDeadCapacityFactor = 2;
static const int cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const double cMaxPruneDeferralDelay = 0.5; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.

MemoryCache* memoryCache()
{
    ASSERT(WTF::isMainThread());
    if (!gMemoryCache)
        gMemoryCache = new MemoryCache();
    return gMemoryCache;
}

void setMemoryCacheForTesting(MemoryCache* memoryCache)
{
    gMemoryCache = memoryCache;
}

MemoryCache::MemoryCache()
    : m_inPruneResources(false)
    , m_prunePending(false)
    , m_maxPruneDeferralDelay(cMaxPruneDeferralDelay)
    , m_capacity(cDefaultCacheCapacity)
    , m_minDeadCapacity(0)
    , m_maxDeadCapacity(cDefaultCacheCapacity)
    , m_maxDeferredPruneDeadCapacity(cDeferredPruneDeadCapacityFactor * cDefaultCacheCapacity)
    , m_delayBeforeLiveDecodedPrune(cMinDelayBeforeLiveDecodedPrune)
    , m_liveSize(0)
    , m_deadSize(0)
#ifdef MEMORY_CACHE_STATS
    , m_statsTimer(this, &MemoryCache::dumpStats)
#endif
{
#ifdef MEMORY_CACHE_STATS
    const double statsIntervalInSeconds = 15;
    m_statsTimer.startRepeating(statsIntervalInSeconds);
#endif
    m_pruneTimeStamp = m_pruneFrameTimeStamp = FrameView::currentFrameTimeStamp();
}

MemoryCache::~MemoryCache()
{
    if (m_prunePending)
        blink::Platform::current()->currentThread()->removeTaskObserver(this);
}

KURL MemoryCache::removeFragmentIdentifierIfNeeded(const KURL& originalURL)
{
    if (!originalURL.hasFragmentIdentifier())
        return originalURL;
    // Strip away fragment identifier from HTTP URLs.
    // Data URLs must be unmodified. For file and custom URLs clients may expect resources
    // to be unique even when they differ by the fragment identifier only.
    if (!originalURL.protocolIsInHTTPFamily())
        return originalURL;
    KURL url = originalURL;
    url.removeFragmentIdentifier();
    return url;
}

void MemoryCache::add(Resource* resource)
{
    ASSERT(WTF::isMainThread());
    m_resources.set(resource->url(), resource);
    resource->setInCache(true);
    resource->updateForAccess();

    WTF_LOG(ResourceLoading, "MemoryCache::add Added '%s', resource %p\n", resource->url().string().latin1().data(), resource);
}

void MemoryCache::replace(Resource* newResource, Resource* oldResource)
{
    evict(oldResource);
    ASSERT(!m_resources.get(newResource->url()));
    m_resources.set(newResource->url(), newResource);
    newResource->setInCache(true);
    insertInLRUList(newResource);
    int delta = newResource->size();
    if (newResource->decodedSize() && newResource->hasClients())
        insertInLiveDecodedResourcesList(newResource);
    if (delta)
        adjustSize(newResource->hasClients(), delta);
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL)
{
    ASSERT(WTF::isMainThread());
    KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
    Resource* resource = m_resources.get(url);
    if (resource && !resource->lock()) {
        ASSERT(!resource->hasClients());
        bool didEvict = evict(resource);
        ASSERT_UNUSED(didEvict, didEvict);
        return 0;
    }
    return resource;
}

size_t MemoryCache::deadCapacity() const
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    size_t capacity = m_capacity - std::min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = std::max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = std::min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

size_t MemoryCache::liveCapacity() const
{
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

void MemoryCache::pruneLiveResources()
{
    ASSERT(!m_prunePending);
    size_t capacity = liveCapacity();
    if (!m_liveSize || (capacity && m_liveSize <= capacity))
        return;

    size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the lowest priority
    // and least recently accessed of the objects.

    // The list might not be sorted by the m_lastDecodedFrameTimeStamp. The impact
    // of this weaker invariant is minor as the below if statement to check the
    // elapsedTime will evaluate to false as the current time will be a lot
    // greater than the current->m_lastDecodedFrameTimeStamp.
    // For more details see: https://bugs.webkit.org/show_bug.cgi?id=30209

    // Start pruning from the lowest priority list.
    for (int priority = Resource::CacheLiveResourcePriorityLow; priority <= Resource::CacheLiveResourcePriorityHigh; ++priority) {
        Resource* current = m_liveDecodedResources[priority].m_tail;
        while (current) {
            Resource* prev = current->m_prevInLiveResourcesList;
            ASSERT(current->hasClients());
            if (current->isLoaded() && current->decodedSize()) {
                // Check to see if the remaining resources are too new to prune.
                double elapsedTime = m_pruneFrameTimeStamp - current->m_lastDecodedAccessTime;
                if (elapsedTime < m_delayBeforeLiveDecodedPrune)
                    return;

                // Destroy our decoded data if possible. This will remove us
                // from m_liveDecodedResources, and possibly move us to a
                // different LRU list in m_allResources.
                current->prune();

                if (targetSize && m_liveSize <= targetSize)
                    return;
            }
            current = prev;
        }
    }
}

void MemoryCache::pruneDeadResources()
{
    size_t capacity = deadCapacity();
    if (!m_deadSize || (capacity && m_deadSize <= capacity))
        return;

    size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    int size = m_allResources.size();

    // See if we have any purged resources we can evict.
    for (int i = 0; i < size; i++) {
        Resource* current = m_allResources[i].m_tail;
        while (current) {
            Resource* prev = current->m_prevInAllResourcesList;
            if (current->wasPurged()) {
                ASSERT(!current->hasClients());
                ASSERT(!current->isPreloaded());
                evict(current);
            }
            current = prev;
        }
    }
    if (targetSize && m_deadSize <= targetSize)
        return;

    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        Resource* current = m_allResources[i].m_tail;

        // First flush all the decoded data in this queue.
        while (current) {
            // Protect 'previous' so it can't get deleted during destroyDecodedData().
            ResourcePtr<Resource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && current->isLoaded()) {
                // Destroy our decoded data if possible. This will remove us
                // from m_liveDecodedResources, and possibly move us to a
                // different LRU list in m_allResources.
                current->prune();

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            // Decoded data may reference other resources. Stop iterating if 'previous' somehow got
            // kicked out of cache during destroyDecodedData().
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            ResourcePtr<Resource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && !current->isCacheValidator()) {
                evict(current);
                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }

        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

void MemoryCache::setCapacities(size_t minDeadBytes, size_t maxDeadBytes, size_t totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_maxDeferredPruneDeadCapacity = cDeferredPruneDeadCapacityFactor * maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

bool MemoryCache::evict(Resource* resource)
{
    ASSERT(WTF::isMainThread());
    WTF_LOG(ResourceLoading, "Evicting resource %p for '%s' from cache", resource, resource->url().string().latin1().data());
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    if (resource->inCache()) {
        // Remove from the resource map.
        m_resources.remove(resource->url());
        resource->setInCache(false);

        // Remove from the appropriate LRU list.
        removeFromLRUList(resource);
        removeFromLiveDecodedResourcesList(resource);
        adjustSize(resource->hasClients(), -static_cast<ptrdiff_t>(resource->size()));
    } else {
        ASSERT(m_resources.get(resource->url()) != resource);
    }

    return resource->deleteIfPossible();
}

MemoryCache::LRUList* MemoryCache::lruListFor(Resource* resource)
{
    unsigned accessCount = std::max(resource->accessCount(), 1U);
    unsigned queueIndex = WTF::fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(Resource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->accessCount())
        return;

#if !ASSERT_DISABLED
    unsigned oldListIndex = resource->m_lruIndex;
#endif

    LRUList* list = lruListFor(resource);

#if !ASSERT_DISABLED
    // Verify that the list we got is the list we want.
    ASSERT(resource->m_lruIndex == oldListIndex);

    // Verify that we are in fact in this list.
    bool found = false;
    for (Resource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    Resource* next = resource->m_nextInAllResourcesList;
    Resource* prev = resource->m_prevInAllResourcesList;

    if (!next && !prev && list->m_head != resource)
        return;

    resource->m_nextInAllResourcesList = 0;
    resource->m_prevInAllResourcesList = 0;

    if (next)
        next->m_prevInAllResourcesList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInAllResourcesList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void MemoryCache::insertInLRUList(Resource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInAllResourcesList && !resource->m_prevInAllResourcesList);
    ASSERT(resource->inCache());
    ASSERT(resource->accessCount() > 0);

    LRUList* list = lruListFor(resource);

    resource->m_nextInAllResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInAllResourcesList = resource;
    list->m_head = resource;

    if (!resource->m_nextInAllResourcesList)
        list->m_tail = resource;

#if !ASSERT_DISABLED
    // Verify that we are in now in the list like we should be.
    list = lruListFor(resource);
    bool found = false;
    for (Resource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void MemoryCache::removeFromLiveDecodedResourcesList(Resource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->m_inLiveDecodedResourcesList)
        return;
    resource->m_inLiveDecodedResourcesList = false;

    LRUList* list = &m_liveDecodedResources[resource->cacheLiveResourcePriority()];

#if !ASSERT_DISABLED
    // Verify that we are in fact in this list.
    bool found = false;
    for (Resource* current = list->m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    Resource* next = resource->m_nextInLiveResourcesList;
    Resource* prev = resource->m_prevInLiveResourcesList;

    if (!next && !prev && list->m_head != resource)
        return;

    resource->m_nextInLiveResourcesList = 0;
    resource->m_prevInLiveResourcesList = 0;

    if (next)
        next->m_prevInLiveResourcesList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInLiveResourcesList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void MemoryCache::insertInLiveDecodedResourcesList(Resource* resource)
{
    // Make sure we aren't in the list already.
    ASSERT(!resource->m_nextInLiveResourcesList && !resource->m_prevInLiveResourcesList && !resource->m_inLiveDecodedResourcesList);
    resource->m_inLiveDecodedResourcesList = true;

    LRUList* list = &m_liveDecodedResources[resource->cacheLiveResourcePriority()];
    resource->m_nextInLiveResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInLiveResourcesList = resource;
    list->m_head = resource;

    if (!resource->m_nextInLiveResourcesList)
        list->m_tail = resource;

#if !ASSERT_DISABLED
    // Verify that we are in now in the list like we should be.
    bool found = false;
    for (Resource* current = list->m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void MemoryCache::addToLiveResourcesSize(Resource* resource)
{
    ASSERT(m_deadSize >= resource->size());
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void MemoryCache::removeFromLiveResourcesSize(Resource* resource)
{
    ASSERT(m_liveSize >= resource->size());
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
}

void MemoryCache::adjustSize(bool live, ptrdiff_t delta)
{
    if (live) {
        ASSERT(delta >= 0 || m_liveSize >= static_cast<size_t>(-delta) );
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || m_deadSize >= static_cast<size_t>(-delta) );
        m_deadSize += delta;
    }
}

void MemoryCache::removeURLFromCache(ExecutionContext* context, const KURL& url)
{
    if (context->isWorkerGlobalScope()) {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        workerGlobalScope->thread()->workerLoaderProxy().postTaskToLoader(createCallbackTask(&removeURLFromCacheInternal, url));
        return;
    }
    removeURLFromCacheInternal(context, url);
}

void MemoryCache::removeURLFromCacheInternal(ExecutionContext*, const KURL& url)
{
    if (Resource* resource = memoryCache()->resourceForURL(url))
        memoryCache()->remove(resource);
}

void MemoryCache::TypeStatistic::addResource(Resource* o)
{
    bool purged = o->wasPurged();
    bool purgeable = o->isPurgeable() && !purged;
    size_t pageSize = (o->encodedSize() + o->overheadSize() + 4095) & ~4095;
    count++;
    size += purged ? 0 : o->size();
    liveSize += o->hasClients() ? o->size() : 0;
    decodedSize += o->decodedSize();
    encodedSize += o->encodedSize();
    encodedSizeDuplicatedInDataURLs += o->url().protocolIsData() ? o->encodedSize() : 0;
    purgeableSize += purgeable ? pageSize : 0;
    purgedSize += purged ? pageSize : 0;
}

MemoryCache::Statistics MemoryCache::getStatistics()
{
    Statistics stats;
    ResourceMap::iterator e = m_resources.end();
    for (ResourceMap::iterator i = m_resources.begin(); i != e; ++i) {
        Resource* resource = i->value;
        switch (resource->type()) {
        case Resource::Image:
            stats.images.addResource(resource);
            break;
        case Resource::CSSStyleSheet:
            stats.cssStyleSheets.addResource(resource);
            break;
        case Resource::Script:
            stats.scripts.addResource(resource);
            break;
        case Resource::XSLStyleSheet:
            stats.xslStyleSheets.addResource(resource);
            break;
        case Resource::Font:
            stats.fonts.addResource(resource);
            break;
        default:
            stats.other.addResource(resource);
            break;
        }
    }
    return stats;
}

void MemoryCache::evictResources()
{
    for (;;) {
        ResourceMap::iterator i = m_resources.begin();
        if (i == m_resources.end())
            break;
        evict(i->value);
    }
}

void MemoryCache::prune(Resource* justReleasedResource)
{
    TRACE_EVENT0("renderer", "MemoryCache::prune()");

    if (m_inPruneResources)
        return;
    if (m_liveSize + m_deadSize <= m_capacity && m_maxDeadCapacity && m_deadSize <= m_maxDeadCapacity) // Fast path.
        return;

    // To avoid burdening the current thread with repetitive pruning jobs,
    // pruning is postponed until the end of the current task. If it has
    // been more that m_maxPruneDeferralDelay since the last prune,
    // then we prune immediately.
    // If the current thread's run loop is not active, then pruning will happen
    // immediately only if it has been over m_maxPruneDeferralDelay
    // since the last prune.
    double currentTime = WTF::currentTime();
    if (m_prunePending) {
        if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
            pruneNow(currentTime);
        }
    } else {
        if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
            pruneNow(currentTime); // Delay exceeded, prune now.
        } else {
            // Defer.
            blink::Platform::current()->currentThread()->addTaskObserver(this);
            m_prunePending = true;
        }
    }

    if (m_prunePending && m_deadSize > m_maxDeferredPruneDeadCapacity && justReleasedResource) {
        // The following eviction does not respect LRU order, but it can be done
        // immediately in constant time, as opposed to pruneDeadResources, which
        // we would rather defer because it is O(N), which would make tear-down of N
        // objects O(N^2) if we pruned immediately. This immediate eviction is a
        // safeguard against runaway memory consumption by dead resources
        // while a prune is pending.
        evict(justReleasedResource);

        // As a last resort, prune immediately
        if (m_deadSize > m_maxDeferredPruneDeadCapacity)
            pruneNow(currentTime);
    }
}

void MemoryCache::willProcessTask()
{
}

void MemoryCache::didProcessTask()
{
    // Perform deferred pruning
    ASSERT(m_prunePending);
    pruneNow(WTF::currentTime());
}

void MemoryCache::pruneNow(double currentTime)
{
    if (m_prunePending) {
        m_prunePending = false;
        blink::Platform::current()->currentThread()->removeTaskObserver(this);
    }

    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);
    pruneDeadResources(); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResources();
    m_pruneFrameTimeStamp = FrameView::currentFrameTimeStamp();
    m_pruneTimeStamp = currentTime;
}

#ifdef MEMORY_CACHE_STATS

void MemoryCache::dumpStats(Timer<MemoryCache>*)
{
    Statistics s = getStatistics();
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize", "PurgeableSize", "PurgedSize");
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize, s.images.purgeableSize, s.images.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "CSS", s.cssStyleSheets.count, s.cssStyleSheets.size, s.cssStyleSheets.liveSize, s.cssStyleSheets.decodedSize, s.cssStyleSheets.purgeableSize, s.cssStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "XSL", s.xslStyleSheets.count, s.xslStyleSheets.size, s.xslStyleSheets.liveSize, s.xslStyleSheets.decodedSize, s.xslStyleSheets.purgeableSize, s.xslStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "JavaScript", s.scripts.count, s.scripts.size, s.scripts.liveSize, s.scripts.decodedSize, s.scripts.purgeableSize, s.scripts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Fonts", s.fonts.count, s.fonts.size, s.fonts.liveSize, s.fonts.decodedSize, s.fonts.purgeableSize, s.fonts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Other", s.other.count, s.other.size, s.other.liveSize, s.other.decodedSize, s.other.purgeableSize, s.other.purgedSize);
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");

    printf("Duplication of encoded data from data URLs\n");
    printf("%-13s %13d of %13d\n", "Images",     s.images.encodedSizeDuplicatedInDataURLs,         s.images.encodedSize);
    printf("%-13s %13d of %13d\n", "CSS",        s.cssStyleSheets.encodedSizeDuplicatedInDataURLs, s.cssStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "XSL",        s.xslStyleSheets.encodedSizeDuplicatedInDataURLs, s.xslStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "JavaScript", s.scripts.encodedSizeDuplicatedInDataURLs,        s.scripts.encodedSize);
    printf("%-13s %13d of %13d\n", "Fonts",      s.fonts.encodedSizeDuplicatedInDataURLs,          s.fonts.encodedSize);
    printf("%-13s %13d of %13d\n", "Other",      s.other.encodedSizeDuplicatedInDataURLs,          s.other.encodedSize);
}

void MemoryCache::dumpLRULists(bool includeLive) const
{
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced, isPurgeable, wasPurged):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        Resource* current = m_allResources[i].m_tail;
        while (current) {
            Resource* prev = current->m_prevInAllResourcesList;
            if (includeLive || !current->hasClients())
                printf("(%.1fK, %.1fK, %uA, %dR, %d, %d); ", current->decodedSize() / 1024.0f, (current->encodedSize() + current->overheadSize()) / 1024.0f, current->accessCount(), current->hasClients(), current->isPurgeable(), current->wasPurged());

            current = prev;
        }
    }
}

#endif // MEMORY_CACHE_STATS

} // namespace WebCore
