// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGExclusionSpace;
class NGPhysicalFragment;

class CORE_EXPORT NGContainerFragmentBuilder : public NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  struct ChildWithOffset {
    DISALLOW_NEW();
    ChildWithOffset(LogicalOffset offset,
                    scoped_refptr<const NGPhysicalFragment> fragment)
        : offset(offset), fragment(std::move(fragment)) {}

    // We store logical offsets (instead of the final physical), as we can't
    // convert into the physical coordinate space until we know our final size.
    LogicalOffset offset;
    scoped_refptr<const NGPhysicalFragment> fragment;
  };
  typedef Vector<ChildWithOffset, 4> ChildrenVector;

  LayoutUnit BfcLineOffset() const { return bfc_line_offset_; }
  void SetBfcLineOffset(LayoutUnit bfc_line_offset) {
    bfc_line_offset_ = bfc_line_offset;
  }

  // The BFC block-offset is where this fragment was positioned within the BFC.
  // If it is not set, this fragment may be placed anywhere within the BFC.
  const base::Optional<LayoutUnit>& BfcBlockOffset() const {
    return bfc_block_offset_;
  }
  void SetBfcBlockOffset(LayoutUnit bfc_block_offset) {
    bfc_block_offset_ = bfc_block_offset;
  }
  void ResetBfcBlockOffset() { bfc_block_offset_.reset(); }

  void SetEndMarginStrut(const NGMarginStrut& end_margin_strut) {
    end_margin_strut_ = end_margin_strut;
  }

  void SetExclusionSpace(NGExclusionSpace&& exclusion_space) {
    exclusion_space_ = std::move(exclusion_space);
  }

  const NGUnpositionedListMarker& UnpositionedListMarker() const {
    return unpositioned_list_marker_;
  }
  void SetUnpositionedListMarker(const NGUnpositionedListMarker& marker) {
    DCHECK(!unpositioned_list_marker_ || !marker);
    unpositioned_list_marker_ = marker;
  }

  void AddChild(const NGPhysicalContainerFragment&,
                const LogicalOffset&,
                const LayoutInline* inline_container = nullptr);

  void AddChild(scoped_refptr<const NGPhysicalTextFragment> child,
                const LogicalOffset& offset) {
    AddChildInternal(child, offset);
  }

  const ChildrenVector& Children() const { return children_; }

  // Builder has non-trivial OOF-positioned methods.
  // They are intended to be used by a layout algorithm like this:
  //
  // Part 1: layout algorithm positions in-flow children.
  //   out-of-flow children, and out-of-flow descendants of fragments
  //   are stored inside builder.
  //
  // for (child : children)
  //   if (child->position == (Absolute or Fixed))
  //     builder->AddOutOfFlowChildCandidate(child);
  //   else
  //     fragment = child->Layout()
  //     builder->AddChild(fragment)
  // end
  //
  // builder->SetSize
  //
  // Part 2: Out-of-flow layout part positions OOF-positioned nodes.
  //
  // NGOutOfFlowLayoutPart(container_style, builder).Run();
  //
  // See layout part for builder interaction.
  void AddOutOfFlowChildCandidate(
      NGBlockNode,
      const LogicalOffset& child_offset,
      NGLogicalStaticPosition::InlineEdge =
          NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::BlockEdge = NGLogicalStaticPosition::kBlockStart,
      bool needs_block_offset_adjustment = true);

  // This should only be used for inline-level OOF-positioned nodes.
  // |inline_container_direction| is the current text direction for determining
  // the correct static-position.
  void AddOutOfFlowInlineChildCandidate(
      NGBlockNode,
      const LogicalOffset& child_offset,
      TextDirection inline_container_direction);

  void AddOutOfFlowDescendant(
      const NGLogicalOutOfFlowPositionedNode& descendant);

  void SwapOutOfFlowPositionedCandidates(
      Vector<NGLogicalOutOfFlowPositionedNode>* candidates);

  bool HasOutOfFlowPositionedCandidates() const {
    return !oof_positioned_candidates_.IsEmpty();
  }

  // This method should only be used within the inline layout algorithm. It is
  // used to convert all OOF-positioned candidates to descendants.
  //
  // During the inline layout algorithm, we don't have enough information to
  // position OOF candidates yet, (as a containing box may be split over
  // multiple lines), instead we bubble all the descendants up to the parent
  // block layout algorithm, to perform the final OOF layout and positioning.
  void MoveOutOfFlowDescendantCandidatesToDescendants();

  void SetIsSelfCollapsing() { is_self_collapsing_ = true; }

  void SetIsPushedByFloats() { is_pushed_by_floats_ = true; }
  bool IsPushedByFloats() const { return is_pushed_by_floats_; }

  void ResetAdjoiningObjectTypes() {
    adjoining_object_types_ = kAdjoiningNone;
    has_adjoining_object_descendants_ = false;
  }
  void AddAdjoiningObjectTypes(NGAdjoiningObjectTypes adjoining_object_types) {
    adjoining_object_types_ |= adjoining_object_types;
    has_adjoining_object_descendants_ |= adjoining_object_types;
  }
  void SetAdjoiningObjectTypes(NGAdjoiningObjectTypes adjoining_object_types) {
    adjoining_object_types_ = adjoining_object_types;
  }
  NGAdjoiningObjectTypes AdjoiningObjectTypes() const {
    return adjoining_object_types_;
  }

  void SetHasBlockFragmentation() { has_block_fragmentation_ = true; }

  // Set for any node that establishes a fragmentation context, such as multicol
  // containers.
  void SetIsBlockFragmentationContextRoot() {
    is_fragmentation_context_root_ = true;
  }

  const NGConstraintSpace* ConstraintSpace() const { return space_; }

