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


template<typename AddressType, typename EntryType>
class RangeMap {
 public:
  RangeMap() : map_() {}

  // Inserts a range into the map.  Returns false for a parameter error,
  // or if the location of the range would conflict with a range already
  // stored in the map.
  bool StoreRange(const AddressType &base,
                  const AddressType &size,
                  const EntryType &entry);

  // Locates the range encompassing the supplied address.  If there is
  // no such range, or if there is a parameter error, returns false.
  bool RetrieveRange(const AddressType &address, EntryType *entry) const;

  // Empties the range map, restoring it to the state it was when it was
  // initially created.
  void Clear();

 private:
  class Range {
   public:
    Range(const AddressType &base, const EntryType &entry)
        : base_(base), entry_(entry) {}

    AddressType base() const { return base_; }
    EntryType entry() const { return entry_; }

   private:
    // The base address of the range.  The high address does not need to
    // be stored, because RangeMap uses it as the key to the map.
    const AddressType base_;

    // The entry corresponding to a range.
    const EntryType entry_;
  };

  // Convenience types.
  typedef std::map<AddressType, Range> AddressToRangeMap;
  typedef typename AddressToRangeMap::const_iterator MapConstIterator;
  typedef typename AddressToRangeMap::value_type MapValue;

  // Maps the high address of each range to a EntryType.
  AddressToRangeMap map_;
};


}  // namespace google_airbag


#endif  // PROCESSOR_RANGE_MAP_H__
