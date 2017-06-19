// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollSnap_h
#define ScrollSnap_h

#include "core/style/ComputedStyleConstants.h"
#include "core/style/QuadLengthValue.h"

namespace blink {

using ScrollSnapMargin = QuadLengthValue;

struct ScrollSnapType {
  DISALLOW_NEW();

  ScrollSnapType()
      : is_none(true),
        axis(kSnapAxisBoth),
        strictness(kSnapStrictnessProximity) {}

  ScrollSnapType(const ScrollSnapType& other)
      : is_none(other.is_none),
        axis(other.axis),
        strictness(other.strictness) {}

  bool operator==(const ScrollSnapType& other) const {
    return is_none == other.is_none && axis == other.axis &&
           strictness == other.strictness;
  }

  bool operator!=(const ScrollSnapType& other) const {
    return !(*this == other);
  }

  bool is_none;
  SnapAxis axis;
  SnapStrictness strictness;
};

struct ScrollSnapAlign {
  DISALLOW_NEW();

  ScrollSnapAlign()
      : alignmentX(kSnapAlignmentNone), alignmentY(kSnapAlignmentNone) {}

  ScrollSnapAlign(const ScrollSnapAlign& other)
      : alignmentX(other.alignmentX), alignmentY(other.alignmentY) {}

  bool operator==(const ScrollSnapAlign& other) const {
    return alignmentX == other.alignmentX && alignmentY == other.alignmentY;
  }

  bool operator!=(const ScrollSnapAlign& other) const {
    return !(*this == other);
  }

  SnapAlignment alignmentX;
  SnapAlignment alignmentY;
};

}  // namespace blink

#endif  // ScrollSnap_h
