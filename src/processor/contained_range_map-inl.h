// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// contained_range_map-inl.h: Hierarchically-organized range map implementation.
//
// See contained_range_map.h for documentation.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_CONTAINED_RANGE_MAP_INL_H__
#define PROCESSOR_CONTAINED_RANGE_MAP_INL_H__


#include "processor/contained_range_map.h"


namespace google_airbag {


template<typename AddressType, typename EntryType>
ContainedRangeMap<AddressType, EntryType>::~ContainedRangeMap() {
  // Clear frees the children pointed to by the map, and frees the map itself.
  Clear();
}


template<typename AddressType, typename EntryType>
bool ContainedRangeMap<AddressType, EntryType>::StoreRange(
    const AddressType &base, const AddressType &size, const EntryType &entry) {
  AddressType high = base + size - 1;

  // Check for undersize or overflow.
  if (size <= 0 || high < base)
    return false;

  if (!map_)
    map_ = new AddressToRangeMap();

  MapIterator iterator_base = map_->lower_bound(base);
  MapIterator iterator_high = map_->lower_bound(high);
  MapConstIterator iterator_end = map_->end();

  if (iterator_base == iterator_high && iterator_base != iterator_end &&
      base >= iterator_base->second->base_) {
    // The new range is entirely within an existing child range.

    // If the new range's geometry is exactly equal to an existing child
    // range's, it violates the containment rules, and an attempt to store
    // it must fail.  iterator_base->first contains the key, which was the
    // containing child's high address.
    if (iterator_base->second->base_ == base && iterator_base->first == high)
      return false;

    // Pass the new range on to the child to attempt to store.
    return iterator_base->second->StoreRange(base, size, entry);
  }

  // iterator_high might refer to an irrelevant range: one whose base address
  // is higher than the new range's high address.  Set contains_high to true
  // only if iterator_high refers to a range that is at least partially
  // within the new range.
  bool contains_high = iterator_high != iterator_end &&
                       high >= iterator_high->second->base_;

  // If the new range encompasses any existing child ranges, it must do so
  // fully.  Partial containment isn't allowed.
  if ((iterator_base != iterator_end && base > iterator_base->second->base_) ||
      (contains_high && high < iterator_high->first)) {
    return false;
  }

  // When copying and erasing contained ranges, the "end" iterator needs to
  // point one past the last item of the range to copy.  If contains_high is
  // false, the iterator's already in the right place; the increment is safe
  // because contains_high can't be true if iterator_high == iterator_end.
  if (contains_high)
    ++iterator_high;

  // Optimization: if the iterators are equal, no child ranges would be
  // moved.  Create the new child range with a NULL map to conserve space
  // in leaf nodes, of which there will be many.
  AddressToRangeMap *child_map = NULL;

  if (iterator_base != iterator_high) {
    // The children of this range that are contained by the new range must
    // be transferred over to the new range.  Create the new child range map
    // and copy the pointers to range maps it should contain into it.
    child_map = new AddressToRangeMap(iterator_base, iterator_high);

    // Remove the copied child pointers from this range's map of children.
    map_->erase(iterator_base, iterator_high);
  }

  // Store the new range in the map by its high address.  Any children that
  // the new child range contains were formerly children of this range but
  // are now this range's grandchildren.  Ownership of these is transferred
  // to the new child range.
  map_->insert(MapValue(high,
                        new ContainedRangeMap(base, entry, child_map)));
  return true;
}


template<typename AddressType, typename EntryType>
bool ContainedRangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType &address, EntryType *entry) const {
  if (!entry || !map_)
    return false;

  // Get an iterator to the child range whose high address is equal to or
  // greater than the supplied address.  If the supplied address is higher
  // than all of the high addresses in the range, then this range does not
  // contain a child at address, so return false.  If the supplied address
  // is lower than the base address of the child range, then it is not within
  // the child range, so return false.
  MapConstIterator iterator = map_->lower_bound(address);
  if (iterator == map_->end() || address < iterator->second->base_)
    return false;

  // The child in iterator->second contains the specified address.  Find out
  // if it has a more-specific descendant that also contains it.  If it does,
  // it will set |entry| appropriately.  If not, set |entry| to the child.
  if (!iterator->second->RetrieveRange(address, entry))
    *entry = iterator->second->entry_;

  return true;
}


template<typename AddressType, typename EntryType>
void ContainedRangeMap<AddressType, EntryType>::Clear() {
  if (map_) {
    MapConstIterator end = map_->end();
    for (MapConstIterator child = map_->begin(); child != end; ++child)
      delete child->second;

    delete map_;
    map_ = NULL;
  }
}


}  // namespace google_airbag


#endif  // PROCESSOR_CONTAINED_RANGE_MAP_INL_H__
