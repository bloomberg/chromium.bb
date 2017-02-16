// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_writing_mode.h"

#include "core/layout/LayoutObject.h"
#include "wtf/Assertions.h"

namespace blink {

NGWritingMode FromPlatformWritingMode(WritingMode mode) {
  switch (mode) {
    case WritingMode::kHorizontalTb:
      return kHorizontalTopBottom;
    case WritingMode::kVerticalRl:
      return kVerticalRightLeft;
    case WritingMode::kVerticalLr:
      return kVerticalLeftRight;
    default:
      NOTREACHED();
      return kHorizontalTopBottom;
  }
}

bool IsParallelWritingMode(NGWritingMode a, NGWritingMode b) {
  return (a == kHorizontalTopBottom) == (b == kHorizontalTopBottom);
}

}  // namespace blink
