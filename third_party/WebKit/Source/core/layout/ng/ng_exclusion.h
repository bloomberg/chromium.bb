// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGExclusion_h
#define NGExclusion_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_rect.h"
#include "wtf/Vector.h"

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
  Type type;

  bool operator==(const NGExclusion& other) const;
  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGExclusion&);

struct CORE_EXPORT NGExclusions {
  NGExclusions();
  NGExclusions(const NGExclusions& other);

  Vector<std::unique_ptr<const NGExclusion>> storage;

  // Last left/right float exclusions are used to enforce the top edge alignment
  // rule for floats and for the support of CSS "clear" property.
  const NGExclusion* last_left_float;   // Owned by storage.
  const NGExclusion* last_right_float;  // Owned by storage.

  NGExclusions& operator=(const NGExclusions& other);

  void Add(const NGExclusion& exclusion);
};

}  // namespace blink

#endif  // NGExclusion_h
