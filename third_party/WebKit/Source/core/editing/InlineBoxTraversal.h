// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineBoxTraversal_h
#define InlineBoxTraversal_h

#include "platform/wtf/Allocator.h"

namespace blink {

class InlineBox;

// This class provides common traveral functions on list of |InlineBox|.
class InlineBoxTraversal final {
  STATIC_ONLY(InlineBoxTraversal);

 public:
  // TODO(yosin): We should take |bidi_level| from |InlineBox::BidiLevel()|,
  // once all call sites satisfy it.
  // Find left boundary variations
  static InlineBox* FindLeftBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);
  static InlineBox* FindLeftBoundaryOfEntireBidiRun(const InlineBox&,
                                                    unsigned bidi_level);
  static InlineBox* FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);

  // Find right boundary variations
  static InlineBox* FindRightBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);
  static InlineBox* FindRightBoundaryOfEntireBidiRun(const InlineBox&,
                                                     unsigned bidi_level);
  static InlineBox* FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);
};

}  // namespace blink

#endif  // InlineBoxTraversal_h
