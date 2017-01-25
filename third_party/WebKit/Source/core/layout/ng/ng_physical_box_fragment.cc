// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    LayoutObject* layout_object,
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    HeapVector<Member<NGPhysicalFragment>>& children,
    HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
    Vector<NGStaticPosition>& out_of_flow_positions,
    NGDeprecatedMarginStrut margin_strut,
    HeapVector<Member<NGFloatingObject>>& unpositioned_floats,
    HeapVector<Member<NGFloatingObject>>& positioned_floats,
    NGBreakToken* break_token)
    : NGPhysicalFragment(layout_object,
                         size,
                         overflow,
                         kFragmentBox,
                         out_of_flow_descendants,
                         out_of_flow_positions,
                         unpositioned_floats,
                         positioned_floats,
                         break_token),
      margin_strut_(margin_strut) {
  children_.swap(children);
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalBoxFragment) {
  visitor->trace(children_);
  NGPhysicalFragment::traceAfterDispatch(visitor);
}

}  // namespace blink
