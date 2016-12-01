// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGPhysicalFragment::NGPhysicalFragment(
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    HeapVector<Member<const NGPhysicalFragmentBase>>& children,
    HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
    Vector<NGLogicalOffset> out_of_flow_offsets,
    NGMarginStrut margin_strut)
    : NGPhysicalFragmentBase(size, overflow, kFragmentBox),
      margin_strut_(margin_strut) {
  children_.swap(children);
  out_of_flow_descendants_.swap(out_of_flow_descendants);
  out_of_flow_offsets_.swap(out_of_flow_offsets);
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalFragment) {
  visitor->trace(children_);
  visitor->trace(out_of_flow_descendants_);
  NGPhysicalFragmentBase::traceAfterDispatch(visitor);
}

}  // namespace blink
