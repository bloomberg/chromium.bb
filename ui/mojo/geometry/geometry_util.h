// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MOJO_GEOMETRY_GEOMETRY_UTIL_H_
#define UI_MOJO_GEOMETRY_GEOMETRY_UTIL_H_

#include "ui/mojo/geometry/geometry.mojom.h"

namespace mojo {

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
         lhs.height == lhs.height;
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const Point& lhs, const Point& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const Point& lhs, const Point& rhs) {
  return !(lhs == rhs);
}

}

#endif  // UI_MOJO_GEOMETRY_GEOMETRY_UTIL_H_
