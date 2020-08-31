// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SIMPLE_FRACTION_H_
#define UTIL_SIMPLE_FRACTION_H_

#include <string>

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

namespace openscreen {

// SimpleFraction is used to represent simple (or "common") fractions, composed
// of a rational number written a/b where a and b are both integers.

// Note: Since SimpleFraction is a trivial type, it comes with a
// default constructor and is copyable, as well as allowing static
// initialization.

// Some helpful notes on SimpleFraction assumptions/limitations:
// 1. SimpleFraction does not perform reductions. 2/4 != 1/2, and -1/-1 != 1/1.
// 2. denominator = 0 is considered undefined.
// 3. numerator = saturates range to int min or int max
// 4. A SimpleFraction is "positive" if and only if it is defined and at least
//    equal to zero. Since reductions are not performed, -1/-1 is negative.
struct SimpleFraction {
  static ErrorOr<SimpleFraction> FromString(absl::string_view value);
  std::string ToString() const;

  bool operator==(const SimpleFraction& other) const;
  bool operator!=(const SimpleFraction& other) const;

  bool is_defined() const;
  bool is_positive() const;
  explicit operator double() const;

  int numerator = 0;
  int denominator = 0;
};

}  // namespace openscreen

#endif  // UTIL_SIMPLE_FRACTION_H_
