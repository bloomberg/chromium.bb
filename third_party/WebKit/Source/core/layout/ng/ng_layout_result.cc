// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_result.h"

#include "core/layout/ng/ng_floating_object.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    PassRefPtr<NGPhysicalFragment> physical_fragment,
    PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>&
        out_of_flow_descendants,
    Vector<NGStaticPosition> out_of_flow_positions,
    Vector<Persistent<NGFloatingObject>>& unpositioned_floats)
    : physical_fragment_(physical_fragment),
      out_of_flow_descendants_(out_of_flow_descendants),
      out_of_flow_positions_(out_of_flow_positions),
      unpositioned_floats_(unpositioned_floats) {}

}  // namespace blink
