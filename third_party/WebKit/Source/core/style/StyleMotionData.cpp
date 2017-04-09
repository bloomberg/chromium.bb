// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleMotionData.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool StyleMotionData::operator==(const StyleMotionData& o) const {
  if (anchor_ != o.anchor_ || position_ != o.position_ ||
      distance_ != o.distance_ || rotation_ != o.rotation_)
    return false;

  return DataEquivalent(path_, o.path_);
}

}  // namespace blink
