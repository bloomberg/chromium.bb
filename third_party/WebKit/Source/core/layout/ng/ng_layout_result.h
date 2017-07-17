// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutResult_h
#define NGLayoutResult_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_static_position.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

struct NGUnpositionedFloat;

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult : public RefCounted<NGLayoutResult> {
 public:
  enum NGLayoutResultStatus {
    kSuccess = 0,
    kBfcOffsetResolved = 1,
    // When adding new values, make sure the bit size of |status_| is large
    // enough to store.
  };

  RefPtr<NGPhysicalFragment> PhysicalFragment() const {
    return physical_fragment_;
  }

  RefPtr<NGPhysicalFragment>& MutablePhysicalFragment() {
    return physical_fragment_;
  }

  const Vector<NGOutOfFlowPositionedDescendant> OutOfFlowPositionedDescendants()
      const {
    return oof_positioned_descendants_;
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

  NGLayoutResultStatus Status() const {
    return static_cast<NGLayoutResultStatus>(status_);
  }

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const {
    return bfc_offset_;
  }

  const NGMarginStrut EndMarginStrut() const { return end_margin_strut_; }

 private:
  friend class NGFragmentBuilder;

  NGLayoutResult(RefPtr<NGPhysicalFragment> physical_fragment,
                 Vector<NGOutOfFlowPositionedDescendant>
                     out_of_flow_positioned_descendants,
                 Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
                 const WTF::Optional<NGLogicalOffset> bfc_offset,
                 const NGMarginStrut end_margin_strut,
                 NGLayoutResultStatus status);

  RefPtr<NGPhysicalFragment> physical_fragment_;
  Vector<RefPtr<NGUnpositionedFloat>> unpositioned_floats_;

  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants_;
  const WTF::Optional<NGLogicalOffset> bfc_offset_;
  const NGMarginStrut end_margin_strut_;

  unsigned status_ : 1;
};

}  // namespace blink

#endif  // NGLayoutResult_h
