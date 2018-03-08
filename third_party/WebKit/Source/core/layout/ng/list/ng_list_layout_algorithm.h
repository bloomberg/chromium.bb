// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGListLayoutAlgorithm_h
#define NGListLayoutAlgorithm_h

#include "core/CoreExport.h"

#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"

namespace blink {

class NGConstraintSpace;

// Algorithm to layout lists and list-items.
// TODO(kojii): This isn't a real NGLayoutAlgorithm yet. Consider restructuring
// or renaming.
class CORE_EXPORT NGListLayoutAlgorithm final {
 public:
  // Add a fragment for an outside list marker for a block content.
  // Returns true if the list marker was successfully added. False indicates
  // that the child content does not have a baseline to align to, and that
  // caller should try next child, or "WithoutLineBoxes" version.
  static bool AddListMarkerForBlockContent(NGBlockNode,
                                           const NGConstraintSpace&,
                                           const NGPhysicalFragment&,
                                           NGLogicalOffset,
                                           NGFragmentBuilder*);

  // Add a fragment for an outside list marker when the list item has no line
  // boxes.
  // Returns the block size of the list marker.
  static LayoutUnit AddListMarkerWithoutLineBoxes(NGBlockNode,
                                                  const NGConstraintSpace&,
                                                  NGFragmentBuilder*);
};

}  // namespace blink

#endif  // NGListLayoutAlgorithm_h
