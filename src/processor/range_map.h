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

// range_map.h: Range maps.
//
// A range map associates a range of addresses with a specific object.  This
// is useful when certain objects of variable size are located within an
// address space.  The range map makes it simple to determine which object is
// associated with a specific address, which may be any address within the
// range associated with an object.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_RANGE_MAP_H__
#define PROCESSOR_RANGE_MAP_H__


#include <map>


namespace google_airbag {


using std::map;


template<typename AddressType, typename EntryType>
class RangeMap {
 public:
  RangeMap() : map_() {}

  // Inserts a range into the map.  Returns false for a parameter error,
  // or if the location of the range would conflict with a range already
  // stored in the map.
  bool StoreRange(const AddressType& base,
                  const AddressType& size,
                  const EntryType&   entry);

  // Locates the range encompassing the supplied address.  If there is
  // no such range, or if there is a parameter error, returns false.
  bool RetrieveRange(const AddressType& address, EntryType* entry) const;

  // Empties the range map, restoring it to the state it was when it was
  // initially created.
  void Clear();

 private:
  class Range {
   public:
    Range(const AddressType& base, const EntryType& entry)
        : base_(base), entry_(entry) {}

    AddressType base() const { return base_; }
    EntryType entry() const { return entry_; }

   private:
    // The base address of the range.  The high address does not need to
    // be stored, because RangeMap uses it as the key to the map.
    const AddressType base_;

    // The entry, owned by the Range object.
    const EntryType   entry_;
  };

  typedef map<AddressType, Range> AddressToRangeMap;

  // Can't depend on implicit typenames in a template
  typedef typename AddressToRangeMap::const_iterator const_iterator;
  typedef typename AddressToRangeMap::value_type value_type;

  // Maps the high address of each range to a EntryType.
  AddressToRangeMap map_;
};


template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::StoreRange(const AddressType& base,
                                                  const AddressType& size,
                                                  const EntryType&   entry) {
  AddressType high = base + size - 1;

  // Check for undersize or overflow.
  if (size <= 0 || high < base)
    return false;

  // Ensure that this range does not overlap with another one already in the
  // map.
  const_iterator iterator_base = map_.lower_bound(base);
  const_iterator iterator_high = map_.lower_bound(high);

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
  map_.insert(value_type(high, Range(base, entry)));
  return true;
}


template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType& address,
    EntryType*         entry) const {
  if (!entry)
    return false;

  const_iterator iterator = map_.lower_bound(address);
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


} // namespace google_airbag


#endif // PROCESSOR_RANGE_MAP_H__
