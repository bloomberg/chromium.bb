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

#include "core/fetch/MemoryCache.h"

#include "core/fetch/ResourceLoadingLog.h"
#include "platform/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/AutoReset.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/text/CString.h"

namespace blink {

static Persistent<MemoryCache>* gMemoryCache;

static const unsigned cDefaultCacheCapacity = 8192 * 1024;
static const unsigned cDeferredPruneDeadCapacityFactor = 2;
static const int cMinDelayBeforeLiveDecodedPrune = 1;  // Seconds.
static const double cMaxPruneDeferralDelay = 0.5;      // Seconds.

// Percentage of capacity toward which we prune, to avoid immediately pruning
// again.
static const float cTargetPrunePercentage = .95f;

MemoryCache* memoryCache() {
  DCHECK(WTF::isMainThread());
  if (!gMemoryCache)
    gMemoryCache = new Persistent<MemoryCache>(MemoryCache::create());
  return gMemoryCache->get();
}

MemoryCache* replaceMemoryCacheForTesting(MemoryCache* cache) {
  memoryCache();
  MemoryCache* oldCache = gMemoryCache->release();
  *gMemoryCache = cache;
  MemoryCacheDumpProvider::instance()->setMemoryCache(cache);
  return oldCache;
}

DEFINE_TRACE(MemoryCacheEntry) {
  visitor->template registerWeakMembers<MemoryCacheEntry,
                                        &MemoryCacheEntry::clearResourceWeak>(
      this);
  visitor->trace(m_previousInLiveResourcesList);
  visitor->trace(m_nextInLiveResourcesList);
  visitor->trace(m_previousInAllResourcesList);
  visitor->trace(m_nextInAllResourcesList);
}

void MemoryCacheEntry::clearResourceWeak(Visitor* visitor) {
  if (!m_resource || ThreadHeap::isHeapObjectAlive(m_resource))
    return;
  memoryCache()->remove(m_resource.get());
  m_resource.clear();
}

void MemoryCacheEntry::dispose() {
  m_resource.clear();
}

Resource* MemoryCacheEntry::resource() {
  return m_resource.get();
}

DEFINE_TRACE(MemoryCacheLRUList) {
  visitor->trace(m_head);
  visitor->trace(m_tail);
}

inline MemoryCache::MemoryCache()
    : m_inPruneResources(false),
      m_prunePending(false),
      m_maxPruneDeferralDelay(cMaxPruneDeferralDelay),
      m_pruneTimeStamp(0.0),
      m_pruneFrameTimeStamp(0.0),
      m_lastFramePaintTimeStamp(0.0),
      m_capacity(cDefaultCacheCapacity),
      m_minDeadCapacity(0),
      m_maxDeadCapacity(cDefaultCacheCapacity),
      m_maxDeferredPruneDeadCapacity(cDeferredPruneDeadCapacityFactor *
                                     cDefaultCacheCapacity),
      m_delayBeforeLiveDecodedPrune(cMinDelayBeforeLiveDecodedPrune),
      m_liveSize(0),
      m_deadSize(0) {
  MemoryCacheDumpProvider::instance()->setMemoryCache(this);
  if (MemoryCoordinator::instance().isLowEndDevice())
    MemoryCoordinator::instance().registerClient(this);
}

MemoryCache* MemoryCache::create() {
  return new MemoryCache;
}

MemoryCache::~MemoryCache() {
  if (m_prunePending)
    Platform::current()->currentThread()->removeTaskObserver(this);
}

DEFINE_TRACE(MemoryCache) {
  visitor->trace(m_allResources);
  visitor->trace(m_liveDecodedResources);
  visitor->trace(m_resourceMaps);
  MemoryCacheDumpClient::trace(visitor);
  MemoryCoordinatorClient::trace(visitor);
}

KURL MemoryCache::removeFragmentIdentifierIfNeeded(const KURL& originalURL) {
  if (!originalURL.hasFragmentIdentifier())
    return originalURL;
  // Strip away fragment identifier from HTTP URLs. Data URLs must be
  // unmodified. For file and custom URLs clients may expect resources to be
  // unique even when they differ by the fragment identifier only.
  if (!originalURL.protocolIsInHTTPFamily())
    return originalURL;
  KURL url = originalURL;
  url.removeFragmentIdentifier();
  return url;
}

String MemoryCache::defaultCacheIdentifier() {
  return emptyString();
}

MemoryCache::ResourceMap* MemoryCache::ensureResourceMap(
    const String& cacheIdentifier) {
  if (!m_resourceMaps.contains(cacheIdentifier)) {
    ResourceMapIndex::AddResult result =
        m_resourceMaps.add(cacheIdentifier, new ResourceMap);
    CHECK(result.isNewEntry);
  }
  return m_resourceMaps.get(cacheIdentifier);
}

void MemoryCache::add(Resource* resource) {
  DCHECK(WTF::isMainThread());
  DCHECK(resource->url().isValid());
  ResourceMap* resources = ensureResourceMap(resource->cacheIdentifier());
  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  CHECK(!contains(resource));
  resources->set(url, MemoryCacheEntry::create(resource));
  update(resource, 0, resource->size(), true);

  RESOURCE_LOADING_DVLOG(1) << "MemoryCache::add Added "
                            << resource->url().getString() << ", resource "
                            << resource;
}

void MemoryCache::remove(Resource* resource) {
  // The resource may have already been removed by someone other than our
  // caller, who needed a fresh copy for a reload.
  if (MemoryCacheEntry* entry = getEntryForResource(resource))
    evict(entry);
}

bool MemoryCache::contains(const Resource* resource) const {
  return getEntryForResource(resource);
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL) {
  return resourceForURL(resourceURL, defaultCacheIdentifier());
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL,
                                      const String& cacheIdentifier) {
  DCHECK(WTF::isMainThread());
  if (!resourceURL.isValid() || resourceURL.isNull())
    return nullptr;
  DCHECK(!cacheIdentifier.isNull());
  ResourceMap* resources = m_resourceMaps.get(cacheIdentifier);
  if (!resources)
    return nullptr;
  KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
  MemoryCacheEntry* entry = resources->get(url);
  if (!entry)
    return nullptr;
  return entry->resource();
}

HeapVector<Member<Resource>> MemoryCache::resourcesForURL(
    const KURL& resourceURL) {
  DCHECK(WTF::isMainThread());
  KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
  HeapVector<Member<Resource>> results;
  for (const auto& resourceMapIter : m_resourceMaps) {
    if (MemoryCacheEntry* entry = resourceMapIter.value->get(url)) {
      Resource* resource = entry->resource();
      DCHECK(resource);
      results.append(resource);
    }
  }
  return results;
}

size_t MemoryCache::deadCapacity() const {
  // Dead resource capacity is whatever space is not occupied by live resources,
  // bounded by an independent minimum and maximum.
  size_t capacity =
      m_capacity -
      std::min(m_liveSize, m_capacity);  // Start with available capacity.
  capacity = std::max(capacity,
                      m_minDeadCapacity);  // Make sure it's above the minimum.
  capacity = std::min(capacity,
                      m_maxDeadCapacity);  // Make sure it's below the maximum.
  return capacity;
}

size_t MemoryCache::liveCapacity() const {
  // Live resource capacity is whatever is left over after calculating dead
  // resource capacity.
  return m_capacity - deadCapacity();
}

void MemoryCache::pruneLiveResources(PruneStrategy strategy) {
  DCHECK(!m_prunePending);
  size_t capacity = liveCapacity();
  if (strategy == MaximalPrune)
    capacity = 0;
  if (!m_liveSize || (capacity && m_liveSize <= capacity))
    return;

  // Cut by a percentage to avoid immediately pruning again.
  size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage);

