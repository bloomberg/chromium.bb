/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef Cache_h
#define Cache_h

#include "core/loader/cache/Resource.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore  {

class CSSStyleSheetResource;
class Resource;
class ResourceFetcher;
class KURL;
class ScriptExecutionContext;
class SecurityOrigin;
struct SecurityOriginHash;

// This cache holds subresources used by Web pages: images, scripts, stylesheets, etc.

// The cache keeps a flexible but bounded window of dead resources that grows/shrinks
// depending on the live resource load. Here's an example of cache growth over time,
// with a min dead resource capacity of 25% and a max dead resource capacity of 50%:

//        |-----|                              Dead: -
//        |----------|                         Live: +
//      --|----------|                         Cache boundary: | (objects outside this mark have been evicted)
//      --|----------++++++++++|
// -------|-----+++++++++++++++|
// -------|-----+++++++++++++++|+++++

// Enable this macro to periodically log information about the memory cache.
#undef MEMORY_CACHE_STATS

class MemoryCache {
    WTF_MAKE_NONCOPYABLE(MemoryCache); WTF_MAKE_FAST_ALLOCATED;
public:
    MemoryCache();
    ~MemoryCache() { }

    typedef HashMap<String, Resource*> ResourceMap;

    struct LRUList {
        Resource* m_head;
        Resource* m_tail;
        LRUList() : m_head(0), m_tail(0) { }
    };

    struct TypeStatistic {
        int count;
        int size;
        int liveSize;
        int decodedSize;
        int encodedSize;
        int encodedSizeDuplicatedInDataURLs;
        int purgeableSize;
        int purgedSize;

        TypeStatistic()
            : count(0)
            , size(0)
            , liveSize(0)
            , decodedSize(0)
            , encodedSize(0)
            , encodedSizeDuplicatedInDataURLs(0)
            , purgeableSize(0)
            , purgedSize(0)
        {
        }

        void addResource(Resource*);
    };

    struct Statistics {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
        TypeStatistic xslStyleSheets;
        TypeStatistic fonts;
        TypeStatistic other;
    };

    Resource* resourceForURL(const KURL&);

    void add(Resource*);
    void replace(Resource* newResource, Resource* oldResource);
    void remove(Resource* resource) { evict(resource); }

    static KURL removeFragmentIdentifierIfNeeded(const KURL& originalURL);

    // Sets the cache's memory capacities, in bytes. These will hold only approximately,
    // since the decoded cost of resources like scripts and stylesheets is not known.
    //  - minDeadBytes: The maximum number of bytes that dead resources should consume when the cache is under pressure.
    //  - maxDeadBytes: The maximum number of bytes that dead resources should consume when the cache is not under pressure.
    //  - totalBytes: The maximum number of bytes that the cache should consume overall.
    void setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes);
    void setDelayBeforeLiveDecodedPrune(unsigned seconds) { m_delayBeforeLiveDecodedPrune = seconds; }

    void evictResources();

    void prune();

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(Resource*);
    void removeFromLRUList(Resource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, int delta);

    // Track decoded resources that are in the cache and referenced by a Web page.
    void insertInLiveDecodedResourcesList(Resource*);
    void removeFromLiveDecodedResourcesList(Resource*);

    void addToLiveResourcesSize(Resource*);
    void removeFromLiveResourcesSize(Resource*);

    static void removeURLFromCache(ScriptExecutionContext*, const KURL&);

    Statistics getStatistics();

    unsigned minDeadCapacity() const { return m_minDeadCapacity; }
    unsigned maxDeadCapacity() const { return m_maxDeadCapacity; }
    unsigned capacity() const { return m_capacity; }
    unsigned liveSize() const { return m_liveSize; }
    unsigned deadSize() const { return m_deadSize; }

private:
    LRUList* lruListFor(Resource*);

#ifdef MEMORY_CACHE_STATS
    void dumpStats(Timer<MemoryCache>*);
    void dumpLRULists(bool includeLive) const;
#endif

    unsigned liveCapacity() const;
    unsigned deadCapacity() const;

    // pruneDeadResources() - Flush decoded and encoded data from resources not referenced by Web pages.
    // pruneLiveResources() - Flush decoded data from resources still referenced by Web pages.
    void pruneDeadResources(); // Automatically decide how much to prune.
    void pruneLiveResources();

    void evict(Resource*);

    static void removeURLFromCacheInternal(ScriptExecutionContext*, const KURL&);

    bool m_inPruneResources;

    unsigned m_capacity;
    unsigned m_minDeadCapacity;
    unsigned m_maxDeadCapacity;
    unsigned m_delayBeforeLiveDecodedPrune;
    double m_deadDecodedDataDeletionInterval;

    unsigned m_liveSize; // The number of bytes currently consumed by "live" resources in the cache.
    unsigned m_deadSize; // The number of bytes currently consumed by "dead" resources in the cache.

    // Size-adjusted and popularity-aware LRU list collection for cache objects.  This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" multiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_allResources;

    // Lists just for live resources with decoded data. Access to this list is based off of painting the resource.
    // The lists are ordered by decode priority, with higher indices having higher priorities.
    LRUList m_liveDecodedResources[Resource::CacheLiveResourcePriorityHigh + 1];

    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being
    // referenced by a Web page).
    HashMap<String, Resource*> m_resources;

#ifdef MEMORY_CACHE_STATS
    Timer<MemoryCache> m_statsTimer;
#endif
};

// Returns the global cache.
MemoryCache* memoryCache();

// Sets the global cache, used to swap in a test instance.
void setMemoryCacheForTesting(MemoryCache*);

}

#endif
