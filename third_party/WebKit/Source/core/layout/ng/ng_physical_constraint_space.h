// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalConstraintSpace_h
#define NGPhysicalConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

enum NGExclusionType {
  NGClearNone = 0,
  NGClearFloatLeft = 1,
  NGClearFloatRight = 2,
  NGClearFragment = 4
};

enum NGFragmentationType {
  FragmentNone,
  FragmentPage,
  FragmentColumn,
  FragmentRegion
};

struct NGExclusion : public GarbageCollected<NGExclusion> {
  NGExclusion(LayoutUnit top,
              LayoutUnit right,
              LayoutUnit bottom,
              LayoutUnit left) {
    rect.offset.left = left;
    rect.offset.top = top;
    rect.size.width = right - left;
    rect.size.height = bottom - top;
  }
  LayoutUnit Top() const { return rect.offset.top; }
  LayoutUnit Right() const { return rect.size.width + rect.offset.left; }
  LayoutUnit Bottom() const { return rect.size.height + rect.offset.top; }
  LayoutUnit Left() const { return rect.offset.left; }

  String ToString() const {
    return String::format("%s,%s %sx%s",
                          rect.offset.left.toString().ascii().data(),
                          rect.offset.top.toString().ascii().data(),
                          rect.size.width.toString().ascii().data(),
                          rect.size.height.toString().ascii().data());
  }
  NGPhysicalRect rect;

  DEFINE_INLINE_TRACE() {}
};

// The NGPhysicalConstraintSpace contains the underlying data for the
// NGConstraintSpace. It is not meant to be used directly as all members are in
// the physical coordinate space. Instead NGConstraintSpace should be used.
class CORE_EXPORT NGPhysicalConstraintSpace final
    : public GarbageCollectedFinalized<NGPhysicalConstraintSpace> {
 public:
  NGPhysicalConstraintSpace(
      NGPhysicalSize container_size,
      bool fixed_width,
      bool fixed_height,
      bool width_direction_triggers_scrollbar,
      bool height_direction_triggers_scrollbar,
      NGFragmentationType width_direction_fragmentation_type,
      NGFragmentationType height_direction_fragmentation_type,
      bool is_new_fc);

  NGPhysicalConstraintSpace();
  NGPhysicalConstraintSpace(NGPhysicalSize);

  NGPhysicalSize ContainerSize() const { return container_size_; }

  void AddExclusion(const NGExclusion*, unsigned options = 0);
  const HeapVector<Member<const NGExclusion>>& Exclusions(
      unsigned options = 0) const;

  DEFINE_INLINE_TRACE() { visitor->trace(exclusions_); }

 private:
  friend class NGConstraintSpace;

  NGPhysicalSize container_size_;

  unsigned fixed_width_ : 1;
  unsigned fixed_height_ : 1;
  unsigned width_direction_triggers_scrollbar_ : 1;
  unsigned height_direction_triggers_scrollbar_ : 1;
  unsigned width_direction_fragmentation_type_ : 2;
  unsigned height_direction_fragmentation_type_ : 2;

  // Whether the current constraint space is for the newly established
  // formatting Context
  unsigned is_new_fc_ : 1;

  HeapVector<Member<const NGExclusion>> exclusions_;
};

}  // namespace blink

#endif  // NGPhysicalConstraintSpace_h
