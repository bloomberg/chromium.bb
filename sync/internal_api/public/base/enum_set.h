// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_ENUM_SET_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_ENUM_SET_H_

#include <bitset>
#include <cstddef>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"

namespace syncer {

// Forward declarations needed for friend declarations.
template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet;

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                           EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                  EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
                                EnumSet<E, Min, Max> set2);

// An EnumSet is a set that can hold enum values between a min and a
// max value (inclusive of both).  It's essentially a wrapper around
// std::bitset<> with stronger type enforcement, more descriptive
// member function names, and an iterator interface.
//
// If you're working with enums with a small number of possible values
// (say, fewer than 64), you can efficiently pass around an EnumSet
// for that enum around by value.

template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet {
 public:
  typedef E EnumType;
  static const E kMinValue = MinEnumValue;
  static const E kMaxValue = MaxEnumValue;
  static const size_t kValueCount = kMaxValue - kMinValue + 1;
  COMPILE_ASSERT(kMinValue < kMaxValue,
                 min_value_must_be_less_than_max_value);

 private:
  // Declaration needed by Iterator.
  typedef std::bitset<kValueCount> EnumBitSet;

 public:
  // Iterator is a forward-only read-only iterator for EnumSet.  Its
  // interface is deliberately distinct from an STL iterator as its
  // semantics are substantially different.
  //
  // Example usage:
  //
  // for (EnumSet<...>::Iterator it = enums.First(); it.Good(); it.Inc()) {
  //   Process(it.Get());
  // }
  //
  // The iterator must not be outlived by the set.  In particular, the
  // following is an error:
  //
  // EnumSet<...> SomeFn() { ... }
  //
  // /* ERROR */
  // for (EnumSet<...>::Iterator it = SomeFun().First(); ...
  //
  // Also, there are no guarantees as to what will happen if you
  // modify an EnumSet while traversing it with an iterator.
  class Iterator {
   public:
    // A default-constructed iterator can't do anything except check
    // Good().  You need to call First() on an EnumSet to get a usable
    // iterator.
    Iterator() : enums_(NULL), i_(kValueCount) {}
    ~Iterator() {}

    // Copy constructor and assignment welcome.

    // Returns true iff the iterator points to an EnumSet and it
    // hasn't yet traversed the EnumSet entirely.
    bool Good() const {
      return enums_ && i_ < kValueCount && enums_->test(i_);
    }

    // Returns the value the iterator currently points to.  Good()
    // must hold.
    E Get() const {
      CHECK(Good());
      return FromIndex(i_);
    }

    // Moves the iterator to the next value in the EnumSet.  Good()
    // must hold.  Takes linear time.
    void Inc() {
      CHECK(Good());
      i_ = FindNext(i_ + 1);
    }

   private:
    friend Iterator EnumSet::First() const;

    explicit Iterator(const EnumBitSet& enums)
        : enums_(&enums), i_(FindNext(0)) {}

    size_t FindNext(size_t i) {
      while ((i < kValueCount) && !enums_->test(i)) {
        ++i;
      }
      return i;
    }

    const EnumBitSet* enums_;
    size_t i_;
  };

  // You can construct an EnumSet with 0, 1, 2, or 3 initial values.

  EnumSet() {}

  explicit EnumSet(E value) {
    Put(value);
  }

  EnumSet(E value1, E value2) {
    Put(value1);
    Put(value2);
  }

  EnumSet(E value1, E value2, E value3) {
    Put(value1);
    Put(value2);
    Put(value3);
  }

  // Returns an EnumSet with all possible values.
  static EnumSet All() {
    EnumBitSet enums;
    enums.set();
    return EnumSet(enums);
  }

  ~EnumSet() {}

  // Copy constructor and assignment welcome.

  // Set operations.  Put, Retain, and Remove are basically
  // self-mutating versions of Union, Intersection, and Difference
  // (defined below).

  // Adds the given value (which must be in range) to our set.
  void Put(E value) {
    enums_.set(ToIndex(value));
  }

  // Adds all values in the given set to our set.
  void PutAll(EnumSet other) {
    enums_ |= other.enums_;
  }

  // There's no real need for a Retain(E) member function.

  // Removes all values not in the given set from our set.
  void RetainAll(EnumSet other) {
    enums_ &= other.enums_;
  }

  // If the given value is in range, removes it from our set.
  void Remove(E value) {
    if (InRange(value)) {
      enums_.reset(ToIndex(value));
    }
  }

  // Removes all values in the given set from our set.
  void RemoveAll(EnumSet other) {
    enums_ &= ~other.enums_;
  }

  // Removes all values from our set.
  void Clear() {
    enums_.reset();
  }

  // Returns true iff the given value is in range and a member of our
  // set.
  bool Has(E value) const {
    return InRange(value) && enums_.test(ToIndex(value));
  }

  // Returns true iff the given set is a subset of our set.
  bool HasAll(EnumSet other) const {
    return (enums_ & other.enums_) == other.enums_;
  }

  // Returns true iff our set and the given set contain exactly the
  // same values.
  bool Equals(const EnumSet& other) const {
    return enums_ == other.enums_;
  }

  // Returns true iff our set is empty.
  bool Empty() const {
    return !enums_.any();
  }

  // Returns how many values our set has.
  size_t Size() const {
    return enums_.count();
  }

  // Returns an iterator pointing to the first element (if any).
  Iterator First() const {
    return Iterator(enums_);
  }

 private:
  friend EnumSet Union<E, MinEnumValue, MaxEnumValue>(
      EnumSet set1, EnumSet set2);
  friend EnumSet Intersection<E, MinEnumValue, MaxEnumValue>(
      EnumSet set1, EnumSet set2);
  friend EnumSet Difference<E, MinEnumValue, MaxEnumValue>(
      EnumSet set1, EnumSet set2);

  explicit EnumSet(EnumBitSet enums) : enums_(enums) {}

  static bool InRange(E value) {
    return (value >= MinEnumValue) && (value <= MaxEnumValue);
  }

  // Converts a value to/from an index into |enums_|.

  static size_t ToIndex(E value) {
    DCHECK_GE(value, MinEnumValue);
    DCHECK_LE(value, MaxEnumValue);
    return value - MinEnumValue;
  }

  static E FromIndex(size_t i) {
    DCHECK_LT(i, kValueCount);
    return static_cast<E>(MinEnumValue + i);
  }

  EnumBitSet enums_;
};

template <typename E, E MinEnumValue, E MaxEnumValue>
const E EnumSet<E, MinEnumValue, MaxEnumValue>::kMinValue;

template <typename E, E MinEnumValue, E MaxEnumValue>
const E EnumSet<E, MinEnumValue, MaxEnumValue>::kMaxValue;

template <typename E, E MinEnumValue, E MaxEnumValue>
const size_t EnumSet<E, MinEnumValue, MaxEnumValue>::kValueCount;

// The usual set operations.

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                           EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ | set2.enums_);
}

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                  EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & set2.enums_);
}

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
                                EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & ~set2.enums_);
}

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_ENUM_SET_H_
