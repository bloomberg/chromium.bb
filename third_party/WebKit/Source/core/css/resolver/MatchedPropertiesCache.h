/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef MatchedPropertiesCache_h
#define MatchedPropertiesCache_h

#include "core/css/StylePropertySet.h"
#include "core/css/resolver/MatchResult.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class ComputedStyle;
class StyleResolverState;

class CachedMatchedProperties final
    : public GarbageCollectedFinalized<CachedMatchedProperties> {
 public:
  HeapVector<MatchedProperties> matched_properties;
  RefPtr<ComputedStyle> computed_style;
  RefPtr<ComputedStyle> parent_computed_style;

  void Set(const ComputedStyle&,
           const ComputedStyle& parent_style,
           const MatchedPropertiesVector&);
  void Clear();
  void Trace(blink::Visitor* visitor) { visitor->Trace(matched_properties); }
};

// Specialize the HashTraits for CachedMatchedProperties to check for dead
// entries in the MatchedPropertiesCache.
struct CachedMatchedPropertiesHashTraits
    : HashTraits<Member<CachedMatchedProperties>> {
  static const WTF::WeakHandlingFlag kWeakHandlingFlag =
      WTF::kWeakHandlingInCollections;

  template <typename VisitorDispatcher>
  static bool TraceInCollection(
      VisitorDispatcher visitor,
      Member<CachedMatchedProperties>& cached_properties,
      WTF::ShouldWeakPointersBeMarkedStrongly strongify) {
    // Only honor the cache's weakness semantics if the collection is traced
    // with WeakPointersActWeak. Otherwise just trace the cachedProperties
    // strongly, ie. call trace on it.
    if (cached_properties && strongify == WTF::kWeakPointersActWeak) {
      // A given cache entry is only kept alive if none of the MatchedProperties
      // in the CachedMatchedProperties value contain a dead "properties" field.
      // If there is a dead field the entire cache entry is removed.
      for (const auto& matched_properties :
           cached_properties->matched_properties) {
        if (!ThreadHeap::IsHeapObjectAlive(matched_properties.properties)) {
          // For now report the cache entry as dead. This might not
          // be the final result if in a subsequent call for this entry,
          // the "properties" field has been marked via another path.
          return true;
        }
      }
    }
    // At this point none of the entries in the matchedProperties vector
    // had a dead "properties" field so trace CachedMatchedProperties strongly.
    // FIXME: traceInCollection is also called from WeakProcessing to check if
    // the entry is dead.  Avoid calling trace in that case by only calling
    // trace when cachedProperties is not yet marked.
    if (!ThreadHeap::IsHeapObjectAlive(cached_properties))
      visitor->Trace(cached_properties);
    return false;
  }
};

class MatchedPropertiesCache {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(MatchedPropertiesCache);

 public:
  MatchedPropertiesCache();
  ~MatchedPropertiesCache() { DCHECK(cache_.IsEmpty()); }

  const CachedMatchedProperties* Find(unsigned hash,
                                      const StyleResolverState&,
                                      const MatchedPropertiesVector&);
  void Add(const ComputedStyle&,
           const ComputedStyle& parent_style,
           unsigned hash,
           const MatchedPropertiesVector&);

  void Clear();
  void ClearViewportDependent();

  static bool IsCacheable(const StyleResolverState&);
  static bool IsStyleCacheable(const ComputedStyle&);

  void Trace(blink::Visitor*);

 private:
  using Cache = HeapHashMap<unsigned,
                            Member<CachedMatchedProperties>,
                            DefaultHash<unsigned>::Hash,
                            HashTraits<unsigned>,
                            CachedMatchedPropertiesHashTraits>;
  Cache cache_;
};

}  // namespace blink

#endif
