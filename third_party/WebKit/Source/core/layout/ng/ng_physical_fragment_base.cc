// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment_base.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"

namespace blink {

NGPhysicalFragmentBase::NGPhysicalFragmentBase(
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    NGFragmentType type,
    HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
    Vector<NGStaticPosition> out_of_flow_positions,
    NGBreakToken* break_token)
    : size_(size),
      overflow_(overflow),
      break_token_(break_token),
      type_(type),
      has_been_placed_(false) {
  out_of_flow_descendants_.swap(out_of_flow_descendants);
  out_of_flow_positions_.swap(out_of_flow_positions);
}

DEFINE_TRACE(NGPhysicalFragmentBase) {
  if (Type() == kFragmentText)
    static_cast<NGPhysicalTextFragment*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGPhysicalFragment*>(this)->traceAfterDispatch(visitor);
}

void NGPhysicalFragmentBase::finalizeGarbageCollectedObject() {
  if (Type() == kFragmentText)
    static_cast<NGPhysicalTextFragment*>(this)->~NGPhysicalTextFragment();
  else
    static_cast<NGPhysicalFragment*>(this)->~NGPhysicalFragment();
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalFragmentBase) {
  visitor->trace(out_of_flow_descendants_);
  visitor->trace(break_token_);
}

}  // namespace blink
