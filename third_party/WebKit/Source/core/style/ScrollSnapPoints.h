// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollSnapPoints_h
#define ScrollSnapPoints_h

#include <memory>
#include "platform/Length.h"

namespace blink {

struct ScrollSnapPoints {
  DISALLOW_NEW();

  ScrollSnapPoints()
      : repeat_offset(100, kPercent), has_repeat(false), uses_elements(false) {}

  bool operator==(const ScrollSnapPoints& other) const {
    return repeat_offset == other.repeat_offset &&
           has_repeat == other.has_repeat &&
           uses_elements == other.uses_elements;
  }
  bool operator!=(const ScrollSnapPoints& other) const {
    return !(*this == other);
  }

  Length repeat_offset;
  bool has_repeat;
  bool uses_elements;
};

}  // namespace blink

#endif  // ScrollSnapPoints_h
