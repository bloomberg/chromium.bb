// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGExclusionSpace_h
#define NGExclusionSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_rect.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Vector.h"

namespace blink {

typedef NGLogicalRect NGLayoutOpportunity;

// The exclusion space represents all of the exclusions within a block
// formatting context.
//
// The space is mutated simply by adding exclusions, and various information
// can be queried based on the exclusions.
class CORE_EXPORT NGExclusionSpace {
 public:
  NGExclusionSpace();

  void Add(const NGExclusion& exclusion);

  // Returns a layout opportunity, within the BFC, starting at the given offset,
  // with a size greater than {@code minimum_size}.
  NGLayoutOpportunity FindLayoutOpportunity(
      const NGLogicalOffset& offset,
      const NGLogicalSize& available_size,
      const NGLogicalSize& minimum_size) const;

  // Returns the clearance offset based on the provided {@code clear_type}.
  LayoutUnit ClearanceOffset(EClear clear_type) const;

  // Returns the block start offset of the last float added.
  LayoutUnit LastFloatBlockStart() const { return last_float_block_start_; }

  bool HasLeftFloat() const { return has_left_float_; }
  bool HasRightFloat() const { return has_right_float_; }

  bool operator==(const NGExclusionSpace& other) const;
  bool operator!=(const NGExclusionSpace& other) const {
    return !(*this == other);
  }

 private:
  friend class NGLayoutOpportunityIterator;
  Vector<NGExclusion> storage_;

  // This member is used for implementing the "top edge alignment rule" for
  // floats. Floats can be positioned at negative offsets, hence is initialized
  // the minimum value.
  LayoutUnit last_float_block_start_;

  // These members are used for keeping track of the "lowest" offset for each
  // type of float. This is used for implementing float clearance.
  LayoutUnit left_float_clear_offset_;
  LayoutUnit right_float_clear_offset_;

  unsigned has_left_float_ : 1;
  unsigned has_right_float_ : 1;
};

}  // namespace blink

#endif  // NGExclusionSpace_h
