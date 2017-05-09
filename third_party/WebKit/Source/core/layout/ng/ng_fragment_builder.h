// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class NGLayoutResult;

class CORE_EXPORT NGFragmentBuilder final {
  DISALLOW_NEW();

 public:
  NGFragmentBuilder(NGPhysicalFragment::NGFragmentType, NGLayoutInputNode*);

  using WeakBoxList = PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>;

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

  NGFragmentBuilder& SetBfcOffset(const NGLogicalOffset& offset);

  NGFragmentBuilder& AddUnpositionedFloat(
      RefPtr<NGFloatingObject> floating_object);

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
  NGFragmentBuilder& AddOutOfFlowChildCandidate(NGBlockNode*, NGLogicalOffset);

  void GetAndClearOutOfFlowDescendantCandidates(WeakBoxList*,
                                                Vector<NGStaticPosition>*);

  NGFragmentBuilder& AddOutOfFlowDescendant(NGBlockNode*,
                                            const NGStaticPosition&);

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

  Vector<RefPtr<NGPhysicalFragment>>& MutableChildren() { return children_; }

  Vector<NGLogicalOffset>& MutableOffsets() { return offsets_; }

  // Mutable list of floats that need to be positioned.
  Vector<RefPtr<NGFloatingObject>>& MutableUnpositionedFloats() {
    return unpositioned_floats_;
  }

  // List of floats that need to be positioned.
  const Vector<RefPtr<NGFloatingObject>>& UnpositionedFloats() const {
    return unpositioned_floats_;
  }

  // Mutable list of positioned floats, i.e. floats with logical_offset set.
  Vector<NGPositionedFloat>& MutablePositionedFloats() {
    return positioned_floats_;
  }

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const {
    return bfc_offset_;
  }

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  bool DidBreak() const { return did_break_; }

 private:
  // Out-of-flow descendant placement information.
  // The generated fragment must compute NGStaticPosition for all
  // out-of-flow descendants.
  // The resulting NGStaticPosition gets derived from:
  // 1. The offset of fragment's child.
  // 2. The static position of descendant wrt child.
  //
  // A child can be:
  // 1. A descendant itself. In this case, descendant position is (0,0).
  // 2. A fragment containing a descendant.
  //
  // child_offset is stored as NGLogicalOffset because physical offset cannot
  // be computed until we know fragment's size.
  struct OutOfFlowPlacement {
    NGLogicalOffset child_offset;
    NGStaticPosition descendant_position;
  };

  NGPhysicalFragment::NGFragmentType type_;
  NGWritingMode writing_mode_;
  TextDirection direction_;

  Persistent<NGLayoutInputNode> node_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;

  bool did_break_;
  LayoutUnit used_block_size_;

  Vector<RefPtr<NGBreakToken>> child_break_tokens_;
  RefPtr<NGBreakToken> last_inline_break_token_;

  WeakBoxList out_of_flow_descendant_candidates_;
  Vector<OutOfFlowPlacement> out_of_flow_candidate_placements_;

  WeakBoxList out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;

  // Floats that need to be positioned by the next in-flow fragment that can
  // determine its block position in space.
  Vector<RefPtr<NGFloatingObject>> unpositioned_floats_;

  Vector<NGPositionedFloat> positioned_floats_;

  WTF::Optional<NGLogicalOffset> bfc_offset_;
  NGMarginStrut end_margin_strut_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
