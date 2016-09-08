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

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(NGDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(NGFragment*, NGLogicalOffset);

  // Sets MarginStrut for the resultant fragment.
  // These 2 methods below should be only called once as we update the
  // fragment's MarginStrut block start/end from first/last children MarginStrut
  NGFragmentBuilder& SetMarginStrutBlockStart(const NGMarginStrut& from);
  NGFragmentBuilder& SetMarginStrutBlockEnd(const NGMarginStrut& from);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGPhysicalFragment* ToFragment();

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(children_); }

 private:
  NGPhysicalFragmentBase::NGFragmentType type_;
  NGWritingMode writing_mode_;
  NGDirection direction_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  NGMarginStrut margin_strut_;

  HeapVector<Member<NGPhysicalFragmentBase>> children_;
  Vector<NGLogicalOffset> offsets_;

  // Whether MarginStrut block start/end was updated.
  // It's used for DCHECK safety check.
  bool is_margin_strut_block_start_updated_ : 1;
  bool is_margin_strut_block_end_updated_ : 1;
};

}  // namespace blink

#endif  // NGFragmentBuilder
