// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    LayoutObject* layout_object,
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>&
        out_of_flow_descendants,
    Vector<NGStaticPosition>& out_of_flow_positions,
    Vector<Persistent<NGFloatingObject>>& unpositioned_floats,
    Vector<Persistent<NGFloatingObject>>& positioned_floats,
    const WTF::Optional<NGLogicalOffset>& bfc_offset,
    const NGMarginStrut& end_margin_strut,
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
      bfc_offset_(bfc_offset),
      end_margin_strut_(end_margin_strut) {
  children_.swap(children);
}

}  // namespace blink
