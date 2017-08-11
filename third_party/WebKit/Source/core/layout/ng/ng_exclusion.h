// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGExclusion_h
#define NGExclusion_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_rect.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Struct that represents NG exclusion.
struct CORE_EXPORT NGExclusion {
  // Type of NG exclusion.
  enum Type {
    // Undefined exclusion type.
    // At this moment it's also used to represent CSS3 exclusion.
    kExclusionTypeUndefined = 0,
    // Exclusion that is created by LEFT float.
    kFloatLeft = 1,
    // Exclusion that is created by RIGHT float.
    kFloatRight = 2
  };

  // Rectangle in logical coordinates the represents this exclusion.
  NGLogicalRect rect;

  // Type of this exclusion.
  Type type = kExclusionTypeUndefined;

  bool operator==(const NGExclusion& other) const;
  bool operator!=(const NGExclusion& other) const { return !(*this == other); }

  String ToString() const;

  // Tries to combine the current exclusion with {@code other} exclusion and
  // returns true if successful.
  // We can combine 2 exclusions if they
  // - adjoining to each other and have the same exclusion type
  // - {@code other} exclusion shadows the current one.
  //   That's because it's not allowed to position anything in the shadowed
  //   area.
  //
  //   Example:
  //   <div id="SS" style="float: left; height: 10px; width: 10px"></div>
  //   <div id="BB" style="float: left; height: 20px; width: 20px"></div>
  //   +----------------+
  //   |SSBB
  //   |**BB
  //   We combine SS and BB exclusions including the shadowed area (**).
  bool MaybeCombineWith(const NGExclusion& other);
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGExclusion&);

struct CORE_EXPORT NGExclusions {
  Vector<NGExclusion> storage;

  // This member is used for implementing the "top edge alignment rule" for
  // floats. Floats can be positioned at negative offsets, hence is initialized
  // the minimum value.
  LayoutUnit last_float_block_start = LayoutUnit::Min();

  // These members are used for keeping track of the "lowest" offset for each
  // type of float. This is used for implementing float clearance.
  Optional<LayoutUnit> float_left_clear_offset;
  Optional<LayoutUnit> float_right_clear_offset;

  bool operator==(const NGExclusions& other) const;
  bool operator!=(const NGExclusions& other) const { return !(*this == other); }

  void Add(const NGExclusion& exclusion);
};

}  // namespace blink

#endif  // NGExclusion_h
