// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutResult_h
#define NGLayoutResult_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_static_position.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult : public RefCounted<NGLayoutResult> {
 public:
  RefPtr<NGPhysicalFragment> PhysicalFragment() const {
    return physical_fragment_;
  }

  const HeapLinkedHashSet<WeakMember<NGBlockNode>>& OutOfFlowDescendants()
      const {
    return out_of_flow_descendants_;
  }

  const Vector<NGStaticPosition>& OutOfFlowPositions() const {
    return out_of_flow_positions_;
  }

  // List of floats that need to be positioned by the next in-flow child that
  // can determine its position in space.
  // Use case example where it may be needed:
  //    <div><float></div>
  //    <div style="margin-top: 10px; height: 20px"></div>
  // The float cannot be positioned right away inside of the 1st div because
  // the vertical position is not known at that moment. It will be known only
  // after the 2nd div collapses its margin with its parent.
  const Vector<RefPtr<NGUnpositionedFloat>>& UnpositionedFloats() const {
    return unpositioned_floats_;
  }

 private:
  friend class NGFragmentBuilder;

  NGLayoutResult(PassRefPtr<NGPhysicalFragment> physical_fragment,
                 PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>>&
                     out_of_flow_descendants,
                 Vector<NGStaticPosition> out_of_flow_positions,
                 Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats);

  RefPtr<NGPhysicalFragment> physical_fragment_;
  PersistentHeapLinkedHashSet<WeakMember<NGBlockNode>> out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;
  Vector<RefPtr<NGUnpositionedFloat>> unpositioned_floats_;
};

}  // namespace blink

#endif  // NGLayoutResult_h
