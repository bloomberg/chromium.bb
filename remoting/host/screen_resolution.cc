// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_resolution.h"

#include <algorithm>
#include <limits>

namespace remoting {

ScreenResolution::ScreenResolution()
    : dimensions_(SkISize::Make(0, 0)),
      dpi_(SkIPoint::Make(0, 0)) {
}

ScreenResolution::ScreenResolution(const SkISize& dimensions,
                                   const SkIPoint& dpi)
    : dimensions_(dimensions),
      dpi_(dpi) {
}

SkISize ScreenResolution::ScaleDimensionsToDpi(const SkIPoint& new_dpi) const {
  int64 width = dimensions_.width();
  int64 height = dimensions_.height();

  // Scale the screen dimensions to new DPI.
  width = std::min(width * new_dpi.x() / dpi_.x(),
                   static_cast<int64>(std::numeric_limits<int32>::max()));
  height = std::min(height * new_dpi.y() / dpi_.y(),
                    static_cast<int64>(std::numeric_limits<int32>::max()));
  return SkISize::Make(static_cast<int32>(width), static_cast<int32>(height));
}

bool ScreenResolution::IsEmpty() const {
  return dimensions_.isEmpty() || dpi_.x() <= 0 || dpi_.y() <= 0;
}

bool ScreenResolution::IsValid() const {
  return !IsEmpty() || dimensions_.isZero();
}

}  // namespace remoting
