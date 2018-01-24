// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIntrinsicSizingInfo_h
#define WebIntrinsicSizingInfo_h

#include "public/platform/WebFloatSize.h"

namespace blink {

struct WebIntrinsicSizingInfo {
  WebIntrinsicSizingInfo() : has_width(true), has_height(true) {}

  WebFloatSize size;
  WebFloatSize aspect_ratio;
  bool has_width;
  bool has_height;
};

}  // namespace blink

#endif  // WebIntrinsicSizingInfo_h
