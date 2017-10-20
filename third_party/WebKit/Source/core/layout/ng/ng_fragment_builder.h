// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/geometry/ng_border_edges.h"
#include "core/layout/ng/geometry/ng_physical_rect.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_container_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class NGPhysicalFragment;

class CORE_EXPORT NGFragmentBuilder final : public NGContainerFragmentBuilder {
  DISALLOW_NEW();

 public:
  NGFragmentBuilder(NGLayoutInputNode,
                    scoped_refptr<const ComputedStyle>,
                    NGWritingMode,
                    TextDirection);

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGFragmentBuilder(LayoutObject*,
                    scoped_refptr<const ComputedStyle>,
                    NGWritingMode,
                    TextDirection);

  ~NGFragmentBuilder();

  using WeakBoxList = PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGFragmentBuilder& SetBlockSize(LayoutUnit);
  NGLogicalSize Size() const final { return {inline_size_, block_size_}; }

  NGFragmentBuilder& SetIntrinsicBlockSize(LayoutUnit);

  using NGContainerFragmentBuilder::AddChild;

  NGContainerFragmentBuilder& AddChild(scoped_refptr<NGLayoutResult>,
                                       const NGBfcOffset& child_bfc_offset,
                                       const NGBfcOffset& parent_bfc_offset);

  // Our version of AddChild captures any child NGBreakTokens.
  NGContainerFragmentBuilder& AddChild(scoped_refptr<NGPhysicalFragment>,
                                       const NGLogicalOffset&) final;

  // Add a break token for a child that doesn't yet have any fragments, because
  // its first fragment is to be produced in the next fragmentainer. This will
  // add a break token for the child, but no fragment.
  NGFragmentBuilder& AddBreakBeforeChild(NGLayoutInputNode child);

  // Update if we have fragmented in this flow.
  NGFragmentBuilder& PropagateBreak(scoped_refptr<NGLayoutResult>);
  NGFragmentBuilder& PropagateBreak(scoped_refptr<NGPhysicalFragment>);

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

  void AddOutOfFlowLegacyCandidate(NGBlockNode, const NGStaticPosition&);

  void GetAndClearOutOfFlowDescendantCandidates(
      Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates);

  // Set how much of the block size we've used so far for this box.
  NGFragmentBuilder& SetUsedBlockSize(LayoutUnit used_block_size) {
    used_block_size_ = used_block_size;
    return *this;
  }

  // Specify that we broke.
  //
  // This will result in a fragment which has an unfinished break token.
  NGFragmentBuilder& SetDidBreak() {
    did_break_ = true;
    return *this;
  }

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  scoped_refptr<NGLayoutResult> ToBoxFragment();

  scoped_refptr<NGLayoutResult> Abort(NGLayoutResult::NGLayoutResultStatus);

  // A vector of child offsets. Initially set by AddChild().
  const Vector<NGLogicalOffset>& Offsets() const { return offsets_; }
  Vector<NGLogicalOffset>& MutableOffsets() { return offsets_; }

  NGPhysicalFragment::NGBoxType BoxType() const;
  NGFragmentBuilder& SetBoxType(NGPhysicalFragment::NGBoxType);

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
  NGLayoutInputNode node_;
  LayoutObject* layout_object_;

  LayoutUnit block_size_;
  LayoutUnit intrinsic_block_size_;

  NGPhysicalFragment::NGBoxType box_type_;
  bool did_break_;
  LayoutUnit used_block_size_;

  Vector<scoped_refptr<NGBreakToken>> child_break_tokens_;
  scoped_refptr<NGBreakToken> last_inline_break_token_;

  Vector<NGBaseline> baselines_;

  NGBorderEdges border_edges_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
