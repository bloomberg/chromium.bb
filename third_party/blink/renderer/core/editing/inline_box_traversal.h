// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_TRAVERSAL_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class InlineBox;

// This class provides common traveral functions on list of |InlineBox|.
class InlineBoxTraversal final {
  STATIC_ONLY(InlineBoxTraversal);

 public:
  // TODO(yosin): We should take |bidi_level| from |InlineBox::BidiLevel()|,
  // once all call sites satisfy it.

  // Traverses left/right from |box|, and returns the first box with bidi level
  // less than or equal to |bidi_level| (excluding |box| itself). Returns
  // |nullptr| when such a box doesn't exist.
  static InlineBox* FindLeftBidiRun(const InlineBox& box, unsigned bidi_level);
  static InlineBox* FindRightBidiRun(const InlineBox& box, unsigned bidi_level);

  // Traverses left/right from |box|, and returns the last non-linebreak box
  // with bidi level greater than |bidi_level| (including |box| itself).
  static InlineBox* FindLeftBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox& box,
      unsigned bidi_level);
  static InlineBox* FindRightBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox& box,
      unsigned bidi_level);

  // Traverses left/right from |box|, and returns the last box with bidi level
  // greater than or equal to |bidi_level| (including |box| itself).
  static InlineBox* FindLeftBoundaryOfEntireBidiRun(const InlineBox& box,
                                                    unsigned bidi_level);
  static InlineBox* FindRightBoundaryOfEntireBidiRun(const InlineBox& box,
                                                     unsigned bidi_level);

  // Variants of the above two where line break boxes are ignored.
  static InlineBox* FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);
  static InlineBox* FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox&,
      unsigned bidi_level);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_TRAVERSAL_H_
