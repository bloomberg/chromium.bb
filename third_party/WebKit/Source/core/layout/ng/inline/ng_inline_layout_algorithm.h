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
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
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
struct NGPositionedFloat;

// A class for laying out an inline formatting context, i.e. a block with inline
// children.
//
// This class determines the position of NGInlineItem and build line boxes.
//
// Uses NGLineBreaker to find NGInlineItems to form a line.
class CORE_EXPORT NGInlineLayoutAlgorithm final
    : public NGLayoutAlgorithm<NGInlineNode,
                               NGLineBoxFragmentBuilder,
                               NGInlineBreakToken> {
 public:
  NGInlineLayoutAlgorithm(NGInlineNode,
                          const NGConstraintSpace&,
                          NGInlineBreakToken* = nullptr);

  void CreateLine(NGLineInfo*, NGExclusionSpace*);

  scoped_refptr<NGLayoutResult> Layout() override;

 private:
  unsigned PositionLeadingItems(NGExclusionSpace*);
  void PositionPendingFloats(LayoutUnit content_size, NGExclusionSpace*);

  bool IsHorizontalWritingMode() const { return is_horizontal_writing_mode_; }

  void BidiReorder();

  void PlaceText(scoped_refptr<const ShapeResult>,
                 scoped_refptr<const ComputedStyle>,
                 UBiDiLevel bidi_level,
                 NGInlineBoxState*,
                 NGTextFragmentBuilder*);
  void PlaceGeneratedContent(scoped_refptr<const ShapeResult>,
                             scoped_refptr<const ComputedStyle>,
                             NGInlineBoxState*,
                             NGTextFragmentBuilder*);
  NGInlineBoxState* PlaceAtomicInline(const NGInlineItem&,
                                      NGInlineItemResult*,
                                      const NGLineInfo&);
  void PlaceLayoutResult(NGInlineItemResult*,
                         NGInlineBoxState*,
                         LayoutUnit inline_offset = LayoutUnit());
  void PlaceListMarker(const NGInlineItem&,
                       NGInlineItemResult*,
                       const NGLineInfo&);

  void ApplyTextAlign(const NGLineInfo&,
                      ETextAlign,
                      LayoutUnit* line_left,
                      LayoutUnit inline_size);
  bool ApplyJustify(NGLineInfo*);

  LayoutUnit ComputeContentSize(const NGLineInfo&,
                                const NGExclusionSpace&,
                                LayoutUnit line_height);

  NGLineBoxFragmentBuilder::ChildList line_box_;
  std::unique_ptr<NGInlineLayoutStateStack> box_states_;

  FontBaseline baseline_type_ = FontBaseline::kAlphabeticBaseline;

  unsigned is_horizontal_writing_mode_ : 1;
  unsigned quirks_mode_ : 1;

  Vector<NGPositionedFloat> positioned_floats_;
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
