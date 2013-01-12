// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_UNIQUE_POSITION_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_UNIQUE_POSITION_H_

#include <string>

#include "base/basictypes.h"

namespace sync_pb {
class UniquePosition;
}

namespace syncer {

// A class to represent positions.
//
// Valid UniquePosition objects have the following properties:
//
//  - a < b and b < c implies a < c (transitivity);
//  - exactly one of a < b, b < a and a = b holds (trichotomy);
//  - if a < b, there is a UniquePosition such that a < x < b (density);
//  - there are UniquePositions x and y such that x < a < y (unboundedness);
//  - if a and b were constructed with different unique suffixes, then a != b.
//
// As long as all UniquePositions used to sort a list were created with unique
// suffixes, then if any item changes its position in the list, only its
// UniquePosition value has to change to represent the new order, and all other
// values can stay the same.
//
// Note that the unique suffixes must be exactly |kSuffixLength| bytes long.
//
// The cost for all these features is potentially unbounded space usage.  In
// practice, however, most ordinals should be not much longer than the suffix.
//
// This class currently has several bookmarks-related assumptions built in,
// though it could be adapted to be more generally useful.
class UniquePosition {
 public:
  static const size_t kSuffixLength;

  static bool IsValidSuffix(const std::string& suffix);
  static bool IsValidBytes(const std::string& bytes);

  // Returns an invalid position.
  static UniquePosition CreateInvalid();

  // Converts from a 'sync_pb::UniquePosition' protobuf to a UniquePosition.
  static UniquePosition FromProto(const sync_pb::UniquePosition& proto);

  // Creates a position with the given suffix.  Ordering among positions created
  // from this function is the same as that of the integer parameters that were
  // passed in.
  static UniquePosition FromInt64(int64 i, const std::string& suffix);

  // Returns a valid position.  Its ordering is not defined.
  static UniquePosition InitialPosition(const std::string& suffix);

  // Returns positions compare smaller than, greater than, or between the input
  // positions.
  static UniquePosition Before(const UniquePosition& x,
                               const std::string& suffix);
  static UniquePosition After(const UniquePosition& x,
                              const std::string& suffix);
  static UniquePosition Between(const UniquePosition& before,
                                const UniquePosition& after,
                                const std::string& suffix);

  // This constructor creates an invalid value.
  UniquePosition();

  bool LessThan(const UniquePosition& other) const;
  bool Equals(const UniquePosition& other) const;

  // Serializes the position's internal state to a protobuf.
  void ToProto(sync_pb::UniquePosition* proto) const;

  // Returns a human-readable representation of this item's internal state.
  std::string ToDebugString() const;

  // Returns the suffix.
  std::string GetSuffixForTest() const;

  // Performs a lossy conversion to an int64 position.  Positions converted to
  // and from int64s using this and the FromInt64 function should maintain their
  // relative orderings unless the int64 values conflict.
  int64 ToInt64() const;

  bool IsValid() const;

 private:
  friend class UniquePositionTest;

  // Returns a string X such that (X ++ |suffix|) < |str|.
  // |str| must be a trailing substring of a valid ordinal.
  // |suffix| must be a valid unique suffix.
  static std::string FindSmallerWithSuffix(const std::string& str,
                                           const std::string& suffix);
  // Returns a string X such that (X ++ |suffix|) > |str|.
  // |str| must be a trailing substring of a valid ordinal.
  // |suffix| must be a valid unique suffix.
  static std::string FindGreaterWithSuffix(const std::string& str,
                                           const std::string& suffix);
  // Returns a string X such that |before| < (X ++ |suffix|) < |after|.
  // |before| and after must be a trailing substrings of valid ordinals.
  // |suffix| must be a valid unique suffix.
  static std::string FindBetweenWithSuffix(const std::string& before,
                                           const std::string& after,
                                           const std::string& suffix);

  explicit UniquePosition(const std::string& internal_rep);
  UniquePosition(const std::string& prefix, const std::string& suffix);

  std::string bytes_;
  bool is_valid_;
};

}  // namespace syncer;

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_UNIQUE_POSITION_H_