#if DCHECK_IS_ON()
  String ToString() const;
#endif

 protected:
  friend class NGInlineLayoutStateStack;
  friend class NGLayoutResult;
  friend class NGPhysicalContainerFragment;

  NGContainerFragmentBuilder(NGLayoutInputNode node,
                             scoped_refptr<const ComputedStyle> style,
                             const NGConstraintSpace* space,
                             WritingMode writing_mode,
                             TextDirection direction)
      : NGFragmentBuilder(std::move(style), writing_mode, direction),
        node_(node),
        space_(space) {
    layout_object_ = node.GetLayoutBox();
  }

  void PropagateChildData(const NGPhysicalContainerFragment& child,
                          const LogicalOffset& child_offset,
                          const LayoutInline* inline_container = nullptr);

  void AddChildInternal(scoped_refptr<const NGPhysicalFragment>,
                        const LogicalOffset&);

  NGLayoutInputNode node_;
  const NGConstraintSpace* space_;

  LayoutUnit bfc_line_offset_;
  base::Optional<LayoutUnit> bfc_block_offset_;
  NGMarginStrut end_margin_strut_;
  NGExclusionSpace exclusion_space_;

  Vector<NGLogicalOutOfFlowPositionedNode> oof_positioned_candidates_;
  Vector<NGLogicalOutOfFlowPositionedNode> oof_positioned_descendants_;

  NGUnpositionedListMarker unpositioned_list_marker_;

  ChildrenVector children_;

  // Only used by the NGBoxFragmentBuilder subclass, but defined here to avoid
  // a virtual function call.
  NGBreakTokenVector child_break_tokens_;
  NGBreakTokenVector inline_break_tokens_;

  scoped_refptr<const NGEarlyBreak> early_break_;
  NGBreakAppeal break_appeal_ = kBreakAppealLastResort;

  NGAdjoiningObjectTypes adjoining_object_types_ = kAdjoiningNone;
  bool has_adjoining_object_descendants_ = false;

  bool is_self_collapsing_ = false;
  bool is_pushed_by_floats_ = false;
  bool is_legacy_layout_root_ = false;

  bool has_floating_descendants_for_paint_ = false;
  bool has_orthogonal_flow_roots_ = false;
  bool has_descendant_that_depends_on_percentage_block_size_ = false;
  bool has_block_fragmentation_ = false;
  bool is_fragmentation_context_root_ = false;
  bool may_have_descendant_above_block_start_ = false;

  bool has_oof_candidate_that_needs_block_offset_adjustment_ = false;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGContainerFragmentBuilder::ChildWithOffset)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_
