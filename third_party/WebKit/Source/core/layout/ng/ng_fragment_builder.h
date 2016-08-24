// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class CORE_EXPORT NGFragmentBuilder final {
 public:
  NGFragmentBuilder(NGFragmentBase::NGFragmentType);

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(NGDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(const NGFragment*);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGFragment* ToFragment();

 private:
  NGFragmentBase::NGFragmentType type_;
  NGWritingMode writing_mode_;
  NGDirection direction_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  PersistentHeapVector<Member<const NGFragmentBase>> children_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