  // Destroy any decoded data in live objects that we can. Start from the tail,
  // since this is the lowest priority and least recently accessed of the
  // objects.

  // The list might not be sorted by the m_lastDecodedFrameTimeStamp. The impact
  // of this weaker invariant is minor as the below if statement to check the
  // elapsedTime will evaluate to false as the current time will be a lot
  // greater than the current->m_lastDecodedFrameTimeStamp. For more details
  // see: https://bugs.webkit.org/show_bug.cgi?id=30209

  MemoryCacheEntry* current = m_liveDecodedResources.m_tail;
  while (current) {
    Resource* resource = current->resource();
    MemoryCacheEntry* previous = current->m_previousInLiveResourcesList;
    DCHECK(resource->isAlive());

    if (resource->isLoaded() && resource->decodedSize()) {
      // Check to see if the remaining resources are too new to prune.
      double elapsedTime =
          m_pruneFrameTimeStamp - current->m_lastDecodedAccessTime;
      if (strategy == AutomaticPrune &&
          elapsedTime < m_delayBeforeLiveDecodedPrune)
        return;

      // Destroy our decoded data if possible. This will remove us from
      // m_liveDecodedResources, and possibly move us to a different LRU list in
      // m_allResources.
      resource->prune();

      if (targetSize && m_liveSize <= targetSize)
        return;
    }
    current = previous;
  }
}

