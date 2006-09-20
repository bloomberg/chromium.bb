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

// range_map-inl.h: Range map implementation.
//
// See range_map.h for documentation.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_RANGE_MAP_INL_H__
#define PROCESSOR_RANGE_MAP_INL_H__


#include "processor/range_map.h"


namespace google_airbag {


template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::StoreRange(const AddressType &base,
                                                  const AddressType &size,
                                                  const EntryType &entry) {
  AddressType high = base + size - 1;

  // Check for undersize or overflow.
  if (size <= 0 || high < base)
    return false;

  // Ensure that this range does not overlap with another one already in the
  // map.
  MapConstIterator iterator_base = map_.lower_bound(base);
  MapConstIterator iterator_high = map_.lower_bound(high);

  if (iterator_base != iterator_high) {
    // Some other range begins in the space used by this range.  It may be
    // contained within the space used by this range, or it may extend lower.
    // Regardless, it is an error.
    return false;
  }

  if (iterator_high != map_.end()) {
    if (iterator_high->second.base() <= high) {
      // The range above this one overlaps with this one.  It may fully
      // contain  this range, or it may begin within this range and extend
      // higher.  Regardless, it's an error.
      return false;
    }
  }

  // Store the range in the map by its high address, so that lower_bound can
  // be used to quickly locate a range by address.
  map_.insert(MapValue(high, Range(base, entry)));
  return true;
}


template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType &address, EntryType *entry) const {
  if (!entry)
    return false;

  MapConstIterator iterator = map_.lower_bound(address);
  if (iterator == map_.end())
    return false;

  // The map is keyed by the high address of each range, so |address| is
  // guaranteed to be lower than the range's high address.  If |range| is
  // not directly preceded by another range, it's possible for address to
  // be below the range's low address, though.  When that happens, address
  // references something not within any range, so return false.
  if (address < iterator->second.base())
    return false;

  *entry = iterator->second.entry();
  return true;
}


template<typename AddressType, typename EntryType>
void RangeMap<AddressType, EntryType>::Clear() {
  map_.clear();
}


}  // namespace google_airbag


#endif  // PROCESSOR_RANGE_MAP_INL_H__
