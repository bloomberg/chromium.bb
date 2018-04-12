// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGCaretRect_h
#define NGCaretRect_h

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/optional.h"

namespace blink {

// This file provides utility functions for computing caret rect in LayoutNG.

class NGPaintFragment;
class LayoutBlockFlow;
struct LocalCaretRect;

// Given an inline formatting context and a position in the context, returns the
// caret rect if a caret should be placed at the position, with the given
// affinity. The caret rect location is local to the given formatting context.
CORE_EXPORT LocalCaretRect ComputeNGLocalCaretRect(const LayoutBlockFlow&,
                                                   const PositionWithAffinity&);

// An NGCaretPosition indicates a caret position relative to an inline
// NGPaintFragment:
// - When |fragment| is box, |position_type| is either |kBeforeBox| or
// |kAfterBox|, indicating either of the two caret positions by the box sides;
// |text_offset| is |nullopt| in this case.
// - When |fragment| is text, |position_type| is |kAtTextOffset|, and
// |text_offset| is in the text offset range of the fragment.
//
// TODO(xiaochengh): Support "in empty container" caret type

enum class NGCaretPositionType { kBeforeBox, kAfterBox, kAtTextOffset };
struct NGCaretPosition {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  bool IsNull() const { return !fragment; }

  const NGPaintFragment* fragment = nullptr;  // owned by root LayoutNGMixin
  NGCaretPositionType position_type;
  Optional<unsigned> text_offset;
};

// Given an inline formatting context, a text offset in the context and a text
// affinity, returns the corresponding NGCaretPosition, or null if not found.
// Note that in many cases, null result indicates that we have reached an
// unexpected case that is not properly handled.
CORE_EXPORT NGCaretPosition ComputeNGCaretPosition(const LayoutBlockFlow&,
                                                   unsigned,
                                                   TextAffinity);

}  // namespace blink

#endif  // NGCaretRect_h
