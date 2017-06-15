// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QuadLengthValue_h
#define QuadLengthValue_h

#include <memory>
#include "platform/Length.h"

namespace blink {

struct QuadLengthValue {
  DISALLOW_NEW();

  QuadLengthValue() {}

  explicit QuadLengthValue(Length length)
      : top(length), right(length), bottom(length), left(length) {}

  QuadLengthValue(const QuadLengthValue& other)
      : top(other.top),
        right(other.right),
        bottom(other.bottom),
        left(other.left) {}

  bool operator==(const QuadLengthValue& other) const {
    return top == other.top && right == other.right && bottom == other.bottom &&
           left == other.left;
  }

  bool operator!=(const QuadLengthValue& other) const {
    return !(*this == other);
  }

  Length top;
  Length right;
  Length bottom;
  Length left;
};

}  // namespace blink

#endif  // QuadLengthValue_h
