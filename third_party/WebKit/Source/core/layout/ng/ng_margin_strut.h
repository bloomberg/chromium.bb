// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMarginStrut_h
#define NGMarginStrut_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"

namespace blink {

// This struct is used for the margin collapsing calculation.
struct NGMarginStrut {
  LayoutUnit marginBlockStart;
  LayoutUnit marginBlockEnd;

  LayoutUnit negativeMarginBlockStart;
  LayoutUnit negativeMarginBlockEnd;
};

}  // namespace blink

#endif  // NGMarginStrut_h
