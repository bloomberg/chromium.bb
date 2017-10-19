// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_fragment_iterator.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/wtf/HashMap.h"

namespace blink {

namespace {

// Returns a static empty list.
const NGInlineFragmentIterator::Results* EmptyResults() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(NGInlineFragmentIterator::Results,
                                  empty_reuslts, ());
  return &empty_reuslts;
}

// The expected use of the iterator is for a LayoutInline/LayoutText to find
// corresponding fragments and implement existing functions using them without
// relying on InlineBox list that were removed in LayoutNGPaintFragments.
//
// When existing code traverses LayoutObject tree, they call such functions to
// all LayoutInline/LayoutText chlidren, and if each call traverses inline
// fragment tree, it will be O(n^2). To avoid O(n^2) in such case, create a map
// from LayoutObject to fragments and keep it in a cache per the containing box.
//
// TODO(kojii): May need to expand to block children when we enabled fragment
// painting everywhere.
//
// Because the caller is likely traversing the tree, keep only one cache entry.
//
// To sipmlify, the cached map ownership is moved to NGInlineFragmentIterator
// when instantiated, and moved back to the cache when destructed, assuming
// there are no use cases to nest the iterator.
struct CacheEntry {
  RefPtr<const NGPhysicalBoxFragment> box;
  NGInlineFragmentIterator::LayoutObjectMap map;
};

CacheEntry* GetCacheEntry() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CacheEntry, cache_entry, ());
  return &cache_entry;
}

bool GetCachedMap(const NGPhysicalBoxFragment& box,
                  NGInlineFragmentIterator::LayoutObjectMap* map) {
  CacheEntry* cache = GetCacheEntry();
  if (cache->box.get() != &box)
    return false;
  cache->map.swap(*map);
  return true;
}

void AddToCache(const NGPhysicalBoxFragment& box,
                NGInlineFragmentIterator::LayoutObjectMap* map) {
  CacheEntry* cache = GetCacheEntry();
  cache->box = &box;
  cache->map.swap(*map);
}

}  // namespace

NGInlineFragmentIterator::NGInlineFragmentIterator(
    const NGPhysicalBoxFragment& box,
    const LayoutObject* filter)
    : box_(box) {
  DCHECK(filter);

  // Check the cache. This iterator owns the map while alive.
  if (!GetCachedMap(box, &map_)) {
    // Create a map if it's not in the cache.
    CollectInlineFragments(box, {}, &map_);
  }

  const auto it = map_.find(filter);
  results_ = it != map_.end() ? &it->value : EmptyResults();
}

NGInlineFragmentIterator::~NGInlineFragmentIterator() {
  // Return the ownership of the map to the cache.
  AddToCache(box_, &map_);
}

// Create a map from a LayoutObject to a vector of PhysicalFragment and its
// offset to the container box. This is done by collecting inline child
// fragments of the container fragment, while accumulating the offset to the
// container box.
void NGInlineFragmentIterator::CollectInlineFragments(
    const NGPhysicalContainerFragment& container,
    NGPhysicalOffset offset_to_container_box,
    LayoutObjectMap* map) {
  for (const auto& child : container.Children()) {
    NGPhysicalOffset child_offset = child->Offset() + offset_to_container_box;

    // In order to find fragments that correspond to the specified LayoutObject,
    // add to the map if the fragment has a LayoutObject.
    if (const LayoutObject* child_layout_object = child->GetLayoutObject()) {
      const auto it = map->find(child_layout_object);
      if (it == map->end()) {
        map->insert(child_layout_object, Results{{child.get(), child_offset}});
      } else {
        it->value.push_back(Result{child.get(), child_offset});
      }
    }

    // Traverse descendants unless the fragment is laid out separately from the
    // inline layout algorithm.
    if (child->IsContainer() && !child->IsBlockLayoutRoot()) {
      CollectInlineFragments(ToNGPhysicalContainerFragment(*child),
                             child_offset, map);
    }
  }
}

}  // namespace blink