void MemoryCache::pruneDeadResources(PruneStrategy strategy) {
  size_t capacity = deadCapacity();
  if (strategy == MaximalPrune)
    capacity = 0;
  if (!m_deadSize || (capacity && m_deadSize <= capacity))
    return;

  // Cut by a percentage to avoid immediately pruning again.
  size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage);

  int size = m_allResources.size();
  if (targetSize && m_deadSize <= targetSize)
    return;

  bool canShrinkLRULists = true;
  for (int i = size - 1; i >= 0; i--) {
    // Remove from the tail, since this is the least frequently accessed of the
    // objects.
    MemoryCacheEntry* current = m_allResources[i].m_tail;

    // First flush all the decoded data in this queue.
    while (current) {
      Resource* resource = current->resource();
      MemoryCacheEntry* previous = current->m_previousInAllResourcesList;

      // Decoded data may reference other resources. Skip |current| if |current|
      // somehow got kicked out of cache during destroyDecodedData().
      if (!resource || !contains(resource)) {
        current = previous;
        continue;
      }

      if (!resource->isAlive() && !resource->isPreloaded() &&
          resource->isLoaded()) {
        // Destroy our decoded data. This will remove us from
        // m_liveDecodedResources, and possibly move us to a different LRU list
        // in m_allResources.
        resource->prune();

        if (targetSize && m_deadSize <= targetSize)
          return;
      }
      current = previous;
    }

    // Now evict objects from this queue.
    current = m_allResources[i].m_tail;
    while (current) {
      Resource* resource = current->resource();
      MemoryCacheEntry* previous = current->m_previousInAllResourcesList;
      if (!resource || !contains(resource)) {
        current = previous;
        continue;
      }
      if (!resource->isAlive() && !resource->isPreloaded()) {
        evict(current);
        if (targetSize && m_deadSize <= targetSize)
          return;
      }
      current = previous;
    }

    // Shrink the vector back down so we don't waste time inspecting empty LRU
    // lists on future prunes.
    if (m_allResources[i].m_head)
      canShrinkLRULists = false;
    else if (canShrinkLRULists)
      m_allResources.resize(i);
  }
}

void MemoryCache::setCapacities(size_t minDeadBytes,
                                size_t maxDeadBytes,
                                size_t totalBytes) {
  DCHECK_LE(minDeadBytes, maxDeadBytes);
  DCHECK_LE(maxDeadBytes, totalBytes);
  m_minDeadCapacity = minDeadBytes;
  m_maxDeadCapacity = maxDeadBytes;
  m_maxDeferredPruneDeadCapacity =
      cDeferredPruneDeadCapacityFactor * maxDeadBytes;
  m_capacity = totalBytes;
  prune();
}

void MemoryCache::evict(MemoryCacheEntry* entry) {
  DCHECK(WTF::isMainThread());

  Resource* resource = entry->resource();
  DCHECK(resource);
  RESOURCE_LOADING_DVLOG(1) << "Evicting resource " << resource << " for "
                            << resource->url().getString() << " from cache";
  TRACE_EVENT1("blink", "MemoryCache::evict", "resource",
               resource->url().getString().utf8());
  // The resource may have already been removed by someone other than our
  // caller, who needed a fresh copy for a reload. See
  // <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
  update(resource, resource->size(), 0, false);
  removeFromLiveDecodedResourcesList(entry);

  ResourceMap* resources = m_resourceMaps.get(resource->cacheIdentifier());
  DCHECK(resources);
  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  ResourceMap::iterator it = resources->find(url);
  DCHECK_NE(it, resources->end());

  MemoryCacheEntry* entryPtr = it->value;
  resources->remove(it);
  if (entryPtr)
    entryPtr->dispose();
}

MemoryCacheEntry* MemoryCache::getEntryForResource(
    const Resource* resource) const {
  if (!resource || resource->url().isNull() || resource->url().isEmpty())
    return nullptr;
  ResourceMap* resources = m_resourceMaps.get(resource->cacheIdentifier());
  if (!resources)
    return nullptr;
  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  MemoryCacheEntry* entry = resources->get(url);
  if (!entry || entry->resource() != resource)
    return nullptr;
  return entry;
}

