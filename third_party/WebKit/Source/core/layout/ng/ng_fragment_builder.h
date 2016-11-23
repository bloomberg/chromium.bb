// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class CORE_EXPORT NGFragmentBuilder final
    : public GarbageCollectedFinalized<NGFragmentBuilder> {
 public:
  NGFragmentBuilder(NGPhysicalFragmentBase::NGFragmentType);

  using WeakBoxList = HeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(TextDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(NGFragmentBase*, NGLogicalOffset);

  NGFragmentBuilder& SetOutOfFlowDescendants(WeakBoxList&,
                                             Vector<NGLogicalOffset>&);

  // Sets MarginStrut for the resultant fragment.
  NGFragmentBuilder& SetMarginStrutBlockStart(const NGMarginStrut& from);
  NGFragmentBuilder& SetMarginStrutBlockEnd(const NGMarginStrut& from);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGPhysicalFragment* ToFragment();

  DECLARE_VIRTUAL_TRACE();

 private:
  NGPhysicalFragmentBase::NGFragmentType type_;
  NGWritingMode writing_mode_;
  TextDirection direction_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  NGMarginStrut margin_strut_;

  HeapVector<Member<NGPhysicalFragmentBase>> children_;
  Vector<NGLogicalOffset> offsets_;
  WeakBoxList out_of_flow_descendants_;
  Vector<NGLogicalOffset> out_of_flow_offsets_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
