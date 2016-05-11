// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
#define UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_

#include "ui/display/display.h"

namespace display {

inline bool operator==(const Display& lhs, const Display& rhs) {
  return lhs.id() == rhs.id() &&
         lhs.bounds() == rhs.bounds() &&
         lhs.work_area() == rhs.work_area() &&
         lhs.device_scale_factor() == rhs.device_scale_factor() &&
         lhs.rotation() == rhs.rotation() &&
         lhs.touch_support() == rhs.touch_support();
}

void PrintTo(const Display& display, ::std::ostream* os) {
  *os << display.ToString();
}

}  // namespace display

#endif  // UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
