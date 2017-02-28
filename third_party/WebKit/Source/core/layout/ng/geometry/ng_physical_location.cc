// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_location.h"

#include "wtf/text/WTFString.h"

namespace blink {

bool NGPhysicalLocation::operator==(const NGPhysicalLocation& other) const {
  return other.left == left && other.top == top;
}

String NGPhysicalLocation::ToString() const {
  return String::format("%dx%d", left.toInt(), top.toInt());
}

}  // namespace blink
