// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class NGFragment;
class NGInlineNode;
class NGPhysicalBoxFragment;
class NGPhysicalTextFragment;

class CORE_EXPORT NGFragmentBuilder final
    : public GarbageCollectedFinalized<NGFragmentBuilder> {
 public:
  NGFragmentBuilder(NGPhysicalFragment::NGFragmentType, LayoutObject*);

  using WeakBoxList = HeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(TextDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);
  NGLogicalSize Size() const { return size_; }

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(NGFragment*, const NGLogicalOffset&);
  NGFragmentBuilder& AddFloatingObject(NGFloatingObject*,
                                       const NGLogicalOffset&);

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
  // builder->SetInlineSize/SetBlockSize
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

  NGFragmentBuilder& AddUnpositionedFloat(NGFloatingObject* floating_object);

  void SetBreakToken(NGBreakToken* token) {
    DCHECK(!break_token_);
    break_token_ = token;
  }
  bool HasBreakToken() const { return break_token_; }

  // Sets MarginStrut for the resultant fragment.
  NGFragmentBuilder& SetMarginStrutBlockStart(
      const NGDeprecatedMarginStrut& from);
  NGFragmentBuilder& SetMarginStrutBlockEnd(
      const NGDeprecatedMarginStrut& from);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGPhysicalBoxFragment* ToBoxFragment();
  NGPhysicalTextFragment* ToTextFragment(NGInlineNode*,
                                         unsigned start_index,
                                         unsigned end_index);

  // List of floats that need to be positioned.
  HeapVector<Member<NGFloatingObject>>& UnpositionedFloats() {
    return unpositioned_floats_;
  }

  DECLARE_VIRTUAL_TRACE();

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

  LayoutObject* layout_object_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  NGDeprecatedMarginStrut margin_strut_;

  HeapVector<Member<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;

  WeakBoxList out_of_flow_descendant_candidates_;
  Vector<OutOfFlowPlacement> out_of_flow_candidate_placements_;

  WeakBoxList out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;

  // Floats that need to be positioned by the next in-flow fragment that can
  // determine its block position in space.
  HeapVector<Member<NGFloatingObject>> unpositioned_floats_;

  Vector<NGLogicalOffset> floating_object_offsets_;
  HeapVector<Member<NGFloatingObject>> positioned_floats_;

  Member<NGBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
