// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOffsetMappingResult_h
#define NGOffsetMappingResult_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutText;

enum class NGOffsetMappingUnitType { kIdentity, kCollapsed, kExpanded };

// An NGOffsetMappingUnit indicates a "simple" offset mapping between dom offset
// range [dom_start, dom_end] on node |owner| and text content offset range
// [text_content_start, text_content_end]. The mapping between them falls in one
// of the following categories, depending on |type|:
// - kIdentity: The mapping between the two ranges is the identity mapping. In
//   other words, the two ranges have the same length, and the offsets are
//   mapped one-to-one.
// - kCollapsed: The mapping is collapsed, namely, |text_content_start| and
//   |text_content_end| are the same, and characters in the dom range are
//   collapsed.
// - kExpanded: The mapping is expanded, namely, |dom_end == dom_start + 1|, and
//   |text_content_end > text_content_start + 1|, indicating that the character
//   in the dom range is expanded into multiple characters.
// See design doc https://goo.gl/CJbxky for details.
struct NGOffsetMappingUnit {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  NGOffsetMappingUnitType type = NGOffsetMappingUnitType::kIdentity;

  // Ideally, we should store |Node| as owner, instead of |LayoutObject|.
  // However, we need to ensure the invariant that, units of the same owner are
  // consecutive in |NGOffsetMappingResult::units|. This might have a subtle
  // conflict with :first-letter if, during inline collection, there there are
  // other strings collected between the first letter and the remaining text of
  // the node.
  // TODO(xiaochengh): Figure out if this the issue really exists. If not, then
  // we should use |Node| as owner.
  const LayoutText* owner = nullptr;

  unsigned dom_start = 0;
  unsigned dom_end = 0;
  unsigned text_content_start = 0;
  unsigned text_content_end = 0;
};

// An NGOffsetMappingResult stores the units of a LayoutNGBlockFlow in sorted
// order in a vector. For each text node, the index range of the units owned by
// the node is also stored.
// See design doc https://goo.gl/CJbxky for details.
struct NGOffsetMappingResult {
  Vector<NGOffsetMappingUnit> units;
  HashMap<const LayoutText*, std::pair<unsigned, unsigned>> ranges;
};

}  // namespace blink

#endif  // NGOffsetMappingResult_h