MemoryCacheLRUList* MemoryCache::lruListFor(unsigned accessCount, size_t size) {
  DCHECK_GT(accessCount, 0u);
  unsigned queueIndex = WTF::fastLog2(size / accessCount);
  if (m_allResources.size() <= queueIndex)
    m_allResources.grow(queueIndex + 1);
  return &m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(MemoryCacheEntry* entry,
                                    MemoryCacheLRUList* list) {
  DCHECK(containedInLRUList(entry, list));

  MemoryCacheEntry* next = entry->m_nextInAllResourcesList;
  MemoryCacheEntry* previous = entry->m_previousInAllResourcesList;
  entry->m_nextInAllResourcesList = nullptr;
  entry->m_previousInAllResourcesList = nullptr;

  if (next)
    next->m_previousInAllResourcesList = previous;
  else
    list->m_tail = previous;

  if (previous)
    previous->m_nextInAllResourcesList = next;
  else
    list->m_head = next;

  DCHECK(!containedInLRUList(entry, list));
}

void MemoryCache::insertInLRUList(MemoryCacheEntry* entry,
                                  MemoryCacheLRUList* list) {
  DCHECK(!containedInLRUList(entry, list));

  entry->m_nextInAllResourcesList = list->m_head;
  list->m_head = entry;

  if (entry->m_nextInAllResourcesList)
    entry->m_nextInAllResourcesList->m_previousInAllResourcesList = entry;
  else
    list->m_tail = entry;

  DCHECK(containedInLRUList(entry, list));
}

bool MemoryCache::containedInLRUList(MemoryCacheEntry* entry,
                                     MemoryCacheLRUList* list) {
  for (MemoryCacheEntry* current = list->m_head; current;
       current = current->m_nextInAllResourcesList) {
    if (current == entry)
      return true;
  }
  DCHECK(!entry->m_nextInAllResourcesList);
  DCHECK(!entry->m_previousInAllResourcesList);
  return false;
}

void MemoryCache::removeFromLiveDecodedResourcesList(MemoryCacheEntry* entry) {
  // If we've never been accessed, then we're brand new and not in any list.
  if (!entry->m_inLiveDecodedResourcesList)
    return;
  DCHECK(containedInLiveDecodedResourcesList(entry));

  entry->m_inLiveDecodedResourcesList = false;

  MemoryCacheEntry* next = entry->m_nextInLiveResourcesList;
  MemoryCacheEntry* previous = entry->m_previousInLiveResourcesList;

  entry->m_nextInLiveResourcesList = nullptr;
  entry->m_previousInLiveResourcesList = nullptr;

  if (next)
    next->m_previousInLiveResourcesList = previous;
  else
    m_liveDecodedResources.m_tail = previous;

  if (previous)
    previous->m_nextInLiveResourcesList = next;
  else
    m_liveDecodedResources.m_head = next;

  DCHECK(!containedInLiveDecodedResourcesList(entry));
}

void MemoryCache::insertInLiveDecodedResourcesList(MemoryCacheEntry* entry) {
  DCHECK(!containedInLiveDecodedResourcesList(entry));

  entry->m_inLiveDecodedResourcesList = true;

  entry->m_nextInLiveResourcesList = m_liveDecodedResources.m_head;
  if (m_liveDecodedResources.m_head)
    m_liveDecodedResources.m_head->m_previousInLiveResourcesList = entry;
  m_liveDecodedResources.m_head = entry;

  if (!entry->m_nextInLiveResourcesList)
    m_liveDecodedResources.m_tail = entry;

  DCHECK(containedInLiveDecodedResourcesList(entry));
}

bool MemoryCache::containedInLiveDecodedResourcesList(MemoryCacheEntry* entry) {
  for (MemoryCacheEntry* current = m_liveDecodedResources.m_head; current;
       current = current->m_nextInLiveResourcesList) {
    if (current == entry) {
      DCHECK(entry->m_inLiveDecodedResourcesList);
      return true;
    }
  }
  DCHECK(!entry->m_nextInLiveResourcesList);
  DCHECK(!entry->m_previousInLiveResourcesList);
  DCHECK(!entry->m_inLiveDecodedResourcesList);
  return false;
}

void MemoryCache::makeLive(Resource* resource) {
  if (!contains(resource))
    return;
  DCHECK_GE(m_deadSize, resource->size());
  m_liveSize += resource->size();
  m_deadSize -= resource->size();
}

void MemoryCache::makeDead(Resource* resource) {
  if (!contains(resource))
    return;
  m_liveSize -= resource->size();
  m_deadSize += resource->size();
  removeFromLiveDecodedResourcesList(getEntryForResource(resource));
}

void MemoryCache::update(Resource* resource,
                         size_t oldSize,
                         size_t newSize,
                         bool wasAccessed) {
  MemoryCacheEntry* entry = getEntryForResource(resource);
  if (!entry)
    return;

  // The object must now be moved to a different queue, since either its size or
  // its accessCount has been changed, and both of those are used to determine
  // which LRU queue the resource should be in.
  if (oldSize)
    removeFromLRUList(entry, lruListFor(entry->m_accessCount, oldSize));
  if (wasAccessed)
    entry->m_accessCount++;
  if (newSize)
    insertInLRUList(entry, lruListFor(entry->m_accessCount, newSize));

  ptrdiff_t delta = newSize - oldSize;
  if (resource->isAlive()) {
    DCHECK(delta >= 0 || m_liveSize >= static_cast<size_t>(-delta));
    m_liveSize += delta;
  } else {
    DCHECK(delta >= 0 || m_deadSize >= static_cast<size_t>(-delta));
    m_deadSize += delta;
  }
}

void MemoryCache::updateDecodedResource(Resource* resource,
                                        UpdateReason reason) {
  MemoryCacheEntry* entry = getEntryForResource(resource);
  if (!entry)
    return;

  removeFromLiveDecodedResourcesList(entry);
  if (resource->decodedSize() && resource->isAlive())
    insertInLiveDecodedResourcesList(entry);

  if (reason != UpdateForAccess)
    return;

  double timestamp = resource->isImage() ? m_lastFramePaintTimeStamp : 0.0;
  if (!timestamp)
    timestamp = currentTime();
  entry->m_lastDecodedAccessTime = timestamp;
}

void MemoryCache::removeURLFromCache(const KURL& url) {
  HeapVector<Member<Resource>> resources = resourcesForURL(url);
  for (Resource* resource : resources)
    memoryCache()->remove(resource);
}

void MemoryCache::TypeStatistic::addResource(Resource* o) {
  count++;
  size += o->size();
  liveSize += o->isAlive() ? o->size() : 0;
  decodedSize += o->decodedSize();
  encodedSize += o->encodedSize();
  overheadSize += o->overheadSize();
  encodedSizeDuplicatedInDataURLs +=
      o->url().protocolIsData() ? o->encodedSize() : 0;
}

MemoryCache::Statistics MemoryCache::getStatistics() {
  Statistics stats;
  for (const auto& resourceMapIter : m_resourceMaps) {
    for (const auto& resourceIter : *resourceMapIter.value) {
      Resource* resource = resourceIter.value->resource();
      DCHECK(resource);
      switch (resource->getType()) {
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
  }
  return stats;
}

void MemoryCache::evictResources(EvictResourcePolicy policy) {
  for (auto resourceMapIter = m_resourceMaps.begin();
       resourceMapIter != m_resourceMaps.end();) {
    ResourceMap* resources = resourceMapIter->value.get();
    HeapVector<Member<MemoryCacheEntry>> unusedPreloads;
    for (auto resourceIter = resources->begin();
         resourceIter != resources->end(); resourceIter = resources->begin()) {
      DCHECK(resourceIter.get());
      DCHECK(resourceIter->value.get());
      DCHECK(resourceIter->value->resource());
      if (policy == EvictAllResources ||
          !(resourceIter->value->resource() &&
            resourceIter->value->resource()->isUnusedPreload())) {
        evict(resourceIter->value.get());
      } else {
        // Store unused preloads aside, so they could be added back later.
        // That is in order to avoid the performance impact of iterating over
        // the same resource multiple times.
        unusedPreloads.append(resourceIter->value.get());
        resources->remove(resourceIter);
      }
    }
    for (const auto& unusedPreload : unusedPreloads) {
      KURL url =
          removeFragmentIdentifierIfNeeded(unusedPreload->resource()->url());
      resources->set(url, unusedPreload);
    }
    // We may iterate multiple times over resourceMaps with unused preloads.
    // That's extremely unlikely to have any real-life performance impact.
    if (!resources->size()) {
      m_resourceMaps.remove(resourceMapIter);
      resourceMapIter = m_resourceMaps.begin();
    } else {
      ++resourceMapIter;
    }
  }
}

void MemoryCache::prune() {
  TRACE_EVENT0("renderer", "MemoryCache::prune()");

  if (m_inPruneResources)
    return;
  if (m_liveSize + m_deadSize <= m_capacity && m_maxDeadCapacity &&
      m_deadSize <= m_maxDeadCapacity)  // Fast path.
    return;

  // To avoid burdening the current thread with repetitive pruning jobs, pruning
  // is postponed until the end of the current task. If it has been more than
  // m_maxPruneDeferralDelay since the last prune, then we prune immediately. If
  // the current thread's run loop is not active, then pruning will happen
  // immediately only if it has been over m_maxPruneDeferralDelay since the last
  // prune.
  double currentTime = WTF::currentTime();
  if (m_prunePending) {
    if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
      pruneNow(currentTime, AutomaticPrune);
    }
  } else {
    if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
      pruneNow(currentTime, AutomaticPrune);  // Delay exceeded, prune now.
    } else {
      // Defer.
      Platform::current()->currentThread()->addTaskObserver(this);
      m_prunePending = true;
    }
  }
}

void MemoryCache::willProcessTask() {}

void MemoryCache::didProcessTask() {
  // Perform deferred pruning
  DCHECK(m_prunePending);
  pruneNow(WTF::currentTime(), AutomaticPrune);
}

void MemoryCache::pruneAll() {
  double currentTime = WTF::currentTime();
  pruneNow(currentTime, MaximalPrune);
}

void MemoryCache::pruneNow(double currentTime, PruneStrategy strategy) {
  if (m_prunePending) {
    m_prunePending = false;
    Platform::current()->currentThread()->removeTaskObserver(this);
  }

  AutoReset<bool> reentrancyProtector(&m_inPruneResources, true);

  // Prune dead first, in case it was "borrowing" capacity from live.
  pruneDeadResources(strategy);
  pruneLiveResources(strategy);
  m_pruneFrameTimeStamp = m_lastFramePaintTimeStamp;
  m_pruneTimeStamp = currentTime;
}

void MemoryCache::updateFramePaintTimestamp() {
  m_lastFramePaintTimeStamp = currentTime();
}

bool MemoryCache::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail,
                               WebProcessMemoryDump* memoryDump) {
  if (levelOfDetail == WebMemoryDumpLevelOfDetail::Background) {
    Statistics stats = getStatistics();
    WebMemoryAllocatorDump* dump1 =
        memoryDump->createMemoryAllocatorDump("web_cache/Image_resources");
    dump1->addScalar("size", "bytes",
                     stats.images.encodedSize + stats.images.overheadSize);
    WebMemoryAllocatorDump* dump2 = memoryDump->createMemoryAllocatorDump(
        "web_cache/CSS stylesheet_resources");
    dump2->addScalar("size", "bytes", stats.cssStyleSheets.encodedSize +
                                          stats.cssStyleSheets.overheadSize);
    WebMemoryAllocatorDump* dump3 =
        memoryDump->createMemoryAllocatorDump("web_cache/Script_resources");
    dump3->addScalar("size", "bytes",
                     stats.scripts.encodedSize + stats.scripts.overheadSize);
    WebMemoryAllocatorDump* dump4 = memoryDump->createMemoryAllocatorDump(
        "web_cache/XSL stylesheet_resources");
    dump4->addScalar("size", "bytes", stats.xslStyleSheets.encodedSize +
                                          stats.xslStyleSheets.overheadSize);
    WebMemoryAllocatorDump* dump5 =
        memoryDump->createMemoryAllocatorDump("web_cache/Font_resources");
    dump5->addScalar("size", "bytes",
                     stats.fonts.encodedSize + stats.fonts.overheadSize);
    WebMemoryAllocatorDump* dump6 =
        memoryDump->createMemoryAllocatorDump("web_cache/Other_resources");
    dump6->addScalar("size", "bytes",
                     stats.other.encodedSize + stats.other.overheadSize);
    return true;
  }

  for (const auto& resourceMapIter : m_resourceMaps) {
    for (const auto& resourceIter : *resourceMapIter.value) {
      Resource* resource = resourceIter.value->resource();
      resource->onMemoryDump(levelOfDetail, memoryDump);
    }
  }
  return true;
}

void MemoryCache::onMemoryPressure(WebMemoryPressureLevel level) {
  pruneAll();
}

bool MemoryCache::isInSameLRUListForTest(const Resource* x, const Resource* y) {
  MemoryCacheEntry* ex = getEntryForResource(x);
  MemoryCacheEntry* ey = getEntryForResource(y);
  DCHECK(ex);
  DCHECK(ey);
  return lruListFor(ex->m_accessCount, x->size()) ==
         lruListFor(ey->m_accessCount, y->size());
}

}  // namespace blink
