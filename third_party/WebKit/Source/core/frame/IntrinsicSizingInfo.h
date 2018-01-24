// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntrinsicSizingInfo_h
#define IntrinsicSizingInfo_h

#include "platform/geometry/FloatSize.h"

namespace blink {

struct IntrinsicSizingInfo {
  IntrinsicSizingInfo() : has_width(true), has_height(true) {}

  FloatSize size;
  FloatSize aspect_ratio;
  bool has_width;
  bool has_height;

  void Transpose() {
    size = size.TransposedSize();
    aspect_ratio = aspect_ratio.TransposedSize();
    std::swap(has_width, has_height);
  }
};

}  // namespace blink

#endif  // IntrinsicSizingInfo_h
