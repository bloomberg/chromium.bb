// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"

namespace blink {

NGPhysicalFragment::NGPhysicalFragment(
    LayoutObject* layout_object,
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    NGFragmentType type,
    HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
    Vector<NGStaticPosition> out_of_flow_positions,
    HeapVector<Member<NGFloatingObject>>& unpositioned_floats,
    HeapVector<Member<NGFloatingObject>>& positioned_floats,
    NGBreakToken* break_token)
    : layout_object_(layout_object),
      size_(size),
      overflow_(overflow),
      break_token_(break_token),
      type_(type),
      is_placed_(false) {
  out_of_flow_descendants_.swap(out_of_flow_descendants);
  out_of_flow_positions_.swap(out_of_flow_positions);
  unpositioned_floats_.swap(unpositioned_floats);
  positioned_floats_.swap(positioned_floats);
}

DEFINE_TRACE(NGPhysicalFragment) {
  if (Type() == kFragmentText)
    static_cast<NGPhysicalTextFragment*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGPhysicalBoxFragment*>(this)->traceAfterDispatch(visitor);
}

void NGPhysicalFragment::finalizeGarbageCollectedObject() {
  if (Type() == kFragmentText)
    static_cast<NGPhysicalTextFragment*>(this)->~NGPhysicalTextFragment();
  else
    static_cast<NGPhysicalBoxFragment*>(this)->~NGPhysicalBoxFragment();
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalFragment) {
  visitor->trace(out_of_flow_descendants_);
  visitor->trace(break_token_);
  visitor->trace(unpositioned_floats_);
  visitor->trace(positioned_floats_);
}

}  // namespace blink
