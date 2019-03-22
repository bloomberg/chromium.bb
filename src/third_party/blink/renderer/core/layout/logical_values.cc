// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/logical_values.h"

namespace blink {

EClear ResolvedClear(EClear value, TextDirection direction) {
  switch (value) {
    case EClear::kInlineStart:
    case EClear::kInlineEnd: {
      return IsLtr(direction) == (value == EClear::kInlineStart)
                 ? EClear::kLeft
                 : EClear::kRight;
    }
    default:
      return value;
  }
}

EFloat ResolvedFloating(EFloat value, TextDirection direction) {
  switch (value) {
    case EFloat::kInlineStart:
    case EFloat::kInlineEnd: {
      return IsLtr(direction) == (value == EFloat::kInlineStart)
                 ? EFloat::kLeft
                 : EFloat::kRight;
    }
    default:
      return value;
  }
}

EResize ResolvedResize(EResize value, WritingMode writing_mode) {
  switch (value) {
    case EResize::kBlock:
    case EResize::kInline: {
      return IsHorizontalWritingMode(writing_mode) == (value == EResize::kBlock)
                 ? EResize::kVertical
                 : EResize::kHorizontal;
    }
    default:
      return value;
  }
}

}  // namespace blink
