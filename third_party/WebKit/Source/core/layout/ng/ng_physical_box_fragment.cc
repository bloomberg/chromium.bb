// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    HeapVector<Member<const NGPhysicalFragment>>& children,
    HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
    Vector<NGStaticPosition>& out_of_flow_positions,
    NGMarginStrut margin_strut)
    : NGPhysicalFragment(size,
                         overflow,
                         kFragmentBox,
                         out_of_flow_descendants,
                         out_of_flow_positions),
      margin_strut_(margin_strut) {
  children_.swap(children);
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalBoxFragment) {
  visitor->trace(children_);
  NGPhysicalFragment::traceAfterDispatch(visitor);
}

}  // namespace blink
