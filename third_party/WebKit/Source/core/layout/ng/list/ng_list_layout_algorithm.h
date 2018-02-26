// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGListLayoutAlgorithm_h
#define NGListLayoutAlgorithm_h

#include "core/CoreExport.h"

#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"

namespace blink {

class LayoutUnit;
class NGConstraintSpace;
class NGLineBoxFragmentBuilder;
class NGLineInfo;

// Algorithm to layout lists and list-items.
// TODO(kojii): This isn't a real NGLayoutAlgorithm yet. Consider restructuring
// or renaming.
class CORE_EXPORT NGListLayoutAlgorithm final {
 public:
  // Compute and set the inline position to an outside list marker for a line
  // box.
  static void SetListMarkerPosition(const NGConstraintSpace&,
                                    const NGLineInfo&,
                                    LayoutUnit line_width,
                                    NGLineBoxFragmentBuilder::Child*);

  // Add a fragment for an outside list marker for a block content.
  static void AddListMarkerForBlockContent(NGBlockNode,
                                           const NGConstraintSpace&,
                                           const NGPhysicalFragment&,
                                           NGLogicalOffset,
                                           NGFragmentBuilder*);
};

}  // namespace blink

#endif  // NGListLayoutAlgorithm_h
