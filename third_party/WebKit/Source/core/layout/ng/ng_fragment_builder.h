// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/geometry/ng_static_position.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CORE_EXPORT NGFragmentBuilder final {
  DISALLOW_NEW();

 public:
  NGFragmentBuilder(NGPhysicalFragment::NGFragmentType, NGLayoutInputNode);

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGFragmentBuilder(NGPhysicalFragment::NGFragmentType, LayoutObject*);

  using WeakBoxList = PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGWritingMode WritingMode() const { return writing_mode_; }
  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(TextDirection);

  NGFragmentBuilder& SetSize(const NGLogicalSize&);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);
  NGLogicalSize Size() const { return size_; }

  NGFragmentBuilder& SetOverflowSize(const NGLogicalSize&);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(RefPtr<NGLayoutResult>, const NGLogicalOffset&);
  NGFragmentBuilder& AddChild(RefPtr<NGPhysicalFragment>,
                              const NGLogicalOffset&);

  NGFragmentBuilder& AddPositionedFloat(NGPositionedFloat);

  NGFragmentBuilder& SetBfcOffset(const NGLogicalOffset& offset);

  // Builder has non-trivial out-of-flow descendant methods.
  // These methods are building blocks for implementation of
  // out-of-flow descendants by layout algorithms.
  //
  // They are intended to be used by layout algorithm like this:
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
  // Part 2: Out-of-flow layout part positions out-of-flow descendants.
  //
  // NGOutOfFlowLayoutPart(container_style, builder).Run();
  //
  // See layout part for builder interaction.
  NGFragmentBuilder& AddOutOfFlowChildCandidate(NGBlockNode,
                                                const NGLogicalOffset&);

  void GetAndClearOutOfFlowDescendantCandidates(
      Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates);

  NGFragmentBuilder& AddOutOfFlowDescendant(NGOutOfFlowPositionedDescendant);

  // Sets how much of the block size we've used so far for this box.
  //
  // This will result in a fragment which has an unfinished break token, which
  // contains this information.
  NGFragmentBuilder& SetUsedBlockSize(LayoutUnit used_block_size) {
    used_block_size_ = used_block_size;
    did_break_ = true;
    return *this;
  }

  NGFragmentBuilder& SetEndMarginStrut(const NGMarginStrut& from) {
    end_margin_strut_ = from;
    return *this;
  }

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  RefPtr<NGLayoutResult> ToBoxFragment();

  RefPtr<NGLayoutResult> Abort(NGLayoutResult::NGLayoutResultStatus);

  // A vector of child offsets. Initially set by AddChild().
  const Vector<NGLogicalOffset>& Offsets() const { return offsets_; }
  Vector<NGLogicalOffset>& MutableOffsets() { return offsets_; }

  void SwapUnpositionedFloats(
      Vector<RefPtr<NGUnpositionedFloat>>* unpositioned_floats) {
    unpositioned_floats_.swap(*unpositioned_floats);
  }

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const {
    return bfc_offset_;
  }

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  bool DidBreak() const { return did_break_; }

  NGFragmentBuilder& SetBorderEdges(NGBorderEdges border_edges) {
    border_edges_ = border_edges;
    return *this;
  }

  // Layout algorithms should call this function for each baseline request in
  // the constraint space.
  //
  // If a request should use a synthesized baseline from the box rectangle,
  // algorithms can omit the call.
  //
  // This function should be called at most once for a given algorithm/baseline
  // type pair.
  void AddBaseline(NGBaselineRequest, LayoutUnit);

 private:
  // An out-of-flow positioned-candidate is a temporary data structure used
  // within the NGFragmentBuilder.
  //
  // A positioned-candidate can be:
  // 1. A direct out-of-flow positioned child. The child_offset is (0,0).
  // 2. A fragment containing an out-of-flow positioned-descendant. The
  //    child_offset in this case is the containing fragment's offset.
  //
  // The child_offset is stored as a NGLogicalOffset as the physical offset
  // cannot be computed until we know the current fragment's size.
  //
  // When returning the positioned-candidates (from
  // GetAndClearOutOfFlowDescendantCandidates), the NGFragmentBuilder will
  // convert the positioned-candidate to a positioned-descendant using the
  // physical size the fragment builder.
  struct NGOutOfFlowPositionedCandidate {
    NGOutOfFlowPositionedDescendant descendant;
    NGLogicalOffset child_offset;
  };

  NGPhysicalFragment::NGFragmentType type_;
  NGWritingMode writing_mode_;
  TextDirection direction_;

  NGLayoutInputNode node_;
  LayoutObject* layout_object_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;

  bool did_break_;
  LayoutUnit used_block_size_;

  Vector<RefPtr<NGBreakToken>> child_break_tokens_;
  RefPtr<NGBreakToken> last_inline_break_token_;

  Vector<NGOutOfFlowPositionedCandidate> oof_positioned_candidates_;
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants_;

  // Floats that need to be positioned by the next in-flow fragment that can
  // determine its block position in space.
  Vector<RefPtr<NGUnpositionedFloat>> unpositioned_floats_;

  Vector<NGPositionedFloat> positioned_floats_;

  WTF::Optional<NGLogicalOffset> bfc_offset_;
  NGMarginStrut end_margin_strut_;

  Vector<NGBaseline> baselines_;

  NGBorderEdges border_edges_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
