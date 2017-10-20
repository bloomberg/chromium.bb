// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/wtf/Vector.h"

namespace blink {

class NGConstraintSpace;
class NGInlineBreakToken;
class NGInlineNode;
class NGInlineItem;
class NGLineBoxFragmentBuilder;
class NGTextFragmentBuilder;

// A class for inline layout (e.g. a <span> with no special style).
//
// This class determines the position of NGInlineItem and build line boxes.
//
// Uses NGLineBreaker to find NGInlineItems to form a line.
class CORE_EXPORT NGInlineLayoutAlgorithm final
    : public NGLayoutAlgorithm<NGInlineNode,
                               NGFragmentBuilder,
                               NGInlineBreakToken> {
 public:
  NGInlineLayoutAlgorithm(NGInlineNode,
                          const NGConstraintSpace&,
                          NGInlineBreakToken* = nullptr);

  // Create a line.
  // @return false if the line does not fit in the constraint space in block
  //         direction.
  bool CreateLine(NGLineInfo*,
                  NGExclusionSpace*,
                  scoped_refptr<NGInlineBreakToken> = nullptr);

  scoped_refptr<NGLayoutResult> Layout() override;

 private:
  bool IsHorizontalWritingMode() const { return is_horizontal_writing_mode_; }

  void BidiReorder(NGInlineItemResults*);

  bool PlaceItems(NGLineInfo*,
                  const NGExclusionSpace&,
                  scoped_refptr<NGInlineBreakToken>);
  void PlaceText(scoped_refptr<const ShapeResult>,
                 scoped_refptr<const ComputedStyle>,
                 LayoutUnit* position,
                 NGInlineBoxState*,
                 NGTextFragmentBuilder*,
                 NGLineBoxFragmentBuilder*);
  void PlaceGeneratedContent(scoped_refptr<const ShapeResult>,
                             scoped_refptr<const ComputedStyle>,
                             LayoutUnit* position,
                             NGInlineBoxState*,
                             NGTextFragmentBuilder*,
                             NGLineBoxFragmentBuilder*);
  NGInlineBoxState* PlaceAtomicInline(const NGInlineItem&,
                                      NGInlineItemResult*,
                                      const NGLineInfo&,
                                      LayoutUnit position,
                                      NGLineBoxFragmentBuilder*);
  void PlaceLayoutResult(NGInlineItemResult*,
                         LayoutUnit position,
                         NGInlineBoxState*,
                         NGLineBoxFragmentBuilder*);
  void PlaceListMarker(const NGInlineItem&,
                       NGInlineItemResult*,
                       const NGLineInfo&,
                       NGLineBoxFragmentBuilder*);

  void ApplyTextAlign(const NGLineInfo&,
                      ETextAlign,
                      LayoutUnit* line_left,
                      LayoutUnit inline_size);
  bool ApplyJustify(NGLineInfo*);

  LayoutUnit ComputeContentSize(const NGLineInfo&,
                                const NGExclusionSpace&,
                                LayoutUnit line_bottom);

  void PropagateBaselinesFromChildren();
  bool AddBaseline(const NGBaselineRequest&,
                   const NGPhysicalFragment*,
                   LayoutUnit child_offset);

  NGInlineLayoutStateStack box_states_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  FontBaseline baseline_type_ = FontBaseline::kAlphabeticBaseline;

  unsigned is_horizontal_writing_mode_ : 1;
  unsigned quirks_mode_ : 1;

  std::unique_ptr<NGExclusionSpace> exclusion_space_;
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
