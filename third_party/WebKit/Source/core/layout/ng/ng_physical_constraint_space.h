// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalConstraintSpace_h
#define NGPhysicalConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/DoublyLinkedList.h"

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

enum NGDirection { LeftToRight = 0, RightToLeft = 1 };

class NGExclusion {
 public:
  NGExclusion();
  ~NGExclusion() {}
};

// The NGPhysicalConstraintSpace contains the underlying data for the
// NGConstraintSpace. It is not meant to be used directly as all members are in
// the physical coordinate space. Instead NGConstraintSpace should be used.
class CORE_EXPORT NGPhysicalConstraintSpace final
    : public GarbageCollected<NGPhysicalConstraintSpace> {
 public:
  NGPhysicalConstraintSpace();
  NGPhysicalConstraintSpace(const NGPhysicalConstraintSpace&);

  void AddExclusion(const NGExclusion, unsigned options = 0);
  DoublyLinkedList<const NGExclusion> Exclusions(unsigned options = 0) const;

  DEFINE_INLINE_TRACE() {}

 private:
  friend class NGConstraintSpace;

  NGPhysicalSize container_size_;

  unsigned fixed_width_ : 1;
  unsigned fixed_height_ : 1;
  unsigned width_direction_triggers_scrollbar_ : 1;
  unsigned height_direction_triggers_scrollbar_ : 1;
  unsigned width_direction_fragmentation_type_ : 2;
  unsigned height_direction_fragmentation_type_ : 2;

  DoublyLinkedList<const NGExclusion> exclusions_;
};

}  // namespace blink

#endif  // NGPhysicalConstraintSpace_h
