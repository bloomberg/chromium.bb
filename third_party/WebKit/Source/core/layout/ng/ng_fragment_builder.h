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
class NGPhysicalLineBoxFragment;

class CORE_EXPORT NGFragmentBuilder final : public NGContainerFragmentBuilder {
  DISALLOW_NEW();

 public:
  NGFragmentBuilder(NGLayoutInputNode,
                    scoped_refptr<const ComputedStyle>,
                    WritingMode,
                    TextDirection);

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGFragmentBuilder(LayoutObject*,
                    scoped_refptr<const ComputedStyle>,
                    WritingMode,
                    TextDirection);

  ~NGFragmentBuilder() override;

  using WeakBoxList = PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGLogicalSize Size() const final { return {inline_size_, block_size_}; }

  NGFragmentBuilder& SetIntrinsicBlockSize(LayoutUnit);

  using NGContainerFragmentBuilder::AddChild;

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

  void AddOutOfFlowLegacyCandidate(NGBlockNode, const NGStaticPosition&);

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

  // Inline containing block geometry is defined by two fragments:
  // start and end. FragmentPair holds the information needed to compute
  // inline containing block geometry wrt enclosing container block.
  //
  // start_fragment is the start fragment of inline containing block.
  // start_fragment_offset is offset wrt start_linebox_fragment
  // start_linebox_offset is offset of linebox from inline-cb
  // end_fragment/linebox are complementary properties for end fragment.
  struct FragmentPair {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    const NGPhysicalLineBoxFragment* start_linebox_fragment;
    NGLogicalOffset start_linebox_offset;
    const NGPhysicalFragment* start_fragment;
    NGPhysicalOffset start_fragment_offset;
    const NGPhysicalLineBoxFragment* end_linebox_fragment;
    NGLogicalOffset end_linebox_offset;
    const NGPhysicalFragment* end_fragment;
    NGPhysicalOffset end_fragment_offset;
  };

  void ComputeInlineContainerFragments(
      HashMap<const LayoutObject*, FragmentPair>* inline_container_fragments,
      NGLogicalSize* container_size);

 private:
  NGLayoutInputNode node_;
  LayoutObject* layout_object_;

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
