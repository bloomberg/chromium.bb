// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_rect.h"

#include "wtf/text/WTFString.h"

namespace blink {

NGPixelSnappedPhysicalRect NGPhysicalRect::SnapToDevicePixels() const {
  NGPixelSnappedPhysicalRect snappedRect;
  snappedRect.left = roundToInt(location.left);
  snappedRect.top = roundToInt(location.top);
  snappedRect.width = snapSizeToPixel(size.width, location.left);
  snappedRect.height = snapSizeToPixel(size.height, location.top);

  return snappedRect;
}

bool NGPhysicalRect::operator==(const NGPhysicalRect& other) const {
  return other.location == location && other.size == size;
}

String NGPhysicalRect::ToString() const {
  return String::format("%s,%s %sx%s", location.left.toString().ascii().data(),
                        location.top.toString().ascii().data(),
                        size.width.toString().ascii().data(),
                        size.height.toString().ascii().data());
}

std::ostream& operator<<(std::ostream& os, const NGPhysicalRect& value) {
  return os << value.ToString();
}

}  // namespace blink
